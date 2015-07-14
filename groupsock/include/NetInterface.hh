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
	// �����NULL������ÿһ���½ӿ�
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
	// ������ϣ��������ΪONE_WORD_HASH_KEYS(u_intptr_t)
	DirectedNetInterfaceSet();
	// ���ٹ�ϣ��
	virtual ~DirectedNetInterfaceSet();

	// �����Ŀ����ϣ������interf����Ϊkey����Ϊvalue
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
	HashTable* fTable;		// �����ڹ�ϣ��
};

class Socket : public NetInterface {
public:
	virtual ~Socket();

	// ����������false��ʾ����
	virtual Boolean handleRead(unsigned char* buffer, unsigned bufferMaxSize,
		unsigned& bytesRead, struct sockaddr_in& fromAddress) = 0;
	// Returns False on error; resultData == NULL if data ignored

	int socketNum() const { return fSocketNum; }

	Port port() const {
		return fPort;
	}

	UsageEnvironment& env() const { return fEnv; }

	static int DebugLevel;	// Debug�ȼ�

protected:
	// �趨ʹ�û����Ͷ˿�
	Socket(UsageEnvironment& env, Port port); // virtual base class

	// �ı�˿ں�
	Boolean changePort(Port newPort); // will also cause socketNum() to change

private:
	int fSocketNum;			// Socket�׽ӿ�
	UsageEnvironment& fEnv;	// ʹ�û���
	Port fPort;				// �˿�
};

UsageEnvironment& operator<<(UsageEnvironment& s, const Socket& sock);

// A data structure for looking up a Socket by port:
// һ�����ݽṹ������ʹ��port������Socket
class SocketLookupTable {
public:
	virtual ~SocketLookupTable();
	// ʹ��port�����Ҷ�Ӧ��Socket��û���ҵ��ʹ���һ����isNew�Ǵ���������ָʾ���ص��Ƿ�Ϊ�´�����
	Socket* Fetch(UsageEnvironment& env, Port port, Boolean& isNew);
	// Creates a new Socket if none already exists
	// ����һ���µ�Socket��������Ѿ�����

	// �ӹ�ϣ�����Ƴ�sock
	Boolean Remove(Socket const* sock);

protected:
	SocketLookupTable(); // abstract base class
	virtual Socket* CreateNew(UsageEnvironment& env, Port port) = 0;

private:
	HashTable* fTable;
};

// A data structure for counting traffic:
// һ�����ݽṹ������ͳ������
class NetInterfaceTrafficStats {
public:
	//���죬��ʼ�����ݳ�ԱΪ0
	NetInterfaceTrafficStats();
	// ͳ��packet
	void countPacket(unsigned packetSize);

	float totNumPackets() const { return fTotNumPackets; }
	float totNumBytes() const { return fTotNumBytes; }
	// �Լ�ͳ�ƹ�����
	Boolean haveSeenTraffic() const;

private:
	float fTotNumPackets;	//�ܰ��� total Number packets
	float fTotNumBytes;		//���ֽ���
};

#endif
