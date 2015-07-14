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
// Network Addresses
// C++ header

#ifndef _NET_ADDRESS_HH
#define _NET_ADDRESS_HH

#ifndef _HASH_TABLE_HH
#include "HashTable.hh"
#endif

#ifndef _NET_COMMON_H
#include "NetCommon.h"
#endif

#ifndef _USAGE_ENVIRONMENT_HH
#include "UsageEnvironment.hh"
#endif

// Definition of a type representing a low-level network address.
// At present, this is 32-bits, for IPv4.  Later, generalize it,
// to allow for IPv6.
// һ�ִ���ײ������ַ���塣Ŀǰ��Ĭ����32λ��IPv4������������չ֧��IPv6��
typedef u_int32_t netAddressBits;

class NetAddress {
public:
	NetAddress(u_int8_t const* data,
		unsigned length = 4 /* default: 32 bits IPv4*/);
	NetAddress(unsigned length = 4); // sets address data to all-zeros
	NetAddress(NetAddress const& orig);
	NetAddress& operator=(NetAddress const& rightSide);
	virtual ~NetAddress();

	unsigned length() const { return fLength; }
	u_int8_t const* data() const // always in network byte order
	{
		return fData;
	}

private:
	void assign(u_int8_t const* data, unsigned length);
	void clean();

	unsigned fLength;
	u_int8_t* fData;
};

class NetAddressList {
public:
	// ���캯��hostname������һ�����ʮ���Ƶ�IP��ַ��Ҳ��������������
	NetAddressList(char const* hostname);
	NetAddressList(NetAddressList const& orig);
	NetAddressList& operator=(NetAddressList const& rightSide);
	virtual ~NetAddressList();
	//��ȡ��ַ����Ԫ�ظ���
	unsigned numAddresses() const { return fNumAddresses; }
	//��ȡ��ַ���һ����ַ���ڴ��ַ
	NetAddress const* firstAddress() const;

	// Used to iterate through the addresses in a list:
	// ���ڱ����б��еĵ�ַ��
	class Iterator {
	public:
		Iterator(NetAddressList const& addressList);
		NetAddress const* nextAddress(); // NULL iff noneû�и����ַ��
	private:
		NetAddressList const& fAddressList;	//�����һ����ַ��
		unsigned fNextIndex;	//��һ����ַ������
	};

private:
	//Ϊ��ַ�������ڴ�ռ䣬������addressArray�е����ݿ�����ȥ
	void assign(netAddressBits numAddresses, NetAddress** addressArray);
	//ɾ����ַ��͵�ַ�������е�ַ
	void clean();

	friend class Iterator;
	unsigned fNumAddresses;		//��ַ����
	NetAddress** fAddressArray;	//��ַ��
};

typedef u_int16_t portNumBits;

class Port {
public:
	Port(portNumBits num /* in host byte order */);

	portNumBits num() const // in network byte order
	{
		return fPortNum;
	}

private:
	portNumBits fPortNum; // stored in network byte order�洢�������ֽ�˳��
#ifdef IRIX
	portNumBits filler; // hack to overcome a bug in IRIX C++ compiler
						// �ڿ����IRIX��C + +����������
#endif
};

// ����ȫ�ֵ�
UsageEnvironment& operator<<(UsageEnvironment& s, const Port& p);


// A generic table for looking up objects by (address1, address2, port)
// ���ڲ��Ҷ���,ͨ��һ��ͨ�ñ���ַ1����ַ2���˿ڣ�
class AddressPortLookupTable {
public:
	// Ϊ�ڲ���ϣ��fTable�������󣬹�ϣ���key��3��Ԫ�ص�unsigned int����
	AddressPortLookupTable();
	// �ͷ��ڲ���ϣ��fTable
	virtual ~AddressPortLookupTable();

	// ʹ��address1��address2��port���key,valueΪֵ��ӵ���ϣ�� 
	// �����Ӧkey����Ŀ�Ѿ����ڣ����ؾɵ�value�����򷵻�NULL
	void* Add(netAddressBits address1, netAddressBits address2,
		Port port, void* value);
	// Returns the old value if different, otherwise 0

	//�ӹ�ϣ�����Ƴ�key��Ӧ����Ŀ����Ӧ��Ŀ���ڷ���true
	Boolean Remove(netAddressBits address1, netAddressBits address2,
		Port port);
	// �ӹ�ϣ���в���key��Ӧ��value��û�ҵ�����NULL
	void* Lookup(netAddressBits address1, netAddressBits address2,
		Port port);
	// Returns 0 if not found

	// Used to iterate through the entries in the table
	// ���ڱ����ڱ��е���Ŀ
	class Iterator {
	public:
		Iterator(AddressPortLookupTable& table);
		virtual ~Iterator();

		void* next(); // NULL iff none

	private:
		HashTable::Iterator* fIter;	//��ϣ�������
	};

private:
	friend class Iterator;
	HashTable* fTable;		//��ϣ��
};


Boolean IsMulticastAddress(netAddressBits address);


// A mechanism for displaying an IPv4 address in ASCII.  This is intended to replace "inet_ntoa()", which is not thread-safe.
// һ�ֻ��ƣ�����ASCII����ʾIPv4��ַ������Ϊ���滻"inet_ntoa()",���ǲ����̰߳�ȫ�ġ�
class AddressString {
public:
	AddressString(struct sockaddr_in const& addr);
	AddressString(struct in_addr const& addr);
	AddressString(netAddressBits addr); // "addr" is assumed to be in host byte order here

	virtual ~AddressString();

	char const* val() const { return fVal; }

private:
	void init(netAddressBits addr); // used to implement each of the constructors

private:
	char* fVal; // The result ASCII string: allocated by the constructor; deleted by the destructor
	//������ASCII���ַ������ɹ��캯������;������������ɾ��
};

#endif
