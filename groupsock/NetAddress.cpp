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
// Implementation

#include "NetAddress.hh"
#include "GroupsockHelper.hh"

#include <stddef.h>
#include <stdio.h>
#if defined(__WIN32__) || defined(_WIN32)
#define USE_GETHOSTBYNAME 1 /*because at least some Windows don't have getaddrinfo()*/
#else
#ifndef INADDR_NONE
#define INADDR_NONE 0xFFFFFFFF
#endif
#endif

////////// NetAddress //////////

//构造函数，为fDate申请length字节内存空间，并将data指向内容拷贝到新空间
NetAddress::NetAddress(u_int8_t const* data, unsigned length) {
	assign(data, length);
}
//为fDate申请length字节内存空间，并将新空间清零
NetAddress::NetAddress(unsigned length) {
	fData = new u_int8_t[length];
	if (fData == NULL) {
		fLength = 0;
		return;
	}

	for (unsigned i = 0; i < length; ++i)	fData[i] = 0;
	fLength = length;
}

//拷贝构造
NetAddress::NetAddress(NetAddress const& orig) {
	assign(orig.data(), orig.length());
}
//重载 = 赋值
NetAddress& NetAddress::operator=(NetAddress const& rightSide) {
	if (&rightSide != this) {
		clean();
		assign(rightSide.data(), rightSide.length());
	}
	return *this;
}
//析构
NetAddress::~NetAddress() {
	clean();
}

//为fDate申请length字节内存空间，并将data指向内容拷贝到新空间
void NetAddress::assign(u_int8_t const* data, unsigned length) {
	fData = new u_int8_t[length];
	if (fData == NULL) {	//new失败会抛出异常
		fLength = 0;
		return;
	}

	for (unsigned i = 0; i < length; ++i)	fData[i] = data[i];
	fLength = length;
}
//清除地址数据
void NetAddress::clean() {
	delete[] fData; fData = NULL;
	fLength = 0;
}


////////// NetAddressList //////////

NetAddressList::NetAddressList(char const* hostname)
: fNumAddresses(0), fAddressArray(NULL) {
	// First, check whether "hostname" is an IP address string:
	// 首先，检查“hostname”是否是一个IP地址字符串
	netAddressBits addr = our_inet_addr((char*)hostname);
	if (addr != INADDR_NONE) {
		// Yes, it was an IP address string.  Return a 1-element list with this address:
		//它是一个IP地址字符串，那么这个地址表只需要1个元素
		fNumAddresses = 1;
		fAddressArray = new NetAddress*[fNumAddresses];
		if (fAddressArray == NULL) return;
		//申请空间，保存这个地址。注意保存的是整数地址而不是字符串
		fAddressArray[0] = new NetAddress((u_int8_t*)&addr, sizeof (netAddressBits));
		return;
	}

	// "hostname" is not an IP address string; try resolving it as a real host name instead:
	// 当它不是一个IP地址字符串，尝试解析hostname真实的地址来代替
#if defined(USE_GETHOSTBYNAME) || defined(VXWORKS)
	struct hostent* host;
#if defined(VXWORKS)
	char hostentBuf[512];

	host = (struct hostent*)resolvGetHostByName((char*)hostname, (char*)&hostentBuf, sizeof hostentBuf);
#else
	//gethostbyname()返回对应于给定主机名的包含主机名字和地址信息的hostent结构指针(不要试图delete这个返回的地址)
	host = gethostbyname((char*)hostname);
#endif
	if (host == NULL || host->h_length != 4 || host->h_addr_list == NULL) return; // no luck	  //不幸，没有得到

	u_int8_t const** const hAddrPtr = (u_int8_t const**)host->h_addr_list;
	// First, count the number of addresses:取得地址个数
	u_int8_t const** hAddrPtr1 = hAddrPtr;
	while (*hAddrPtr1 != NULL) {
		++fNumAddresses;
		++hAddrPtr1;
	}

	// Next, set up the list: 给地址表分配内存
	fAddressArray = new NetAddress*[fNumAddresses];
	if (fAddressArray == NULL) return;
	//逐个拷贝地址到地址表
	for (unsigned i = 0; i < fNumAddresses; ++i) {
		fAddressArray[i] = new NetAddress(hAddrPtr[i], host->h_length);
	}
#else
	// Use "getaddrinfo()" (rather than the older, deprecated "gethostbyname()"):
	struct addrinfo addrinfoHints;
	memset(&addrinfoHints, 0, sizeof addrinfoHints);
	addrinfoHints.ai_family = AF_INET; // For now, we're interested in IPv4 addresses only
	struct addrinfo* addrinfoResultPtr = NULL;
	int result = getaddrinfo(hostname, NULL, &addrinfoHints, &addrinfoResultPtr);
	if (result != 0 || addrinfoResultPtr == NULL) return; // no luck

	// First, count the number of addresses:
	const struct addrinfo* p = addrinfoResultPtr;
	while (p != NULL) {
		if (p->ai_addrlen < 4) continue; // sanity check: skip over addresses that are too small
		++fNumAddresses;
		p = p->ai_next;
	}

	// Next, set up the list:
	fAddressArray = new NetAddress*[fNumAddresses];
	if (fAddressArray == NULL) return;

	unsigned i = 0;
	p = addrinfoResultPtr;
	while (p != NULL) {
		if (p->ai_addrlen < 4) continue;
		fAddressArray[i++] = new NetAddress((u_int8_t const*)&(((struct sockaddr_in*)p->ai_addr)->sin_addr.s_addr), 4);
		p = p->ai_next;
	}

	// Finally, free the data that we had allocated by calling "getaddrinfo()":
	freeaddrinfo(addrinfoResultPtr);
#endif
}

NetAddressList::NetAddressList(NetAddressList const& orig) {
	assign(orig.numAddresses(), orig.fAddressArray);
}

NetAddressList& NetAddressList::operator=(NetAddressList const& rightSide) {
	if (&rightSide != this) {
		clean();
		assign(rightSide.numAddresses(), rightSide.fAddressArray);
	}
	return *this;
}

NetAddressList::~NetAddressList() {
	clean();
}

void NetAddressList::assign(unsigned numAddresses, NetAddress** addressArray) {
	//为地址表分配内存空间
	fAddressArray = new NetAddress*[numAddresses];
	if (fAddressArray == NULL) {
		fNumAddresses = 0;
		return;
	}
	//为地址表每个地址分配内存空间
	for (unsigned i = 0; i < numAddresses; ++i) {
		fAddressArray[i] = new NetAddress(*addressArray[i]);
	}
	fNumAddresses = numAddresses;
}

void NetAddressList::clean() {
	while (fNumAddresses-- > 0) {	//逐个删除地址
		delete fAddressArray[fNumAddresses];
	}
	//释放地址表
	delete[] fAddressArray; fAddressArray = NULL;
}

NetAddress const* NetAddressList::firstAddress() const {
	if (fNumAddresses == 0) return NULL;

	return fAddressArray[0];
}

////////// NetAddressList::Iterator //////////
NetAddressList::Iterator::Iterator(NetAddressList const& addressList)
: fAddressList(addressList), fNextIndex(0) {}

NetAddress const* NetAddressList::Iterator::nextAddress() {
	if (fNextIndex >= fAddressList.numAddresses()) return NULL; // no more
	return fAddressList.fAddressArray[fNextIndex++];
}


////////// Port //////////

Port::Port(portNumBits num /* in host byte order */) {
	fPortNum = htons(num);
}

UsageEnvironment& operator<<(UsageEnvironment& s, const Port& p) {
	return s << ntohs(p.num());
}


////////// AddressPortLookupTable //////////

AddressPortLookupTable::AddressPortLookupTable()
: fTable(HashTable::create(3)) { // three-word keys are used 键使用3个元素的unsigned int数组
}

AddressPortLookupTable::~AddressPortLookupTable() {
	delete fTable;
}

// 使用address1、address2、port组成key,value为值添加到哈希表
void* AddressPortLookupTable::Add(netAddressBits address1,
	netAddressBits address2,
	Port port, void* value) {
	int key[3];
	key[0] = (int)address1;
	key[1] = (int)address2;
	key[2] = (int)port.num();
	return fTable->Add((char*)key, value);
}

// 从哈希表中查找key对应的value，没找到返回NULL
void* AddressPortLookupTable::Lookup(netAddressBits address1,
	netAddressBits address2,
	Port port) {
	int key[3];
	key[0] = (int)address1;
	key[1] = (int)address2;
	key[2] = (int)port.num();
	return fTable->Lookup((char*)key);
}

//从哈希表中移除key对应的条目，对应条目存在返回true
Boolean AddressPortLookupTable::Remove(netAddressBits address1,
	netAddressBits address2,
	Port port) {
	int key[3];
	key[0] = (int)address1;
	key[1] = (int)address2;
	key[2] = (int)port.num();
	return fTable->Remove((char*)key);
}

// 创建迭代器，绑定地址端口查找表
AddressPortLookupTable::Iterator::Iterator(AddressPortLookupTable& table)
// 创建哈希表迭代器，绑定哈希表
: fIter(HashTable::Iterator::create(*(table.fTable))) {
}

AddressPortLookupTable::Iterator::~Iterator() {
	delete fIter;
}

// 返回当前迭代器指向条目的value,迭代器走向下一个
void* AddressPortLookupTable::Iterator::next() {
	char const* key; // dummy
	return fIter->next(key);
}


////////// isMulticastAddress() implementation //////////
// 判断参数释放是一个多播地址
Boolean IsMulticastAddress(netAddressBits address) {
	// Note: We return False for addresses in the range 224.0.0.0
	// through 224.0.0.255, because these are non-routable
	// 注意：我们在224.0.0.0到224.0.0.255范围地址返回false，因为这些是不可路由的 
	// Note: IPv4-specific #####
	// 注：支持IPv4特定#####
	netAddressBits addressInNetworkOrder = htonl(address);
	return addressInNetworkOrder > 0xE00000FF &&
		addressInNetworkOrder <= 0xEFFFFFFF;
}


////////// AddressString implementation //////////

AddressString::AddressString(struct sockaddr_in const& addr) {
	init(addr.sin_addr.s_addr);
}

AddressString::AddressString(struct in_addr const& addr) {
	init(addr.s_addr);
}

AddressString::AddressString(netAddressBits addr) {
	init(addr);
}

void AddressString::init(netAddressBits addr) {
	//针对的是IPv4类型，16byte足够,IPv6需要46Byte
	fVal = new char[16]; // large enough for "abc.def.ghi.jkl"
	//转为网络字节序
	netAddressBits addrNBO = htonl(addr); // make sure we have a value in a known byte order: big endian确保我们有一个已知的字节顺序值：大端
	//转为点分十进制表示
	sprintf(fVal, "%u.%u.%u.%u", (addrNBO >> 24) & 0xFF, (addrNBO >> 16) & 0xFF, (addrNBO >> 8) & 0xFF, addrNBO & 0xFF);
	
}

AddressString::~AddressString() {
	delete[] fVal;
}
