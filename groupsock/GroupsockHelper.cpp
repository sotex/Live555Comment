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
// Helper routines to implement 'group sockets'
// Implementation

#include "GroupsockHelper.hh"


#if defined(__WIN32__) || defined(_WIN32)
#include <time.h>
extern "C" int initializeWinsockIfNecessary();
#else
#include <stdarg.h>
#include <time.h>
#include <fcntl.h>
#define initializeWinsockIfNecessary() 1
#endif
#include <stdio.h>

// By default, use INADDR_ANY for the sending and receiving interfaces:
// 默认情况下，使用INADDR_ANY为发送和接收接口：
netAddressBits SendingInterfaceAddr = INADDR_ANY;
netAddressBits ReceivingInterfaceAddr = INADDR_ANY;

static void socketErr(UsageEnvironment& env, char const* errorMsg) {
	env.setResultErrMsg(errorMsg);
}

// 构造的时候为env.groupsockPriv分配对象
// 并设置groupsockPriv对象的reuseFlag=0
NoReuse::NoReuse(UsageEnvironment& env)
: fEnv(env) {
	groupsockPriv(fEnv)->reuseFlag = 0;
}

// 若groupsockPriv对象的socketTable==NULL
// 析构的时候为env.groupsockPriv释放对象
NoReuse::~NoReuse() {
	groupsockPriv(fEnv)->reuseFlag = 1;
	reclaimGroupsockPriv(fEnv);
}


_groupsockPriv* groupsockPriv(UsageEnvironment& env) {
	if (env.groupsockPriv == NULL) { // We need to create it
		_groupsockPriv* result = new _groupsockPriv;
		result->socketTable = NULL;	//默认为NULL
		result->reuseFlag = 1; // default value => allow reuse of socket numbers
		env.groupsockPriv = result;
	}
	return (_groupsockPriv*)(env.groupsockPriv);
}

void reclaimGroupsockPriv(UsageEnvironment& env) {
	_groupsockPriv* priv = (_groupsockPriv*)(env.groupsockPriv);
	// 两个成员是默认值的时候，进行释放
	if (priv->socketTable == NULL && priv->reuseFlag == 1/*default value*/) {
		// We can delete the structure (to save space); it will get created again, if needed:
		delete priv;
		env.groupsockPriv = NULL;
	}
}

// type socket类型，有SOCK_STREAM、SOCK_DGRAM、SOCK_RAW、SOCK_PACKET、SOCK_SEQPACKET等
static int createSocket(int type) {
	// Call "socket()" to create a (IPv4) socket of the specified type.
	// 调用“socket()创建一个（IPv4）指定类型的套接字。
	// But also set it to have the 'close on exec' property (if we can)
	// 还设置它具有“执行exec时关闭"属性（如果可以）
	int sock;

#ifdef SOCK_CLOEXEC
	sock = socket(AF_INET, type|SOCK_CLOEXEC, 0);
	if (sock != -1 || errno != EINVAL) return sock;
	// An "errno" of EINVAL likely means that the system wasn't happy with the SOCK_CLOEXEC; fall through and try again without it:
#endif

	sock = socket(AF_INET, type, 0);
#ifdef FD_CLOEXEC
	if (sock != -1) fcntl(sock, F_SETFD, FD_CLOEXEC);
#endif
	return sock;
}

// 设置数据报套接字
int setupDatagramSocket(UsageEnvironment& env, Port port) {
	// 初始化网络
	if (!initializeWinsockIfNecessary()) {
		socketErr(env, "Failed to initialize 'winsock': ");
		return -1;
	}
	// 创建数据报套接字
	int newSocket = createSocket(SOCK_DGRAM);
	if (newSocket < 0) {
		socketErr(env, "unable to create datagram socket: ");
		return newSocket;
	}
	// 获取env的groupsockPriv重新使用标识
	int reuseFlag = groupsockPriv(env)->reuseFlag;
	// 根据需要，为env释放groupsockPriv成员指向对象
	reclaimGroupsockPriv(env);
	/*
	用于任意类型、任意状态套接口的设置选项值。
	int setsockopt(int sockfd, int level, int optname,const void *optval, socklen_t optlen);
	sockfd：标识一个套接口的描述字。
	level：选项定义的层次；支持SOL_SOCKET、IPPROTO_TCP、IPPROTO_IP和IPPROTO_IPV6。
	optname：需设置的选项。
	optval：指针，指向存放选项待设置的新值的缓冲区。
	optlen：optval缓冲区长度。

	成功执行时，返回0。失败返回-1，errno被设为以下的某个值
	EBADF：sock不是有效的文件描述词
	EFAULT：optval指向的内存并非有效的进程空间
	EINVAL：在调用setsockopt()时，optlen无效
	ENOPROTOOPT：指定的协议层不能识别选项
	ENOTSOCK：sock描述的不是套接字
	*/
	// 设置允许重用本地地址和端口，reuseFlag用来接受传出值
	if (setsockopt(newSocket, SOL_SOCKET, SO_REUSEADDR,
		(const char*)&reuseFlag, sizeof reuseFlag) < 0) {
		socketErr(env, "setsockopt(SO_REUSEADDR) error: ");
		closeSocket(newSocket);
		return -1;
	}

#if defined(__WIN32__) || defined(_WIN32)
	// Windoze doesn't properly handle SO_REUSEPORT or IP_MULTICAST_LOOP
	// win-doze 贬义,可能是由于操作系统BUG很多,而且运行速度慢,导致在运行的是后你会DOZE(打瞌睡)
	// Windows无法正确的处理SO_REUSEPORT或 IP_MULTICAST_LOOP
#else
#ifdef SO_REUSEPORT		//在定义了重新使用端口宏下设置
	if (setsockopt(newSocket, SOL_SOCKET, SO_REUSEPORT,
		(const char*)&reuseFlag, sizeof reuseFlag) < 0) {
		socketErr(env, "setsockopt(SO_REUSEPORT) error: ");
		closeSocket(newSocket);
		return -1;
	}
#endif

#ifdef IP_MULTICAST_LOOP	//在定义了IP多播循环下设置
	const u_int8_t loop = 1;
	if (setsockopt(newSocket, IPPROTO_IP, IP_MULTICAST_LOOP,
		(const char*)&loop, sizeof loop) < 0) {
		socketErr(env, "setsockopt(IP_MULTICAST_LOOP) error: ");
		closeSocket(newSocket);
		return -1;
	}
#endif
#endif
	// Note: Windoze requires binding, even if the port number is 0
	// Windows 需要绑定，即使端口号是0
	netAddressBits addr = INADDR_ANY;	// 设置绑定地址是任意网口IP
#if defined(__WIN32__) || defined(_WIN32)
#else
	if (port.num() != 0 || ReceivingInterfaceAddr != INADDR_ANY) {
		//ReceivingInterfaceAddr是一个全局的定义，默认是INADDR_ANY
#endif
		if (port.num() == 0) addr = ReceivingInterfaceAddr;
		// 组建sockaddr_in结构体
		MAKE_SOCKADDR_IN(name, addr, port.num());
		// 绑定socket套接口和sockaddr地址
		if (bind(newSocket, (struct sockaddr*)&name, sizeof name) != 0) {
			char tmpBuffer[100];
			sprintf(tmpBuffer, "bind() error (port number: %d): ",
				ntohs(port.num()));
			socketErr(env, tmpBuffer);
			closeSocket(newSocket);
			return -1;
		}
#if defined(__WIN32__) || defined(_WIN32)
#else
	}
#endif

	// Set the sending interface for multicasts, if it's not the default:
	// 设置发送多播的接口，如果它不是默认的：
	if (SendingInterfaceAddr != INADDR_ANY) {
		struct in_addr addr;
		addr.s_addr = SendingInterfaceAddr;
		// 设置多播接口
		if (setsockopt(newSocket, IPPROTO_IP, IP_MULTICAST_IF,
			(const char*)&addr, sizeof addr) < 0) {
			socketErr(env, "error setting outgoing multicast interface: ");
			closeSocket(newSocket);
			return -1;
		}
	}

	return newSocket;
}

// 设置sock为非阻塞模式
Boolean makeSocketNonBlocking(int sock) {
#if defined(__WIN32__) || defined(_WIN32)
	unsigned long arg = 1;
	return ioctlsocket(sock, FIONBIO, &arg) == 0;
#elif defined(VXWORKS)
	int arg = 1;
	return ioctl(sock, FIONBIO, (int)&arg) == 0;
#else
	int curFlags = fcntl(sock, F_GETFL, 0);
	return fcntl(sock, F_SETFL, curFlags|O_NONBLOCK) >= 0;
#endif
}

// 设置sock为阻塞模式
Boolean makeSocketBlocking(int sock) {
#if defined(__WIN32__) || defined(_WIN32)
	unsigned long arg = 0;
	return ioctlsocket(sock, FIONBIO, &arg) == 0;
#elif defined(VXWORKS)
	int arg = 0;
	return ioctl(sock, FIONBIO, (int)&arg) == 0;
#else
	int curFlags = fcntl(sock, F_GETFL, 0);
	return fcntl(sock, F_SETFL, curFlags&(~O_NONBLOCK)) >= 0;
#endif
}

// 设置流式套接字
int setupStreamSocket(UsageEnvironment& env,
	Port port, Boolean makeNonBlocking) {
	if (!initializeWinsockIfNecessary()) {
		socketErr(env, "Failed to initialize 'winsock': ");
		return -1;
	}

	int newSocket = createSocket(SOCK_STREAM);
	if (newSocket < 0) {
		socketErr(env, "unable to create stream socket: ");
		return newSocket;
	}

	int reuseFlag = groupsockPriv(env)->reuseFlag;
	reclaimGroupsockPriv(env);
	if (setsockopt(newSocket, SOL_SOCKET, SO_REUSEADDR,
		(const char*)&reuseFlag, sizeof reuseFlag) < 0) {
		socketErr(env, "setsockopt(SO_REUSEADDR) error: ");
		closeSocket(newSocket);
		return -1;
	}

	// SO_REUSEPORT doesn't really make sense for TCP sockets, so we
	// normally don't set them.  However, if you really want to do this
	// #define REUSE_FOR_TCP
#ifdef REUSE_FOR_TCP
#if defined(__WIN32__) || defined(_WIN32)
	// Windoze doesn't properly handle SO_REUSEPORT
#else
#ifdef SO_REUSEPORT
	if (setsockopt(newSocket, SOL_SOCKET, SO_REUSEPORT,
		(const char*)&reuseFlag, sizeof reuseFlag) < 0) {
		socketErr(env, "setsockopt(SO_REUSEPORT) error: ");
		closeSocket(newSocket);
		return -1;
	}
#endif
#endif
#endif

	// Note: Windoze requires binding, even if the port number is 0
#if defined(__WIN32__) || defined(_WIN32)
#else
	if (port.num() != 0 || ReceivingInterfaceAddr != INADDR_ANY) {
#endif
		MAKE_SOCKADDR_IN(name, ReceivingInterfaceAddr, port.num());
		if (bind(newSocket, (struct sockaddr*)&name, sizeof name) != 0) {
			char tmpBuffer[100];
			sprintf(tmpBuffer, "bind() error (port number: %d): ",
				ntohs(port.num()));
			socketErr(env, tmpBuffer);
			closeSocket(newSocket);
			return -1;
		}
#if defined(__WIN32__) || defined(_WIN32)
#else
	}
#endif
	// 根据参数设置是否为非阻塞
	if (makeNonBlocking) {
		if (!makeSocketNonBlocking(newSocket)) {
			socketErr(env, "failed to make non-blocking: ");
			closeSocket(newSocket);
			return -1;
		}
	}

	return newSocket;
}

// 从套接口读数据
int readSocket(UsageEnvironment& env,
	int socket, unsigned char* buffer, unsigned bufferSize,
struct sockaddr_in& fromAddress) 
{
	SOCKLEN_T addressSize = sizeof fromAddress;
	// ssize_t recvfrom(int sockfd,void *buf,int len,unsigned int flags, struct sockaddr *from,socket_t *fromlen);
	// 读取主机经指定的socket传来的数据,并把数据传到由参数buf指向的内存空间,参数len为可接收数据的最大长度。flag一般设置为0。from是来源地址，fromlen传出来源长度
	// 如果正确接收返回接收到的字节数，失败返回-1.
	int bytesRead = recvfrom(socket, (char*)buffer, bufferSize, 0,
		(struct sockaddr*)&fromAddress,
		&addressSize);
	if (bytesRead < 0) {
		//##### HACK to work around bugs in Linux and Windows:
		int err = env.getErrno();
		if (err == 111 /*ECONNREFUSED (Linux) 连接请求被服务器拒绝*/
#if defined(__WIN32__) || defined(_WIN32)
			// What a piece of crap Windows is.  Sometimes
			// recvfrom() returns -1, but with an 'errno' of 0.
			// This appears not to be a real error; just treat
			// it as if it were a read of zero bytes, and hope
			// we don't have to do anything else to 'reset'
			// this alleged error:
			// 垃圾的Windows。有时recvfrom()返回- 1，但是有errno为0。
			// 这似乎不是一个真正的错误；只是把它当作一个读取零字节，并希望我们不需要做什么“reset”
			// 这所谓的错误：
			|| err == 0 || err == EWOULDBLOCK
#else
			|| err == EAGAIN
#endif
			|| err == 113 /*EHOSTUNREACH (Linux)*/) { // Why does Linux return this for datagram sock?
			fromAddress.sin_addr.s_addr = 0;
			return 0;
		}
		//##### END HACK
		socketErr(env, "recvfrom() error: ");
	}

	return bytesRead;
}

// 往套接口写数据
Boolean writeSocket(UsageEnvironment& env,
	int socket, struct in_addr address, Port port,
	u_int8_t ttlArg,
	unsigned char* buffer, unsigned bufferSize) 
{
	do {
		if (ttlArg != 0) {
			// TTL是 Time To Live的缩写，该字段指定IP包被路由器丢弃之前允许通过的最大网段数量。
			// TTL的最大值是255，TTL的一个推荐值是64。
			// Before sending, set the socket's TTL:发送前设置socket TTL
#if defined(__WIN32__) || defined(_WIN32)
#define TTL_TYPE int
#else
#define TTL_TYPE u_int8_t
#endif
			TTL_TYPE ttl = (TTL_TYPE)ttlArg;
			// 设置多播TTL值
			if (setsockopt(socket, IPPROTO_IP, IP_MULTICAST_TTL,
				(const char*)&ttl, sizeof ttl) < 0) {
				socketErr(env, "setsockopt(IP_MULTICAST_TTL) error: ");
				break;
			}
		}

		MAKE_SOCKADDR_IN(dest, address.s_addr, port.num());
		// 将buffer中的数据经socket发送到dest
		int bytesSent = sendto(socket, (char*)buffer, bufferSize, 0,
			(struct sockaddr*)&dest, sizeof dest);
		// 发送非全部发送成功
		if (bytesSent != (int)bufferSize) {
			char tmpBuf[100];
			sprintf(tmpBuf, "writeSocket(%d), sendTo() error: wrote %d bytes instead of %u: ", socket, bytesSent, bufferSize);
			socketErr(env, tmpBuf);
			break;
		}

		return True;	//发送成功返回
	} while (0);

	return False;	//失败返回
}

// 获取bufferSize	bufOptName
// SO_RCVBUF接收缓冲区大小 SO_SNDBUF发送缓冲区大小
// SO_RCVLOWAT接收缓冲区下限 SO_SNDLOWAT发送缓冲区下限
static unsigned getBufferSize(UsageEnvironment& env, int bufOptName,
	int socket) {
	unsigned curSize;
	SOCKLEN_T sizeSize = sizeof curSize;
	if (getsockopt(socket, SOL_SOCKET, bufOptName,
		(char*)&curSize, &sizeSize) < 0) {
		socketErr(env, "getBufferSize() error: ");
		return 0;
	}

	return curSize;
}


// 获取发送缓冲区size
unsigned getSendBufferSize(UsageEnvironment& env, int socket) {
	return getBufferSize(env, SO_SNDBUF, socket);
}
// 获取接收缓冲区size
unsigned getReceiveBufferSize(UsageEnvironment& env, int socket) {
	return getBufferSize(env, SO_RCVBUF, socket);
}

static unsigned setBufferTo(UsageEnvironment& env, int bufOptName,
	int socket, unsigned requestedSize) {
	SOCKLEN_T sizeSize = sizeof requestedSize;
	// 设置缓冲区大小
	setsockopt(socket, SOL_SOCKET, bufOptName, (char*)&requestedSize, sizeSize);

	// Get and return the actual, resulting buffer size:
	// 获取并返回实际的，缓冲区大小
	return getBufferSize(env, bufOptName, socket);
}

// 设置发送缓冲区size
unsigned setSendBufferTo(UsageEnvironment& env,
	int socket, unsigned requestedSize) {
	return setBufferTo(env, SO_SNDBUF, socket, requestedSize);
}

//设置接收缓冲区size
unsigned setReceiveBufferTo(UsageEnvironment& env,
	int socket, unsigned requestedSize) {
	return setBufferTo(env, SO_RCVBUF, socket, requestedSize);
}

static unsigned increaseBufferTo(UsageEnvironment& env, int bufOptName,
	int socket, unsigned requestedSize) {
	// First, get the current buffer size.  If it's already at least
	// as big as what we're requesting, do nothing.
	// 获取当前的缓冲区大小
	unsigned curSize = getBufferSize(env, bufOptName, socket);

	// Next, try to increase the buffer to the requested size,
	// or to some smaller size, if that's not possible:
	// 当前的小于要达到的
	while (requestedSize > curSize) {
		SOCKLEN_T sizeSize = sizeof requestedSize;
		if (setsockopt(socket, SOL_SOCKET, bufOptName,
			(char*)&requestedSize, sizeSize) >= 0) {
			// success
			return requestedSize;
		}
		requestedSize = (requestedSize + curSize) / 2;
	}

	return getBufferSize(env, bufOptName, socket);
}

// 增长发送缓冲区size
unsigned increaseSendBufferTo(UsageEnvironment& env,
	int socket, unsigned requestedSize) {
	return increaseBufferTo(env, SO_SNDBUF, socket, requestedSize);
}
// 增长接收缓冲区size
unsigned increaseReceiveBufferTo(UsageEnvironment& env,
	int socket, unsigned requestedSize) {
	return increaseBufferTo(env, SO_RCVBUF, socket, requestedSize);
}

// 在本地ReceivingInterfaceAddr上加入一个多播组地址groupAddress
Boolean socketJoinGroup(UsageEnvironment& env, int socket,
	netAddressBits groupAddress)
{	// 如果不是多播地址，直接忽略
	if (!IsMulticastAddress(groupAddress)) return True; // ignore this case
	/*	struct ip_mreq {
			struct in_addr  imr_multiaddr;  // 多播组IP地址
		struct in_addr  imr_interface;		// 本地IP地址的接口
		};*/
	struct ip_mreq imr;
	imr.imr_multiaddr.s_addr = groupAddress;
	imr.imr_interface.s_addr = ReceivingInterfaceAddr;
	// 将指定的接口加入多播
	if (setsockopt(socket, IPPROTO_IP, IP_ADD_MEMBERSHIP,
		(const char*)&imr, sizeof (struct ip_mreq)) < 0) {
#if defined(__WIN32__) || defined(_WIN32)
		if (env.getErrno() != 0) {
			// That piece-of-shit toy operating system (Windows) sometimes lies
			// about setsockopt() failing!
#endif
			socketErr(env, "setsockopt(IP_ADD_MEMBERSHIP) error: ");
			return False;
#if defined(__WIN32__) || defined(_WIN32)
		}
#endif
	}

	return True;
}

// 套接字离开多播组
Boolean socketLeaveGroup(UsageEnvironment&, int socket,
	netAddressBits groupAddress) 
{
	if (!IsMulticastAddress(groupAddress)) return True; // ignore this case

	struct ip_mreq imr;
	imr.imr_multiaddr.s_addr = groupAddress;
	imr.imr_interface.s_addr = ReceivingInterfaceAddr;
	//离开本地接口上不限源的多播组
	if (setsockopt(socket, IPPROTO_IP, IP_DROP_MEMBERSHIP,
		(const char*)&imr, sizeof (struct ip_mreq)) < 0) {
		return False;
	}

	return True;
}

// The source-specific join/leave operations require special setsockopt()
// commands, and a special structure (ip_mreq_source).  If the include files
// didn't define these, we do so here:
// 特定源的加入/离开操作需要特殊的setsockopt()命令，和一个特殊的结构体（ip_mreq_source）。
// 如果文件没有定义这些，我们这样做：
#if !defined(IP_ADD_SOURCE_MEMBERSHIP)
struct ip_mreq_source {
	struct  in_addr imr_multiaddr;  /* IP multicast address of group */
	struct  in_addr imr_sourceaddr; /* IP address of source */
	struct  in_addr imr_interface;  /* local IP address of interface */
};
#endif

#ifndef IP_ADD_SOURCE_MEMBERSHIP

#ifdef LINUX
#define IP_ADD_SOURCE_MEMBERSHIP   39
#define IP_DROP_SOURCE_MEMBERSHIP 40
#else
#define IP_ADD_SOURCE_MEMBERSHIP   25
#define IP_DROP_SOURCE_MEMBERSHIP 26
#endif

#endif

// 在指定的本地接口上加入一个特定源的多播组
Boolean socketJoinGroupSSM(UsageEnvironment& env, int socket,
	netAddressBits groupAddress,
	netAddressBits sourceFilterAddr) 
{
	if (!IsMulticastAddress(groupAddress)) return True; // ignore this case

	struct ip_mreq_source imr;
	imr.imr_multiaddr.s_addr = groupAddress;
	imr.imr_sourceaddr.s_addr = sourceFilterAddr;
	imr.imr_interface.s_addr = ReceivingInterfaceAddr;
	if (setsockopt(socket, IPPROTO_IP, IP_ADD_SOURCE_MEMBERSHIP,
		(const char*)&imr, sizeof (struct ip_mreq_source)) < 0) {
		socketErr(env, "setsockopt(IP_ADD_SOURCE_MEMBERSHIP) error: ");
		return False;
	}

	return True;
}

// 离开一个特定源的多播组
Boolean socketLeaveGroupSSM(UsageEnvironment& /*env*/, int socket,
	netAddressBits groupAddress,
	netAddressBits sourceFilterAddr) 
{
	if (!IsMulticastAddress(groupAddress)) return True; // ignore this case

	struct ip_mreq_source imr;
	imr.imr_multiaddr.s_addr = groupAddress;
	imr.imr_sourceaddr.s_addr = sourceFilterAddr;
	imr.imr_interface.s_addr = ReceivingInterfaceAddr;
	if (setsockopt(socket, IPPROTO_IP, IP_DROP_SOURCE_MEMBERSHIP,
		(const char*)&imr, sizeof (struct ip_mreq_source)) < 0) {
		return False;
	}

	return True;
}

// 获取socket的本地关联端口
static Boolean getSourcePort0(int socket, portNumBits& resultPortNum/*host order*/)
{
	sockaddr_in test; test.sin_port = 0;
	SOCKLEN_T len = sizeof test;
	// getsockname函数用于获取与某个套接字关联的本地协议地址 
	if (getsockname(socket, (struct sockaddr*)&test, &len) < 0) return False;

	resultPortNum = ntohs(test.sin_port);
	return True;
}

// 获取socket本地关联端口，若是没有，则调用bind由系统内核分配一个
Boolean getSourcePort(UsageEnvironment& env, int socket, Port& port) 
{
	portNumBits portNum = 0;
	if (!getSourcePort0(socket, portNum) || portNum == 0) {
		// Hack - call bind(), then try again:
		MAKE_SOCKADDR_IN(name, INADDR_ANY, 0);
		// socket为关联本地端口，进行bind操作
		bind(socket, (struct sockaddr*)&name, sizeof name);

		if (!getSourcePort0(socket, portNum) || portNum == 0) {
			socketErr(env, "getsockname() error: ");
			return False;
		}
	}

	port = Port(portNum);
	return True;
}

// IP地址错误检查
static Boolean badAddressForUs(netAddressBits addr) 
{
	// Check for some possible erroneous addresses:
	// 检查一些可能的地址错误
	netAddressBits nAddr = htonl(addr);
	return (nAddr == 0x7F000001 /* 127.0.0.1 */
		|| nAddr == 0
		|| nAddr == (netAddressBits)(~0));
}

Boolean loopbackWorks = 1;	//环回工作标识

netAddressBits ourIPAddress(UsageEnvironment& env) 
{
	static netAddressBits ourAddress = 0;
	int sock = -1;
	struct in_addr testAddr;

	if (ourAddress == 0) {
		// We need to find our source address
		// 我们需要找到我们的源地址
		struct sockaddr_in fromAddr;
		fromAddr.sin_addr.s_addr = 0;

		// Get our address by sending a (0-TTL) multicast packet,
		// receiving it, and looking at the source address used.
		// (This is kinda bogus, but it provides the best guarantee
		// that other nodes will think our address is the same as we do.)
		do {
			loopbackWorks = 0; // until we learn otherwise直到我们获知,否则...

			testAddr.s_addr = our_inet_addr("228.67.43.91"); // arbitrary任意
			Port testPort(15947); // ditto
			// 创建数据报socket
			sock = setupDatagramSocket(env, testPort);
			if (sock < 0) break;
			// 加入任意源多播组"228.67.43.91"
			if (!socketJoinGroup(env, sock, testAddr.s_addr)) break;

			unsigned char testString[] = "hostIdTest";
			unsigned testStringLength = sizeof testString;
			// 向socket写入数据"hostIdTest"
			if (!writeSocket(env, sock, testAddr, testPort, 0,
				testString, testStringLength)) break;

			// Block until the socket is readable (with a 5-second timeout):
			// 阻塞直到套接字可读（5秒超时）：
			fd_set rd_set;
			FD_ZERO(&rd_set);
			FD_SET((unsigned)sock, &rd_set);
			const unsigned numFds = sock + 1;
			struct timeval timeout;
			timeout.tv_sec = 5;
			timeout.tv_usec = 0;
			// 阻塞等待sock变为可读
			int result = select(numFds, &rd_set, NULL, NULL, &timeout);
			if (result <= 0) break;

			unsigned char readBuffer[20];
			// 从sock读取数据，捕获来源主机地址到fromAddr
			int bytesRead = readSocket(env, sock,
				readBuffer, sizeof readBuffer,
				fromAddr);
			if (bytesRead != (int)testStringLength
				|| strncmp((char*)readBuffer, (char*)testString, testStringLength) != 0) {
				break;
			}

			loopbackWorks = 1;
		} while (0);

		if (sock >= 0) {
			// 离开多播组
			socketLeaveGroup(env, sock, testAddr.s_addr);
			closeSocket(sock);
		}

		// 前面do while里面的操作没有都成功
		if (!loopbackWorks) do {
			// We couldn't find our address using multicast loopback,
			// so try instead to look it up directly - by first getting our host name, and then resolving this host name
			char hostname[100];
			hostname[0] = '\0';
			// 该函数把本地主机名存放入由hostname参数指定的缓冲区中。
			int result = gethostname(hostname, sizeof hostname);
			if (result != 0 || hostname[0] == '\0') {
				env.setResultErrMsg("initial gethostname() failed");
				break;
			}

			// Try to resolve "hostname" to an IP address:
			// 尝试解析hostname到IP地址
			NetAddressList addresses(hostname);
			NetAddressList::Iterator iter(addresses);
			NetAddress const* address;

			// Take the first address that's not bad:
			// 采取的第一个无错的地址
			netAddressBits addr = 0;
			while ((address = iter.nextAddress()) != NULL) {
				netAddressBits a = *(netAddressBits*)(address->data());
				if (!badAddressForUs(a)) {
					addr = a;
					break;
				}
			}

			// Assign the address that we found to "fromAddr" (as if the 'loopback' method had worked), to simplify the code below: 
			// 指定的地址，我们发现fromaddr（如可环回法工作），简化代码如下：
			fromAddr.sin_addr.s_addr = addr;
		} while (0);

		// Make sure we have a good address:确保我们有一个良好的地址
		netAddressBits from = fromAddr.sin_addr.s_addr;
		if (badAddressForUs(from)) {
			char tmp[100];
			sprintf(tmp, "This computer has an invalid IP address: %s", AddressString(from).val());
			env.setResultMsg(tmp);
			from = 0;
		}

		ourAddress = from;

		// Use our newly-discovered IP address, and the current time,
		// to initialize the random number generator's seed:
		// 使用我们的新发现的IP地址，和当前时间，初始化随机数生成器的种子：
		struct timeval timeNow;
		gettimeofday(&timeNow, NULL);
		unsigned seed = ourAddress^timeNow.tv_sec^timeNow.tv_usec;
		our_srandom(seed);
	}
	return ourAddress;	// 返回我们的IP地址
}

// 选择随机IPv4 SSM地址
netAddressBits chooseRandomIPv4SSMAddress(UsageEnvironment& env)
{
	// First, a hack to ensure that our random number generator is seeded:
	// 首先，一个黑客，确保我们的随机数生成器种子的初始化：
	(void)ourIPAddress(env);

	// Choose a random address in the range [232.0.1.0, 232.255.255.255)
	// i.e., [0xE8000100, 0xE8FFFFFF)
	netAddressBits const first = 0xE8000100, lastPlus1 = 0xE8FFFFFF;
	netAddressBits const range = lastPlus1 - first;	//范围

	return ntohl(first + ((netAddressBits)our_random()) % range);
}

// 时间戳字符串
char const* timestampString()
{
	struct timeval tvNow;
	gettimeofday(&tvNow, NULL);	//获取当前时间

#if !defined(_WIN32_WCE)	//非wince
	static char timeString[9]; // holds hh:mm:ss plus trailing '\0'
	char const* ctimeResult = ctime((time_t*)&tvNow.tv_sec);
	if (ctimeResult == NULL) {
		sprintf(timeString, "??:??:??");
	}
	else {
		char const* from = &ctimeResult[11];
		int i;
		for (i = 0; i < 8; ++i) {
			timeString[i] = from[i];
		}
		timeString[i] = '\0';
	}
#else
	// WinCE apparently doesn't have "ctime()", so instead, construct
	// a timestamp string just using the integer and fractional parts
	// of "tvNow":
	static char timeString[50];
	sprintf(timeString, "%lu.%06ld", tvNow.tv_sec, tvNow.tv_usec);
#endif

	return (char const*)&timeString;
}

#if defined(__WIN32__) || defined(_WIN32)
// For Windoze, we need to implement our own gettimeofday()
#if !defined(_WIN32_WCE)
#include <sys/timeb.h>
#endif

int gettimeofday(struct timeval* tp, int* /*tz*/) {
#if defined(_WIN32_WCE)
	/* FILETIME of Jan 1 1970 00:00:00. */
	static const unsigned __int64 epoch = 116444736000000000LL;

	FILETIME    file_time;
	SYSTEMTIME  system_time;
	ULARGE_INTEGER ularge;

	GetSystemTime(&system_time);
	SystemTimeToFileTime(&system_time, &file_time);
	ularge.LowPart = file_time.dwLowDateTime;
	ularge.HighPart = file_time.dwHighDateTime;

	tp->tv_sec = (long) ((ularge.QuadPart - epoch) / 10000000L);
	tp->tv_usec = (long) (system_time.wMilliseconds * 1000);
#else
	static LARGE_INTEGER tickFrequency, epochOffset;

	// For our first call, use "ftime()", so that we get a time with a proper epoch.
	// For subsequent calls, use "QueryPerformanceCount()", because it's more fine-grain.
	static Boolean isFirstCall = True;

	LARGE_INTEGER tickNow;
	QueryPerformanceCounter(&tickNow);

	if (isFirstCall) {
		struct timeb tb;
		ftime(&tb);
		tp->tv_sec = tb.time;
		tp->tv_usec = 1000 * tb.millitm;

		// Also get our counter frequency:
		QueryPerformanceFrequency(&tickFrequency);

		// And compute an offset to add to subsequent counter times, so we get a proper epoch:
		epochOffset.QuadPart
			= tb.time*tickFrequency.QuadPart + (tb.millitm*tickFrequency.QuadPart) / 1000 - tickNow.QuadPart;

		isFirstCall = False; // for next time
	}
	else {
		// Adjust our counter time so that we get a proper epoch:
		tickNow.QuadPart += epochOffset.QuadPart;

		tp->tv_sec = (long)(tickNow.QuadPart / tickFrequency.QuadPart);
		tp->tv_usec = (long)(((tickNow.QuadPart % tickFrequency.QuadPart) * 1000000L) / tickFrequency.QuadPart);
	}
#endif
	return 0;
}
#endif
