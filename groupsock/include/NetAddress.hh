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
// 一种代表底层网络地址定义。目前，默认它32位，IPv4。将来，可扩展支持IPv6。
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
	// 构造函数hostname可以是一个点分十进制的IP地址，也可以是主机域名
	NetAddressList(char const* hostname);
	NetAddressList(NetAddressList const& orig);
	NetAddressList& operator=(NetAddressList const& rightSide);
	virtual ~NetAddressList();
	//获取地址表中元素个数
	unsigned numAddresses() const { return fNumAddresses; }
	//获取地址表第一个地址的内存地址
	NetAddress const* firstAddress() const;

	// Used to iterate through the addresses in a list:
	// 用于遍历列表中的地址：
	class Iterator {
	public:
		Iterator(NetAddressList const& addressList);
		NetAddress const* nextAddress(); // NULL iff none没有跟多地址了
	private:
		NetAddressList const& fAddressList;	//必须绑定一个地址表
		unsigned fNextIndex;	//下一个地址的索引
	};

private:
	//为地址表申请内存空间，并将表addressArray中的内容拷贝进去
	void assign(netAddressBits numAddresses, NetAddress** addressArray);
	//删除地址表和地址表中所有地址
	void clean();

	friend class Iterator;
	unsigned fNumAddresses;		//地址个数
	NetAddress** fAddressArray;	//地址表
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
	portNumBits fPortNum; // stored in network byte order存储在网络字节顺序
#ifdef IRIX
	portNumBits filler; // hack to overcome a bug in IRIX C++ compiler
						// 黑客针对IRIX的C + +编译器错误
#endif
};

// 重载全局的
UsageEnvironment& operator<<(UsageEnvironment& s, const Port& p);


// A generic table for looking up objects by (address1, address2, port)
// 用于查找对象,通过一个通用表（地址1，地址2，端口）
class AddressPortLookupTable {
public:
	// 为内部哈希表fTable创建对象，哈希表的key是3个元素的unsigned int数组
	AddressPortLookupTable();
	// 释放内部哈希表fTable
	virtual ~AddressPortLookupTable();

	// 使用address1、address2、port组成key,value为值添加到哈希表 
	// 如果对应key的条目已经存在，返回旧的value，否则返回NULL
	void* Add(netAddressBits address1, netAddressBits address2,
		Port port, void* value);
	// Returns the old value if different, otherwise 0

	//从哈希表中移除key对应的条目，对应条目存在返回true
	Boolean Remove(netAddressBits address1, netAddressBits address2,
		Port port);
	// 从哈希表中查找key对应的value，没找到返回NULL
	void* Lookup(netAddressBits address1, netAddressBits address2,
		Port port);
	// Returns 0 if not found

	// Used to iterate through the entries in the table
	// 用于遍历在表中的条目
	class Iterator {
	public:
		Iterator(AddressPortLookupTable& table);
		virtual ~Iterator();

		void* next(); // NULL iff none

	private:
		HashTable::Iterator* fIter;	//哈希表迭代器
	};

private:
	friend class Iterator;
	HashTable* fTable;		//哈希表
};


Boolean IsMulticastAddress(netAddressBits address);


// A mechanism for displaying an IPv4 address in ASCII.  This is intended to replace "inet_ntoa()", which is not thread-safe.
// 一种机制，用于ASCII码显示IPv4地址。这是为了替换"inet_ntoa()",这是不是线程安全的。
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
	//其结果是ASCII码字符串：由构造函数分配;在析构函数中删除
};

#endif
