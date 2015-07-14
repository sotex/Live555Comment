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
: Socket(env, 0 /* let kernel choose port ���ں�ѡ��˿�*/),
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

// ��buffer�е�bufferSize�ֽ����ݷ��͵�����(address+port),TTL =ttl
Boolean OutputSocket::write(netAddressBits address, Port port, u_int8_t ttl,
	unsigned char* buffer, unsigned bufferSize)
{
	if (ttl == fLastSentTTL) {
		// Optimization: So we don't do a 'set TTL' system call again
		// �Ż����������ǲ���Ҫ���������ˣ�������TTL'��һ��ϵͳ����
		ttl = 0;
	}
	else {
		fLastSentTTL = ttl;
	}
	struct in_addr destAddr; destAddr.s_addr = address;

	if (!writeSocket(env(), socketNum(), destAddr, port, ttl,
		buffer, bufferSize))
		return False;	//ʧ��ʱ����

	// Դ�˿ں�Ϊ0
	if (sourcePortNum() == 0) {
		// Now that we've sent a packet, we can find out what the
		// kernel chose as our ephemeral source port number:
		// ���������Ѿ�����һ���������ǿ��Ի�֪
		// �ں�Ϊѡ������ѡ�����ʱԴ�˿ںţ�
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
	addDestination(groupAddr, port);	//���Ŀ�굽����
	// ��������Դ�ಥ��
	if (!socketJoinGroup(env, socketNum(), groupAddr.s_addr)) {
		if (DebugLevel >= 1) {
			env << *this << ": failed to join group: "
				<< env.getResultMsg() << "\n";
		}
	}

	// Make sure we can get our source address:
	// ȷ�����ǿ��Եõ����ǵ�Դ��ַ��
	if (ourIPAddress(env) == 0) {
		if (DebugLevel >= 0) { // this is a fatal error
			env << "Unable to determine our source address: "
				<< env.getResultMsg() << "\n";
		}
	}

	if (DebugLevel >= 2) env << *this << ": created\n";
}

// Constructor for a source-specific multicast group
// ���캯������һ���ض�Դ���鲥��
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
	// �ȳ��Լ���һ��SSM�����ʧ���ˣ����Գ�������
	if (!socketJoinGroupSSM(env, socketNum(), groupAddr.s_addr,
		sourceFilterAddr.s_addr)) {
		if (DebugLevel >= 3) {
			env << *this << ": SSM join failed: "
				<< env.getResultMsg();
			env << " - trying regular join instead\n";
		}
		// ���Լ���һ������Դ�ಥ��
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
		// �뿪�ض�Դ�鲥
		if (!socketLeaveGroupSSM(env(), socketNum(), groupAddress().s_addr,
			sourceFilterAddress().s_addr)) {
			socketLeaveGroup(env(), socketNum(), groupAddress().s_addr);
		}
	}
	else {
		// �뿪����Դ�鲥
		socketLeaveGroup(env(), socketNum(), groupAddress().s_addr);
	}

	delete fDests;	//�ͷ�Ŀ������

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
			// ����µ�Ŀ���Ƕಥ��ַ����ô���Ǽٶ�����ϣ����������
			// ��������ʵ�ʵ�����£��ǵ��á�multicastSendOnly()��֮�󡣣�

			// �뿪�ɵĶಥ��
			socketLeaveGroup(env(), socketNum(), destAddr.s_addr);
			// �����µĶಥ��
			socketJoinGroup(env(), socketNum(), newDestAddr.s_addr);
		}
		destAddr.s_addr = newDestAddr.s_addr;
	}

	portNumBits destPortNum = fDests->fGroupEId.portNum();
	if (newDestPort.num() != 0) {
		if (newDestPort.num() != destPortNum
			&& IsMulticastAddress(destAddr.s_addr)) {
			// Also bind to the new port number:
			// Ҳ�ǰ󶨵��µĶ˿ں�
			changePort(newDestPort);
			// And rejoin the multicast group:
			// �����¼����鲥�飺
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
	// ����Ŀ���Ƿ�Ϊ��֪�ģ�
	for (destRecord* dests = fDests; dests != NULL; dests = dests->fNext) {
		if (addr.s_addr == dests->fGroupEId.groupAddress().s_addr
			&& port.num() == dests->fPort.num()) {
			return;
		}
	}
	// ͷ���뵽����
	fDests = new destRecord(addr, port, ttl(), fDests);
}

void Groupsock::removeDestination(struct in_addr const& addr, Port const& port)
{
	for (destRecord** destsPtr = &fDests; *destsPtr != NULL;
		destsPtr = &((*destsPtr)->fNext)) {
		if (addr.s_addr == (*destsPtr)->fGroupEId.groupAddress().s_addr
			&& port.num() == (*destsPtr)->fPort.num()) {
			// Remove the record pointed to by *destsPtr :
			// ɾ����* destsPtrָ��ļ�¼
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
	// ���ǽ��ô˴������ڣ���Ϊ - ��ĳЩϵͳ - �뿪�ಥ���ƺ��ᵼ�·��͵����ݰ�����������Ӧ�ó�����գ���������ͬһ�����ϣ���
#if 0
	// �뿪�ಥ��
	socketLeaveGroup(env(), socketNum(), fIncomingGroupEId.groupAddress().s_addr);
	for (destRecord* dests = fDests; dests != NULL; dests = dests->fNext) {
		socketLeaveGroup(env(), socketNum(), dests->fGroupEId.groupAddress().s_addr);
	}
#endif
}

Boolean Groupsock::output(UsageEnvironment& env, u_int8_t ttlToSend,
	unsigned char* buffer, unsigned bufferSize,
	DirectedNetInterface* interfaceNotToFwdBackTo/*��ת���ӿ�*/)
{
	do {
		// First, do the datagram send, to each destination:
		// ���ȣ��������ݱ���ÿһ��Ŀ��
		Boolean writeSuccess = True;
		for (destRecord* dests = fDests; dests != NULL; dests = dests->fNext)
		{
			if (!write(dests->fGroupEId.groupAddress().s_addr, dests->fPort, ttlToSend,
				buffer, bufferSize)) {
				writeSuccess = False;
				break;	//��һ������ʧ�ܣ�������
			}
		}
		if (!writeSuccess) break;	//����ʧ�ܣ�ֱ������
		// ͳ������
		statsOutgoing.countPacket(bufferSize);
		statsGroupOutgoing.countPacket(bufferSize);

		// Then, forward to our members:Ȼ��ת�������ǵĳ�Ա��
		int numMembers = 0;
		if (!members().IsEmpty()) {
			numMembers =
				outputToAllMembersExcept(interfaceNotToFwdBackTo,
				ttlToSend, buffer, bufferSize,
				ourIPAddress(env)/*����ĵ�ַ*/);
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
	// ��ȡ�����׽��ֵ����ݣ���ת���������ӵ��κ�һ�����
	//##### later make this code more general - independent of tunnels
	//##### ������ʹ��δ����ͨ�� - �޹ص����

	bytesRead = 0;

	int maxBytesToRead = bufferMaxSize - TunnelEncapsulationTrailerMaxSize;
	// ��socket��ȡ����
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
	// ���������һ��SSM�飬ȷ��Դ��ַƥ�䣺
	if (isSSM()
		&& fromAddress.sin_addr.s_addr != sourceFilterAddress().s_addr) {
		return True;
	}

	// We'll handle this data.
	// Also write it (with the encapsulation trailer) to each member,
	// unless the packet was originally sent by us to begin with.
	// ���ǻᴦ����Щ���ݡ�
	// Ҳ�ǰ������͵���encapsulation trailer��ÿ����Ա
	// ���Ǹ����ݰ�����������Ƿ�����ʼ��
	bytesRead = numBytes;

	int numMembers = 0;
	if (!wasLoopedBackFromUs(env(), fromAddress)) {
		//���Ǳ��ػ��أ�ͳ������
		statsIncoming.countPacket(numBytes);
		statsGroupIncoming.countPacket(numBytes);
		numMembers =	//���͵�ÿһ����Ա
			outputToAllMembersExcept(NULL, ttl(),
			buffer, bytesRead,
			fromAddress.sin_addr.s_addr);
		if (numMembers > 0) {
			// ͳ���м�����
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
	// Don't forward TTL-0 packets����ת��TTLΪ0��
	if (ttlToFwd == 0) return 0;

	DirectedNetInterfaceSet::Iterator iter(members());
	unsigned numMembers = 0;
	DirectedNetInterface* interf;
	while ((interf = iter.next()) != NULL) {
		// Check whether we've asked to exclude this interface:
		// ��������Ƿ��Ѿ�Ҫ���ų�����ӿڣ�
		if (interf == exceptInterface)
			continue;

		// Check that the packet's source address makes it OK to
		// be relayed across this interface:
		// ������ݰ���Դ��ַ��ʹ����ȷ������ýӿڱ�ת����
		UsageEnvironment& saveEnv = env();
		// because the following call may delete "this"
		// ��Ϊ����ĵ��ÿ��ܻ� delete this
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
			// ����֪�������ǽ�ת��������һ����Ա��������������tunnel encapsulation trailer��
			// (Note: Allow for it not being 4-byte-aligned.)
			// (ע������������4�ֽڶ���ġ�)
			TunnelEncapsulationTrailer* trailerInPacket
				= (TunnelEncapsulationTrailer*)&data[size];
			TunnelEncapsulationTrailer* trailer;

			Boolean misaligned = ((uintptr_t)trailerInPacket & 3) != 0;
			unsigned trailerOffset;
			u_int8_t tunnelCmd;
			if (isSSM()) {
				// add an 'auxilliary address' before the trailer
				// �� trailerǰ��һ�������ɸ�����ַ��
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
				trailer->port() = fDests->fPort; // structure copy, outputs in network order �ṹ���ƣ������ֽ������
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
// Ϊenv.groupsockPriv->socketTable����(������ʱ����)ͨ���׽��ֱ������Groupsocks�Ĺ�ϣ��
static HashTable*& getSocketTable(UsageEnvironment& env)
{
	_groupsockPriv* priv = groupsockPriv(env);
	if (priv->socketTable == NULL) { // We need to create it
		// ����hashTable
		priv->socketTable = HashTable::create(ONE_WORD_HASH_KEYS);
	}
	return priv->socketTable;
}

// ��groupsock->env().groupsockPriv->socketTable��ϣ���в���groupsock
// ����ҵ��˾��Ƴ��������ϣ�����Ѿ�û����Ԫ�أ����ͷŹ�ϣ��
static Boolean unsetGroupsockBySocket(Groupsock const* groupsock)
{
	do {
		if (groupsock == NULL) break;

		int sock = groupsock->socketNum();
		// Make sure "sock" is in bounds:
		// ȷ��sock���ڷ�Χ�ڣ�
		if (sock < 0) break;
		//���ù�ϣ��
		HashTable*& sockets = getSocketTable(groupsock->env());
		//�ӹ�ϣ���в���sock��Ӧ��Groupsock����
		Groupsock* gs = (Groupsock*)sockets->Lookup((char*)(long)sock);
		if (gs == NULL || gs != groupsock) break;
		sockets->Remove((char*)(long)sock);	//�ҵ��˾��Ƴ�

		if (sockets->IsEmpty()) {
			// We can also delete the table (to reclaim space):
			// û����Ԫ�أ����ͷŹ�ϣ��
			delete sockets; sockets = NULL;
			reclaimGroupsockPriv(gs->env());
		}

		return True;
	} while (0);

	return False;
}

// ��groupsock->env().groupsockPriv->socketTable���һ����Ŀ
static Boolean setGroupsockBySocket(UsageEnvironment& env, int sock,
	Groupsock* groupsock)
{
	do {
		// Make sure the "sock" parameter is in bounds:
		// ȷ��sock�������ڷ�Χ��
		if (sock < 0) {
			char buf[100];
			sprintf(buf, "trying to use bad socket (%d)", sock);
			env.setResultMsg(buf);
			break;
		}
		//����groupsock->env().groupsockPriv->socketTable
		HashTable* sockets = getSocketTable(env);

		// Make sure we're not replacing an existing Groupsock (although that shouldn't happen)
		// ȷ�����ǲ����滻���е�Groupsock�������ⲻӦ�÷�����
		Boolean alreadyExists
			= (sockets->Lookup((char*)(long)sock) != 0);
		if (alreadyExists) {	//�����ϣ�����Ѿ�����sock��Ӧ����Ŀ
			char buf[100];
			sprintf(buf,
				"Attempting to replace an existing socket (%d",
				sock);
			env.setResultMsg(buf);
			break;
		}
		// ���һ����Ŀ��sockΪkey,groupsockΪvalue
		sockets->Add((char*)(long)sock, groupsock);
		return True;
	} while (0);

	return False;
}

// ��groupsock->env().groupsockPriv->socketTable�в���sock��Ӧ��groupsock
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
		// ����Groupsock���� --value
		struct in_addr groupAddr; groupAddr.s_addr = groupAddress;
		if (sourceFilterAddress == netAddressBits(~0)) {
			// regular, ISM groupsock	����Դ
			groupsock = new Groupsock(env, groupAddr, port, ttl);
		}
		else {
			// SSM groupsock	�ض�Դ
			struct in_addr sourceFilterAddr;
			sourceFilterAddr.s_addr = sourceFilterAddress;
			groupsock = new Groupsock(env, groupAddr, sourceFilterAddr, port);
		}

		if (groupsock == NULL || groupsock->socketNum() < 0) break;
		// ��ӵ�env.groupsockPriv->socketTable
		if (!setGroupsockBySocket(env, groupsock->socketNum(), groupsock)) break;
		//��ӵ�fTable
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
