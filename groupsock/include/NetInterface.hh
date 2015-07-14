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
// Network Interfaces
// C++ header

#ifndef _NET_INTERFACE_HH
#define _NET_INTERFACE_HH

#ifndef _NET_ADDRESS_HH
#include "NetAddress.hh"
#endif


class NetInterface {
public:
	virtual ~NetInterface();

	static UsageEnvironment* DefaultUsageEnvironment;
	// if non-NULL, used for each new interfaces
	// 如果非NULL，用于每一个新接口
protected:
	NetInterface(); // virtual base class
};

class DirectedNetInterface : public NetInterface {
public:
	virtual ~DirectedNetInterface();

	virtual Boolean write(unsigned char* data, unsigned numBytes) = 0;

	virtual Boolean SourceAddrOKForRelaying(UsageEnvironment& env,
		unsigned addr) = 0;

protected:
	DirectedNetInterface(); // virtual base class
};

class DirectedNetInterfaceSet {
public:
	// 创建哈希表，键类型为ONE_WORD_HASH_KEYS(u_intptr_t)
	DirectedNetInterfaceSet();
	// 销毁哈希表
	virtual ~DirectedNetInterfaceSet();

	// 添加条目到哈希表，参数interf即作为key又作为value
	DirectedNetInterface* Add(DirectedNetInterface const* interf);
	// Returns the old value if different, otherwise 0

	Boolean Remove(DirectedNetInterface const* interf);

	Boolean IsEmpty() { return fTable->IsEmpty(); }

	// Used to iterate through the interfaces in the set
	class Iterator {
	public:
		Iterator(DirectedNetInterfaceSet& interfaces);
		virtual ~Iterator();

		DirectedNetInterface* next(); // NULL iff none

	private:
		HashTable::Iterator* fIter;
	};

private:
	friend class Iterator;
	HashTable* fTable;		// 保存在哈希表
};

class Socket : public NetInterface {
public:
	virtual ~Socket();

	// 读处理。返回false表示出错
	virtual Boolean handleRead(unsigned char* buffer, unsigned bufferMaxSize,
		unsigned& bytesRead, struct sockaddr_in& fromAddress) = 0;
	// Returns False on error; resultData == NULL if data ignored

	int socketNum() const { return fSocketNum; }

	Port port() const {
		return fPort;
	}

	UsageEnvironment& env() const { return fEnv; }

	static int DebugLevel;	// Debug等级

protected:
	// 设定使用环境和端口
	Socket(UsageEnvironment& env, Port port); // virtual base class

	// 改变端口号
	Boolean changePort(Port newPort); // will also cause socketNum() to change

private:
	int fSocketNum;			// Socket套接口
	UsageEnvironment& fEnv;	// 使用环境
	Port fPort;				// 端口
};

UsageEnvironment& operator<<(UsageEnvironment& s, const Socket& sock);

// A data structure for looking up a Socket by port:
// 一个数据结构，用于使用port来查找Socket
class SocketLookupTable {
public:
	virtual ~SocketLookupTable();
	// 使用port来查找对应的Socket，没有找到就创建一个。isNew是传出参数，指示返回的是否为新创建的
	Socket* Fetch(UsageEnvironment& env, Port port, Boolean& isNew);
	// Creates a new Socket if none already exists
	// 创建一个新的Socket如果不是已经存在

	// 从哈希表中移除sock
	Boolean Remove(Socket const* sock);

protected:
	SocketLookupTable(); // abstract base class
	virtual Socket* CreateNew(UsageEnvironment& env, Port port) = 0;

private:
	HashTable* fTable;
};

// A data structure for counting traffic:
// 一个数据结构，用于统计流量
class NetInterfaceTrafficStats {
public:
	//构造，初始化数据成员为0
	NetInterfaceTrafficStats();
	// 统计packet
	void countPacket(unsigned packetSize);

	float totNumPackets() const { return fTotNumPackets; }
	float totNumBytes() const { return fTotNumBytes; }
	// 以及统计过流量
	Boolean haveSeenTraffic() const;

private:
	float fTotNumPackets;	//总包数 total Number packets
	float fTotNumBytes;		//总字节数
};

#endif
