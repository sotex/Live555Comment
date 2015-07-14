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
// "mTunnel" multicast access service
// Copyright (c) 1996-2012 Live Networks, Inc.  All rights reserved.
// 'Group sockets'
// C++ header

#ifndef _GROUPSOCK_HH
#define _GROUPSOCK_HH

#ifndef _GROUPSOCK_VERSION_HH
#include "groupsock_version.hh"
#endif

#ifndef _NET_INTERFACE_HH
#include "NetInterface.hh"
#endif

#ifndef _GROUPEID_HH
#include "GroupEId.hh"
#endif

// An "OutputSocket" is (by default) used only to send packets.
// No packets are received on it (unless a subclass arranges this)
// OutputSocket是（默认）仅用于发送数据包的类。
// 没有接收数据包方法就可以了（除非子类添加这一点）
class OutputSocket : public Socket {
public:
	//调用父类构造初始化，使用内核分配端口，设置两个成员为0
	OutputSocket(UsageEnvironment& env);
	// 空的，还是调用基类的析构
	virtual ~OutputSocket();

	Boolean write(netAddressBits address, Port port, u_int8_t ttl,
		unsigned char* buffer, unsigned bufferSize);

protected:
	// 注意此处的权限
	OutputSocket(UsageEnvironment& env, Port port);

	portNumBits sourcePortNum() const { return fSourcePort.num(); }

private: // redefined virtual function
	virtual Boolean handleRead(unsigned char* buffer, unsigned bufferMaxSize,
		unsigned& bytesRead,
	struct sockaddr_in& fromAddress);

private:
	Port		fSourcePort;	//源端口
	u_int8_t	fLastSentTTL;	//最后发送TTL
};

class destRecord {
public:
	destRecord(struct in_addr const& addr, Port const& port, u_int8_t ttl,
		destRecord* next);
	virtual ~destRecord();

public:
	destRecord* fNext;	//指向下一条记录
	GroupEId fGroupEId;	//记录GroupEId
	Port fPort;			//记录端口号
};

// A "Groupsock" is used to both send and receive packets.
// As the name suggests, it was originally designed to send/receive
// multicast, but it can send/receive unicast as well.
// Groupsock 是用来发送和接收数据包。
// 正如其名称所暗示的，它最初被设计为多播数据发送/接收，但它可以很好的单播发送/接收。
class Groupsock : public OutputSocket {
public:
	Groupsock(UsageEnvironment& env, struct in_addr const& groupAddr,
		Port port, u_int8_t ttl);
	// used for a 'source-independent multicast' group
	// 用于“源无关组播'组(任意源组播)

	Groupsock(UsageEnvironment& env, struct in_addr const& groupAddr,
	struct in_addr const& sourceFilterAddr/*源地址过滤*/,
		Port port);
	// used for a 'source-specific multicast' group
	// 用于特定源组播

	virtual ~Groupsock();

	// 改变目标参数(fDest第一个节点)
	void changeDestinationParameters(struct in_addr const& newDestAddr,
		Port newDestPort, int newDestTTL);
	// By default, the destination address, port and ttl for
	// outgoing packets are those that were specified in
	// the constructor.  This works OK for multicast sockets,
	// but for unicast we usually want the destination port
	// number, at least, to be different from the source port.
	// 默认情况下，目的地址，端口和TTL传出数据包是在构造中指定的那些。
	// 这适用于组播套接字确定,但对于单播,我们通常希望的目的端口号,至少是来自源端口不同的。
	// (If a parameter is 0 (or ~0 for ttl), then no change made.)
	//（如果参数为0（或~0为TTL），则不改变made）

	// As a special case, we also allow multiple destinations (addresses & ports)
	// (This can be used to implement multi-unicast.)
	//作为一个特殊的情况下，我们也允许多个目标（地址和端口）
	//（这可以用于实现多播）。

	// 添加目标
	void addDestination(struct in_addr const& addr, Port const& port);
	// 移除目标
	void removeDestination(struct in_addr const& addr, Port const& port);
	// 移除所有目标
	void removeAllDestinations();

	struct in_addr const& groupAddress() const 
	{
		return fIncomingGroupEId.groupAddress();
	}
	struct in_addr const& sourceFilterAddress() const 
	{
		return fIncomingGroupEId.sourceFilterAddress();
	}

	Boolean isSSM() const 
	{
		return fIncomingGroupEId.isSSM();
	}

	u_int8_t ttl() const { return fTTL; }

	// 仅组播发送
	void multicastSendOnly(); // send, but don't receive any multicast packets发送，但不接收任何多播报文

	Boolean output(UsageEnvironment& env, u_int8_t ttl,
		unsigned char* buffer, unsigned bufferSize,
		DirectedNetInterface* interfaceNotToFwdBackTo = NULL);

	DirectedNetInterfaceSet& members() { return fMembers; }

	Boolean deleteIfNoMembers;
	Boolean isSlave; // for tunneling

	static NetInterfaceTrafficStats statsIncoming;	//统计传入数据
	static NetInterfaceTrafficStats statsOutgoing;	//统计传出数据
	static NetInterfaceTrafficStats statsRelayedIncoming;	//统计中继传入数据
	static NetInterfaceTrafficStats statsRelayedOutgoing;	//统计中继传出数据
	NetInterfaceTrafficStats statsGroupIncoming; // *not* static
	NetInterfaceTrafficStats statsGroupOutgoing; // *not* static
	NetInterfaceTrafficStats statsGroupRelayedIncoming; // *not* static
	NetInterfaceTrafficStats statsGroupRelayedOutgoing; // *not* static

	// 检测是否是在本地环回
	Boolean wasLoopedBackFromUs(UsageEnvironment& env,
	struct sockaddr_in& fromAddress);

public: // redefined virtual functions

	virtual Boolean handleRead(unsigned char* buffer, unsigned bufferMaxSize,
		unsigned& bytesRead,
	struct sockaddr_in& fromAddress);

private:
	// 发送到所有成员，排除exceptInterface
	int outputToAllMembersExcept(DirectedNetInterface* exceptInterface,
		u_int8_t ttlToFwd,
		unsigned char* data, unsigned size,
		netAddressBits sourceAddr);

private:
	GroupEId fIncomingGroupEId;		//传入GroupEId
	destRecord* fDests;		//目标记录链表头指针
	u_int8_t fTTL;		//Time To Live
	DirectedNetInterfaceSet fMembers;	//有向网络接口接口集
};

UsageEnvironment& operator<<(UsageEnvironment& s, const Groupsock& g);





// A data structure for looking up a 'groupsock'
// 数据结构，寻找一个'groupsock“
// by (multicast address, port), or by socket number
// 通过（多播地址，端口），或通过套接字数值
class GroupsockLookupTable {
public:
	// 通过groupAddress和port查找一个Groupsock对象，没找到就新建一个
	Groupsock* Fetch(UsageEnvironment& env, netAddressBits groupAddress,
		Port port, u_int8_t ttl, Boolean& isNew);
	// Creates a new Groupsock if none already exists

	Groupsock* Fetch(UsageEnvironment& env, netAddressBits groupAddress,
		netAddressBits sourceFilterAddr,
		Port port, Boolean& isNew);
	// Creates a new Groupsock if none already exists

	Groupsock* Lookup(netAddressBits groupAddress, Port port);
	// Returns NULL if none already exists

	Groupsock* Lookup(netAddressBits groupAddress,
		netAddressBits sourceFilterAddr,
		Port port);
	// Returns NULL if none already exists

	Groupsock* Lookup(UsageEnvironment& env, int sock);
	// Returns NULL if none already exists

	Boolean Remove(Groupsock const* groupsock);

	// Used to iterate through the groupsocks in the table
	class Iterator {
	public:
		Iterator(GroupsockLookupTable& groupsocks);

		Groupsock* next(); // NULL iff none

	private:
		AddressPortLookupTable::Iterator fIter;
	};

private:
	// 向fTable中添加条目，也向(env.groupsockPriv->socketTable)
	Groupsock* AddNew(UsageEnvironment& env/*使用环境*/,
		netAddressBits groupAddress/*组地址*/,
		netAddressBits sourceFilterAddress/*源地址过滤器*/,
		Port port, u_int8_t ttl);

private:
	friend class Iterator;
	AddressPortLookupTable fTable;	//哈希表
};

#endif
