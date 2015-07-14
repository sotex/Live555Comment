/**********
This library is free software; you can redistribute it and/or modify it under
the terms of the GNU Lesser General Public License as published by the
Free Software Foundation; either version 2.1 of the License, or (at your
option) any later version. (See <http://www.gnu.org/copyleft/lesser.html>.)

This library is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
more details.

You should have received a copy of the GNU Lesser General Public License
along with this library; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
**********/
// Copyright (c) 1996-2012 Live Networks, Inc.  All rights reserved.
// 'Group sockets'
// Implementation

#include "Groupsock.hh"
#include "GroupsockHelper.hh"
//##### Eventually fix the following #include; we shouldn't know about tunnels
#include "TunnelEncaps.hh"

#ifndef NO_SSTREAM
#include <sstream>
#endif
#include <stdio.h>

///////// OutputSocket //////////

OutputSocket::OutputSocket(UsageEnvironment& env)
: Socket(env, 0 /* let kernel choose port 让内核选择端口*/),
fSourcePort(0), fLastSentTTL(0)
{
}

OutputSocket::OutputSocket(UsageEnvironment& env, Port port)
: Socket(env, port),
fSourcePort(0), fLastSentTTL(0)
{
}

OutputSocket::~OutputSocket()
{
}

// 将buffer中的bufferSize字节数据发送到主机(address+port),TTL =ttl
Boolean OutputSocket::write(netAddressBits address, Port port, u_int8_t ttl,
	unsigned char* buffer, unsigned bufferSize)
{
	if (ttl == fLastSentTTL) {
		// Optimization: So we don't do a 'set TTL' system call again
		// 优化：所以我们不需要再这样做了，“设置TTL'是一个系统调用
		ttl = 0;
	}
	else {
		fLastSentTTL = ttl;
	}
	struct in_addr destAddr; destAddr.s_addr = address;

	if (!writeSocket(env(), socketNum(), destAddr, port, ttl,
		buffer, bufferSize))
		return False;	//失败时返回

	// 源端口号为0
	if (sourcePortNum() == 0) {
		// Now that we've sent a packet, we can find out what the
		// kernel chose as our ephemeral source port number:
		// 现在我们已经发送一个包，我们可以获知
		// 内核为选择我们选择的临时源端口号：
		if (!getSourcePort(env(), socketNum(), fSourcePort)) {
			if (DebugLevel >= 1)
				env() << *this
				<< ": failed to get source port: "
				<< env().getResultMsg() << "\n";
			return False;
		}
	}

	return True;
}

// By default, we don't do reads:
Boolean OutputSocket
::handleRead(unsigned char* /*buffer*/, unsigned /*bufferMaxSize*/,
unsigned& /*bytesRead*/, struct sockaddr_in& /*fromAddress*/)
{
	return True;
}


///////// destRecord //////////

destRecord
::destRecord(struct in_addr const& addr, Port const& port, u_int8_t ttl,
destRecord* next)
: fNext(next), fGroupEId(addr, port.num(), ttl), fPort(port)
{
}

destRecord::~destRecord()
{
	delete fNext;
}


///////// Groupsock //////////

NetInterfaceTrafficStats Groupsock::statsIncoming;
NetInterfaceTrafficStats Groupsock::statsOutgoing;
NetInterfaceTrafficStats Groupsock::statsRelayedIncoming;
NetInterfaceTrafficStats Groupsock::statsRelayedOutgoing;

// Constructor for a source-independent multicast group
Groupsock::Groupsock(UsageEnvironment& env, struct in_addr const& groupAddr,
	Port port, u_int8_t ttl)
	: OutputSocket(env, port),
	deleteIfNoMembers(False), isSlave(False),
	fIncomingGroupEId(groupAddr, port.num(), ttl), fDests(NULL), fTTL(ttl)
{
	addDestination(groupAddr, port);	//添加目标到链表
	// 加入任意源多播组
	if (!socketJoinGroup(env, socketNum(), groupAddr.s_addr)) {
		if (DebugLevel >= 1) {
			env << *this << ": failed to join group: "
				<< env.getResultMsg() << "\n";
		}
	}

	// Make sure we can get our source address:
	// 确保我们可以得到我们的源地址：
	if (ourIPAddress(env) == 0) {
		if (DebugLevel >= 0) { // this is a fatal error
			env << "Unable to determine our source address: "
				<< env.getResultMsg() << "\n";
		}
	}

	if (DebugLevel >= 2) env << *this << ": created\n";
}

// Constructor for a source-specific multicast group
// 构造函数用于一个特定源的组播组
Groupsock::Groupsock(UsageEnvironment& env, struct in_addr const& groupAddr,
struct in_addr const& sourceFilterAddr,
	Port port)
	: OutputSocket(env, port),
	deleteIfNoMembers(False), isSlave(False),
	fIncomingGroupEId(groupAddr, sourceFilterAddr, port.num()),
	fDests(NULL), fTTL(255)
{
	addDestination(groupAddr, port);

	// First try a SSM join.  If that fails, try a regular join:
	// 先尝试加入一个SSM，如果失败了，尝试常规连接
	if (!socketJoinGroupSSM(env, socketNum(), groupAddr.s_addr,
		sourceFilterAddr.s_addr)) {
		if (DebugLevel >= 3) {
			env << *this << ": SSM join failed: "
				<< env.getResultMsg();
			env << " - trying regular join instead\n";
		}
		// 尝试加入一个任意源多播组
		if (!socketJoinGroup(env, socketNum(), groupAddr.s_addr)) {
			if (DebugLevel >= 1) {
				env << *this << ": failed to join group: "
					<< env.getResultMsg() << "\n";
			}
		}
	}

	if (DebugLevel >= 2) env << *this << ": created\n";
}

Groupsock::~Groupsock()
{
	if (isSSM()) {
		// 离开特定源组播
		if (!socketLeaveGroupSSM(env(), socketNum(), groupAddress().s_addr,
			sourceFilterAddress().s_addr)) {
			socketLeaveGroup(env(), socketNum(), groupAddress().s_addr);
		}
	}
	else {
		// 离开任意源组播
		socketLeaveGroup(env(), socketNum(), groupAddress().s_addr);
	}

	delete fDests;	//释放目标链表

	if (DebugLevel >= 2) env() << *this << ": deleting\n";
}

void
Groupsock::changeDestinationParameters(struct in_addr const& newDestAddr,
Port newDestPort, int newDestTTL)
{
	if (fDests == NULL) return;

	struct in_addr destAddr = fDests->fGroupEId.groupAddress();
	if (newDestAddr.s_addr != 0) {
		if (newDestAddr.s_addr != destAddr.s_addr
			&& IsMulticastAddress(newDestAddr.s_addr)) {
			// If the new destination is a multicast address, then we assume that
			// we want to join it also.  (If this is not in fact the case, then
			// call "multicastSendOnly()" afterwards.)
			// 如果新的目标是多播地址，那么我们假定我们希望加入它。
			// （否则在实际的情况下，是调用“multicastSendOnly()”之后。）

			// 离开旧的多播组
			socketLeaveGroup(env(), socketNum(), destAddr.s_addr);
			// 加入新的多播组
			socketJoinGroup(env(), socketNum(), newDestAddr.s_addr);
		}
		destAddr.s_addr = newDestAddr.s_addr;
	}

	portNumBits destPortNum = fDests->fGroupEId.portNum();
	if (newDestPort.num() != 0) {
		if (newDestPort.num() != destPortNum
			&& IsMulticastAddress(destAddr.s_addr)) {
			// Also bind to the new port number:
			// 也是绑定到新的端口号
			changePort(newDestPort);
			// And rejoin the multicast group:
			// 并重新加入组播组：
			socketJoinGroup(env(), socketNum(), destAddr.s_addr);
		}
		destPortNum = newDestPort.num();
		fDests->fPort = newDestPort;
	}

	u_int8_t destTTL = ttl();
	if (newDestTTL != ~0) destTTL = (u_int8_t)newDestTTL;

	fDests->fGroupEId = GroupEId(destAddr, destPortNum, destTTL);
}

void Groupsock::addDestination(struct in_addr const& addr, Port const& port)
{
	// Check whether this destination is already known:
	// 检查此目标是否为已知的：
	for (destRecord* dests = fDests; dests != NULL; dests = dests->fNext) {
		if (addr.s_addr == dests->fGroupEId.groupAddress().s_addr
			&& port.num() == dests->fPort.num()) {
			return;
		}
	}
	// 头插入到链表
	fDests = new destRecord(addr, port, ttl(), fDests);
}

void Groupsock::removeDestination(struct in_addr const& addr, Port const& port)
{
	for (destRecord** destsPtr = &fDests; *destsPtr != NULL;
		destsPtr = &((*destsPtr)->fNext)) {
		if (addr.s_addr == (*destsPtr)->fGroupEId.groupAddress().s_addr
			&& port.num() == (*destsPtr)->fPort.num()) {
			// Remove the record pointed to by *destsPtr :
			// 删除该* destsPtr指向的记录
			destRecord* next = (*destsPtr)->fNext;
			(*destsPtr)->fNext = NULL;
			delete (*destsPtr);
			*destsPtr = next;
			return;
		}
	}
}

void Groupsock::removeAllDestinations()
{
	delete fDests; fDests = NULL;
}

void Groupsock::multicastSendOnly()
{
	// We disable this code for now, because - on some systems - leaving the multicast group seems to cause sent packets
	// to not be received by other applications (at least, on the same host).
	// 我们禁用此代码现在，因为 - 在某些系统 - 离开多播组似乎会导致发送的数据包不被其他的应用程序接收（至少是在同一主机上）。
#if 0
	// 离开多播组
	socketLeaveGroup(env(), socketNum(), fIncomingGroupEId.groupAddress().s_addr);
	for (destRecord* dests = fDests; dests != NULL; dests = dests->fNext) {
		socketLeaveGroup(env(), socketNum(), dests->fGroupEId.groupAddress().s_addr);
	}
#endif
}

Boolean Groupsock::output(UsageEnvironment& env, u_int8_t ttlToSend,
	unsigned char* buffer, unsigned bufferSize,
	DirectedNetInterface* interfaceNotToFwdBackTo/*不转发接口*/)
{
	do {
		// First, do the datagram send, to each destination:
		// 首先，发送数据报到每一个目标
		Boolean writeSuccess = True;
		for (destRecord* dests = fDests; dests != NULL; dests = dests->fNext)
		{
			if (!write(dests->fGroupEId.groupAddress().s_addr, dests->fPort, ttlToSend,
				buffer, bufferSize)) {
				writeSuccess = False;
				break;	//有一个发送失败，就跳出
			}
		}
		if (!writeSuccess) break;	//发送失败，直接跳出
		// 统计流量
		statsOutgoing.countPacket(bufferSize);
		statsGroupOutgoing.countPacket(bufferSize);

		// Then, forward to our members:然后，转发至我们的成员：
		int numMembers = 0;
		if (!members().IsEmpty()) {
			numMembers =
				outputToAllMembersExcept(interfaceNotToFwdBackTo,
				ttlToSend, buffer, bufferSize,
				ourIPAddress(env)/*自身的地址*/);
			if (numMembers < 0) break;
		}

		if (DebugLevel >= 3) {
			env << *this << ": wrote " << bufferSize << " bytes, ttl "
				<< (unsigned)ttlToSend;
			if (numMembers > 0) {
				env << "; relayed to " << numMembers << " members";
			}
			env << "\n";
		}
		return True;
	} while (0);

	if (DebugLevel >= 0) { // this is a fatal error
		env.setResultMsg("Groupsock write failed: ", env.getResultMsg());
	}
	return False;
}

Boolean Groupsock::handleRead(unsigned char* buffer, unsigned bufferMaxSize,
	unsigned& bytesRead,
struct sockaddr_in& fromAddress)
{
	// Read data from the socket, and relay it across any attached tunnels
	// 读取来自套接字的数据，并转发给它连接的任何一个隧道
	//##### later make this code more general - independent of tunnels
	//##### 后来又使这段代码更通用 - 无关的隧道

	bytesRead = 0;

	int maxBytesToRead = bufferMaxSize - TunnelEncapsulationTrailerMaxSize;
	// 从socket读取数据
	int numBytes = readSocket(env(), socketNum(),
		buffer, maxBytesToRead, fromAddress);
	if (numBytes < 0) {
		if (DebugLevel >= 0) { // this is a fatal error
			env().setResultMsg("Groupsock read failed: ",
				env().getResultMsg());
		}
		return False;
	}

	// If we're a SSM group, make sure the source address matches:
	// 如果我们是一个SSM组，确保源地址匹配：
	if (isSSM()
		&& fromAddress.sin_addr.s_addr != sourceFilterAddress().s_addr) {
		return True;
	}

	// We'll handle this data.
	// Also write it (with the encapsulation trailer) to each member,
	// unless the packet was originally sent by us to begin with.
	// 我们会处理这些数据。
	// 也是把它发送到（encapsulation trailer）每个成员
	// 除非该数据包最初是由我们发出开始。
	bytesRead = numBytes;

	int numMembers = 0;
	if (!wasLoopedBackFromUs(env(), fromAddress)) {
		//不是本地环回，统计流量
		statsIncoming.countPacket(numBytes);
		statsGroupIncoming.countPacket(numBytes);
		numMembers =	//发送到每一个成员
			outputToAllMembersExcept(NULL, ttl(),
			buffer, bytesRead,
			fromAddress.sin_addr.s_addr);
		if (numMembers > 0) {
			// 统计中继流量
			statsRelayedIncoming.countPacket(numBytes);
			statsGroupRelayedIncoming.countPacket(numBytes);
		}
	}
	if (DebugLevel >= 3) {
		env() << *this << ": read " << bytesRead << " bytes from " << AddressString(fromAddress).val();
		if (numMembers > 0) {
			env() << "; relayed to " << numMembers << " members";
		}
		env() << "\n";
	}

	return True;
}

Boolean Groupsock::wasLoopedBackFromUs(UsageEnvironment& env,
struct sockaddr_in& fromAddress)
{
	if (fromAddress.sin_addr.s_addr
		== ourIPAddress(env)) {
		if (fromAddress.sin_port == sourcePortNum()) {
#ifdef DEBUG_LOOPBACK_CHECKING
			if (DebugLevel >= 3) {
				env() << *this << ": got looped-back packet\n";
			}
#endif
			return True;
		}
	}

	return False;
}

int Groupsock::outputToAllMembersExcept(DirectedNetInterface* exceptInterface,
	u_int8_t ttlToFwd,
	unsigned char* data, unsigned size,
	netAddressBits sourceAddr)
{
	// Don't forward TTL-0 packets无需转发TTL为0包
	if (ttlToFwd == 0) return 0;

	DirectedNetInterfaceSet::Iterator iter(members());
	unsigned numMembers = 0;
	DirectedNetInterface* interf;
	while ((interf = iter.next()) != NULL) {
		// Check whether we've asked to exclude this interface:
		// 检查我们是否已经要求排除这个接口：
		if (interf == exceptInterface)
			continue;

		// Check that the packet's source address makes it OK to
		// be relayed across this interface:
		// 检查数据包的源地址，使得它确定跨过该接口被转发：
		UsageEnvironment& saveEnv = env();
		// because the following call may delete "this"
		// 因为下面的调用可能会 delete this
		if (!interf->SourceAddrOKForRelaying(saveEnv, sourceAddr)) {
			if (strcmp(saveEnv.getResultMsg(), "") != 0) {
				// Treat this as a fatal error
				return -1;
			}
			else {
				continue;
			}
		}

		if (numMembers == 0) {
			// We know that we're going to forward to at least one
			// member, so fill in the tunnel encapsulation trailer.
			// 我们知道，我们将转发给至少一个成员，所以请填满该tunnel encapsulation trailer。
			// (Note: Allow for it not being 4-byte-aligned.)
			// (注：允许它不是4字节对齐的。)
			TunnelEncapsulationTrailer* trailerInPacket
				= (TunnelEncapsulationTrailer*)&data[size];
			TunnelEncapsulationTrailer* trailer;

			Boolean misaligned = ((uintptr_t)trailerInPacket & 3) != 0;
			unsigned trailerOffset;
			u_int8_t tunnelCmd;
			if (isSSM()) {
				// add an 'auxilliary address' before the trailer
				// 在 trailer前加一个“若干辅助地址”
				trailerOffset = TunnelEncapsulationTrailerAuxSize;
				tunnelCmd = TunnelDataAuxCmd;
			}
			else {
				trailerOffset = 0;
				tunnelCmd = TunnelDataCmd;
			}
			unsigned trailerSize = TunnelEncapsulationTrailerSize + trailerOffset;
			unsigned tmpTr[TunnelEncapsulationTrailerMaxSize];
			if (misaligned) {
				trailer = (TunnelEncapsulationTrailer*)&tmpTr;
			}
			else {
				trailer = trailerInPacket;
			}
			trailer += trailerOffset;

			if (fDests != NULL) {
				trailer->address() = fDests->fGroupEId.groupAddress().s_addr;
				trailer->port() = fDests->fPort; // structure copy, outputs in network order 结构复制，网络字节序输出
			}
			trailer->ttl() = ttlToFwd;
			trailer->command() = tunnelCmd;

			if (isSSM()) {
				trailer->auxAddress() = sourceFilterAddress().s_addr;
			}

			if (misaligned) {
				memmove(trailerInPacket, trailer - trailerOffset, trailerSize);
			}

			size += trailerSize;
		}

		interf->write(data, size);
		++numMembers;
	}

	return numMembers;
}

UsageEnvironment& operator<<(UsageEnvironment& s, const Groupsock& g)
{
	UsageEnvironment& s1 = s << timestampString() << " Groupsock("
		<< g.socketNum() << ": "
		<< AddressString(g.groupAddress()).val()
		<< ", " << g.port() << ", ";
	if (g.isSSM()) {
		return s1 << "SSM source: "
			<< AddressString(g.sourceFilterAddress()).val() << ")";
	}
	else {
		return s1 << (unsigned)(g.ttl()) << ")";
	}
}


////////// GroupsockLookupTable //////////


// A hash table used to index Groupsocks by socket number.
// 为env.groupsockPriv->socketTable引用(不存在时创建)通过套接字编号索引Groupsocks的哈希表。
static HashTable*& getSocketTable(UsageEnvironment& env)
{
	_groupsockPriv* priv = groupsockPriv(env);
	if (priv->socketTable == NULL) { // We need to create it
		// 创建hashTable
		priv->socketTable = HashTable::create(ONE_WORD_HASH_KEYS);
	}
	return priv->socketTable;
}

// 从groupsock->env().groupsockPriv->socketTable哈希表中查找groupsock
// 如果找到了就移除，如果哈希表中已经没有了元素，就释放哈希表
static Boolean unsetGroupsockBySocket(Groupsock const* groupsock)
{
	do {
		if (groupsock == NULL) break;

		int sock = groupsock->socketNum();
		// Make sure "sock" is in bounds:
		// 确保sock是在范围内：
		if (sock < 0) break;
		//引用哈希表
		HashTable*& sockets = getSocketTable(groupsock->env());
		//从哈希表中查找sock对应的Groupsock对象
		Groupsock* gs = (Groupsock*)sockets->Lookup((char*)(long)sock);
		if (gs == NULL || gs != groupsock) break;
		sockets->Remove((char*)(long)sock);	//找到了就移除

		if (sockets->IsEmpty()) {
			// We can also delete the table (to reclaim space):
			// 没有了元素，就释放哈希表
			delete sockets; sockets = NULL;
			reclaimGroupsockPriv(gs->env());
		}

		return True;
	} while (0);

	return False;
}

// 向groupsock->env().groupsockPriv->socketTable添加一个条目
static Boolean setGroupsockBySocket(UsageEnvironment& env, int sock,
	Groupsock* groupsock)
{
	do {
		// Make sure the "sock" parameter is in bounds:
		// 确保sock参数是在范围内
		if (sock < 0) {
			char buf[100];
			sprintf(buf, "trying to use bad socket (%d)", sock);
			env.setResultMsg(buf);
			break;
		}
		//引用groupsock->env().groupsockPriv->socketTable
		HashTable* sockets = getSocketTable(env);

		// Make sure we're not replacing an existing Groupsock (although that shouldn't happen)
		// 确保我们不会替换现有的Groupsock（尽管这不应该发生）
		Boolean alreadyExists
			= (sockets->Lookup((char*)(long)sock) != 0);
		if (alreadyExists) {	//如果哈希表中已经存在sock对应的条目
			char buf[100];
			sprintf(buf,
				"Attempting to replace an existing socket (%d",
				sock);
			env.setResultMsg(buf);
			break;
		}
		// 添加一个条目，sock为key,groupsock为value
		sockets->Add((char*)(long)sock, groupsock);
		return True;
	} while (0);

	return False;
}

// 从groupsock->env().groupsockPriv->socketTable中查找sock对应的groupsock
static Groupsock* getGroupsockBySocket(UsageEnvironment& env, int sock)
{
	do {
		// Make sure the "sock" parameter is in bounds:
		if (sock < 0) break;

		HashTable* sockets = getSocketTable(env);
		return (Groupsock*)sockets->Lookup((char*)(long)sock);
	} while (0);

	return NULL;
}

//=======================================================================
Groupsock*
GroupsockLookupTable::Fetch(UsageEnvironment& env,
netAddressBits groupAddress, Port port, u_int8_t ttl, Boolean& isNew)
{
	isNew = False;
	Groupsock* groupsock;
	do {
		groupsock = (Groupsock*)fTable.Lookup(groupAddress, (~0), port);
		if (groupsock == NULL) { // we need to create one:
			groupsock = AddNew(env, groupAddress, (~0), port, ttl);
			if (groupsock == NULL) break;
			isNew = True;
		}
	} while (0);

	return groupsock;
}

Groupsock*
GroupsockLookupTable::Fetch(UsageEnvironment& env,
netAddressBits groupAddress,
netAddressBits sourceFilterAddr, Port port,
Boolean& isNew)
{
	isNew = False;
	Groupsock* groupsock;
	do {
		groupsock
			= (Groupsock*)fTable.Lookup(groupAddress, sourceFilterAddr, port);
		if (groupsock == NULL) { // we need to create one:
			groupsock = AddNew(env, groupAddress, sourceFilterAddr, port, 0);
			if (groupsock == NULL) break;
			isNew = True;
		}
	} while (0);

	return groupsock;
}

Groupsock*
GroupsockLookupTable::Lookup(netAddressBits groupAddress, Port port)
{
	return (Groupsock*)fTable.Lookup(groupAddress, (~0), port);
}

Groupsock*
GroupsockLookupTable::Lookup(netAddressBits groupAddress,
netAddressBits sourceFilterAddr, Port port)
{
	return (Groupsock*)fTable.Lookup(groupAddress, sourceFilterAddr, port);
}

Groupsock* GroupsockLookupTable::Lookup(UsageEnvironment& env, int sock)
{
	return getGroupsockBySocket(env, sock);
}

Boolean GroupsockLookupTable::Remove(Groupsock const* groupsock)
{
	unsetGroupsockBySocket(groupsock);
	return fTable.Remove(groupsock->groupAddress().s_addr,
		groupsock->sourceFilterAddress().s_addr,
		groupsock->port());
}

Groupsock* GroupsockLookupTable::AddNew(UsageEnvironment& env,
	netAddressBits groupAddress,
	netAddressBits sourceFilterAddress,
	Port port, u_int8_t ttl)
{
	Groupsock* groupsock;
	do {
		// 构建Groupsock对象 --value
		struct in_addr groupAddr; groupAddr.s_addr = groupAddress;
		if (sourceFilterAddress == netAddressBits(~0)) {
			// regular, ISM groupsock	任意源
			groupsock = new Groupsock(env, groupAddr, port, ttl);
		}
		else {
			// SSM groupsock	特定源
			struct in_addr sourceFilterAddr;
			sourceFilterAddr.s_addr = sourceFilterAddress;
			groupsock = new Groupsock(env, groupAddr, sourceFilterAddr, port);
		}

		if (groupsock == NULL || groupsock->socketNum() < 0) break;
		// 添加到env.groupsockPriv->socketTable
		if (!setGroupsockBySocket(env, groupsock->socketNum(), groupsock)) break;
		//添加到fTable
		fTable.Add(groupAddress, sourceFilterAddress, port, (void*)groupsock);
	} while (0);

	return groupsock;
}

GroupsockLookupTable::Iterator::Iterator(GroupsockLookupTable& groupsocks)
: fIter(AddressPortLookupTable::Iterator(groupsocks.fTable))
{
}

Groupsock* GroupsockLookupTable::Iterator::next()
{
	return (Groupsock*)fIter.next();
};
