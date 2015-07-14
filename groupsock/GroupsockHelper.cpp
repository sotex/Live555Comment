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
// Ĭ������£�ʹ��INADDR_ANYΪ���ͺͽ��սӿڣ�
netAddressBits SendingInterfaceAddr = INADDR_ANY;
netAddressBits ReceivingInterfaceAddr = INADDR_ANY;

static void socketErr(UsageEnvironment& env, char const* errorMsg) {
	env.setResultErrMsg(errorMsg);
}

// �����ʱ��Ϊenv.groupsockPriv�������
// ������groupsockPriv�����reuseFlag=0
NoReuse::NoReuse(UsageEnvironment& env)
: fEnv(env) {
	groupsockPriv(fEnv)->reuseFlag = 0;
}

// ��groupsockPriv�����socketTable==NULL
// ������ʱ��Ϊenv.groupsockPriv�ͷŶ���
NoReuse::~NoReuse() {
	groupsockPriv(fEnv)->reuseFlag = 1;
	reclaimGroupsockPriv(fEnv);
}


_groupsockPriv* groupsockPriv(UsageEnvironment& env) {
	if (env.groupsockPriv == NULL) { // We need to create it
		_groupsockPriv* result = new _groupsockPriv;
		result->socketTable = NULL;	//Ĭ��ΪNULL
		result->reuseFlag = 1; // default value => allow reuse of socket numbers
		env.groupsockPriv = result;
	}
	return (_groupsockPriv*)(env.groupsockPriv);
}

void reclaimGroupsockPriv(UsageEnvironment& env) {
	_groupsockPriv* priv = (_groupsockPriv*)(env.groupsockPriv);
	// ������Ա��Ĭ��ֵ��ʱ�򣬽����ͷ�
	if (priv->socketTable == NULL && priv->reuseFlag == 1/*default value*/) {
		// We can delete the structure (to save space); it will get created again, if needed:
		delete priv;
		env.groupsockPriv = NULL;
	}
}

// type socket���ͣ���SOCK_STREAM��SOCK_DGRAM��SOCK_RAW��SOCK_PACKET��SOCK_SEQPACKET��
static int createSocket(int type) {
	// Call "socket()" to create a (IPv4) socket of the specified type.
	// ���á�socket()����һ����IPv4��ָ�����͵��׽��֡�
	// But also set it to have the 'close on exec' property (if we can)
	// �����������С�ִ��execʱ�ر�"���ԣ�������ԣ�
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

// �������ݱ��׽���
int setupDatagramSocket(UsageEnvironment& env, Port port) {
	// ��ʼ������
	if (!initializeWinsockIfNecessary()) {
		socketErr(env, "Failed to initialize 'winsock': ");
		return -1;
	}
	// �������ݱ��׽���
	int newSocket = createSocket(SOCK_DGRAM);
	if (newSocket < 0) {
		socketErr(env, "unable to create datagram socket: ");
		return newSocket;
	}
	// ��ȡenv��groupsockPriv����ʹ�ñ�ʶ
	int reuseFlag = groupsockPriv(env)->reuseFlag;
	// ������Ҫ��Ϊenv�ͷ�groupsockPriv��Աָ�����
	reclaimGroupsockPriv(env);
	/*
	�����������͡�����״̬�׽ӿڵ�����ѡ��ֵ��
	int setsockopt(int sockfd, int level, int optname,const void *optval, socklen_t optlen);
	sockfd����ʶһ���׽ӿڵ������֡�
	level��ѡ���Ĳ�Σ�֧��SOL_SOCKET��IPPROTO_TCP��IPPROTO_IP��IPPROTO_IPV6��
	optname�������õ�ѡ�
	optval��ָ�룬ָ����ѡ������õ���ֵ�Ļ�������
	optlen��optval���������ȡ�

	�ɹ�ִ��ʱ������0��ʧ�ܷ���-1��errno����Ϊ���µ�ĳ��ֵ
	EBADF��sock������Ч���ļ�������
	EFAULT��optvalָ����ڴ沢����Ч�Ľ��̿ռ�
	EINVAL���ڵ���setsockopt()ʱ��optlen��Ч
	ENOPROTOOPT��ָ����Э��㲻��ʶ��ѡ��
	ENOTSOCK��sock�����Ĳ����׽���
	*/
	// �����������ñ��ص�ַ�Ͷ˿ڣ�reuseFlag�������ܴ���ֵ
	if (setsockopt(newSocket, SOL_SOCKET, SO_REUSEADDR,
		(const char*)&reuseFlag, sizeof reuseFlag) < 0) {
		socketErr(env, "setsockopt(SO_REUSEADDR) error: ");
		closeSocket(newSocket);
		return -1;
	}

#if defined(__WIN32__) || defined(_WIN32)
	// Windoze doesn't properly handle SO_REUSEPORT or IP_MULTICAST_LOOP
	// win-doze ����,���������ڲ���ϵͳBUG�ܶ�,���������ٶ���,���������е��Ǻ����DOZE(���˯)
	// Windows�޷���ȷ�Ĵ���SO_REUSEPORT�� IP_MULTICAST_LOOP
#else
#ifdef SO_REUSEPORT		//�ڶ���������ʹ�ö˿ں�������
	if (setsockopt(newSocket, SOL_SOCKET, SO_REUSEPORT,
		(const char*)&reuseFlag, sizeof reuseFlag) < 0) {
		socketErr(env, "setsockopt(SO_REUSEPORT) error: ");
		closeSocket(newSocket);
		return -1;
	}
#endif

#ifdef IP_MULTICAST_LOOP	//�ڶ�����IP�ಥѭ��������
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
	// Windows ��Ҫ�󶨣���ʹ�˿ں���0
	netAddressBits addr = INADDR_ANY;	// ���ð󶨵�ַ����������IP
#if defined(__WIN32__) || defined(_WIN32)
#else
	if (port.num() != 0 || ReceivingInterfaceAddr != INADDR_ANY) {
		//ReceivingInterfaceAddr��һ��ȫ�ֵĶ��壬Ĭ����INADDR_ANY
#endif
		if (port.num() == 0) addr = ReceivingInterfaceAddr;
		// �齨sockaddr_in�ṹ��
		MAKE_SOCKADDR_IN(name, addr, port.num());
		// ��socket�׽ӿں�sockaddr��ַ
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
	// ���÷��Ͷಥ�Ľӿڣ����������Ĭ�ϵģ�
	if (SendingInterfaceAddr != INADDR_ANY) {
		struct in_addr addr;
		addr.s_addr = SendingInterfaceAddr;
		// ���öಥ�ӿ�
		if (setsockopt(newSocket, IPPROTO_IP, IP_MULTICAST_IF,
			(const char*)&addr, sizeof addr) < 0) {
			socketErr(env, "error setting outgoing multicast interface: ");
			closeSocket(newSocket);
			return -1;
		}
	}

	return newSocket;
}

// ����sockΪ������ģʽ
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

// ����sockΪ����ģʽ
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

// ������ʽ�׽���
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
	// ���ݲ��������Ƿ�Ϊ������
	if (makeNonBlocking) {
		if (!makeSocketNonBlocking(newSocket)) {
			socketErr(env, "failed to make non-blocking: ");
			closeSocket(newSocket);
			return -1;
		}
	}

	return newSocket;
}

// ���׽ӿڶ�����
int readSocket(UsageEnvironment& env,
	int socket, unsigned char* buffer, unsigned bufferSize,
struct sockaddr_in& fromAddress) 
{
	SOCKLEN_T addressSize = sizeof fromAddress;
	// ssize_t recvfrom(int sockfd,void *buf,int len,unsigned int flags, struct sockaddr *from,socket_t *fromlen);
	// ��ȡ������ָ����socket����������,�������ݴ����ɲ���bufָ����ڴ�ռ�,����lenΪ�ɽ������ݵ���󳤶ȡ�flagһ������Ϊ0��from����Դ��ַ��fromlen������Դ����
	// �����ȷ���շ��ؽ��յ����ֽ�����ʧ�ܷ���-1.
	int bytesRead = recvfrom(socket, (char*)buffer, bufferSize, 0,
		(struct sockaddr*)&fromAddress,
		&addressSize);
	if (bytesRead < 0) {
		//##### HACK to work around bugs in Linux and Windows:
		int err = env.getErrno();
		if (err == 111 /*ECONNREFUSED (Linux) �������󱻷������ܾ�*/
#if defined(__WIN32__) || defined(_WIN32)
			// What a piece of crap Windows is.  Sometimes
			// recvfrom() returns -1, but with an 'errno' of 0.
			// This appears not to be a real error; just treat
			// it as if it were a read of zero bytes, and hope
			// we don't have to do anything else to 'reset'
			// this alleged error:
			// ������Windows����ʱrecvfrom()����- 1��������errnoΪ0��
			// ���ƺ�����һ�������Ĵ���ֻ�ǰ�������һ����ȡ���ֽڣ���ϣ�����ǲ���Ҫ��ʲô��reset��
			// ����ν�Ĵ���
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

// ���׽ӿ�д����
Boolean writeSocket(UsageEnvironment& env,
	int socket, struct in_addr address, Port port,
	u_int8_t ttlArg,
	unsigned char* buffer, unsigned bufferSize) 
{
	do {
		if (ttlArg != 0) {
			// TTL�� Time To Live����д�����ֶ�ָ��IP����·��������֮ǰ����ͨ�����������������
			// TTL�����ֵ��255��TTL��һ���Ƽ�ֵ��64��
			// Before sending, set the socket's TTL:����ǰ����socket TTL
#if defined(__WIN32__) || defined(_WIN32)
#define TTL_TYPE int
#else
#define TTL_TYPE u_int8_t
#endif
			TTL_TYPE ttl = (TTL_TYPE)ttlArg;
			// ���öಥTTLֵ
			if (setsockopt(socket, IPPROTO_IP, IP_MULTICAST_TTL,
				(const char*)&ttl, sizeof ttl) < 0) {
				socketErr(env, "setsockopt(IP_MULTICAST_TTL) error: ");
				break;
			}
		}

		MAKE_SOCKADDR_IN(dest, address.s_addr, port.num());
		// ��buffer�е����ݾ�socket���͵�dest
		int bytesSent = sendto(socket, (char*)buffer, bufferSize, 0,
			(struct sockaddr*)&dest, sizeof dest);
		// ���ͷ�ȫ�����ͳɹ�
		if (bytesSent != (int)bufferSize) {
			char tmpBuf[100];
			sprintf(tmpBuf, "writeSocket(%d), sendTo() error: wrote %d bytes instead of %u: ", socket, bytesSent, bufferSize);
			socketErr(env, tmpBuf);
			break;
		}

		return True;	//���ͳɹ�����
	} while (0);

	return False;	//ʧ�ܷ���
}

// ��ȡbufferSize	bufOptName
// SO_RCVBUF���ջ�������С SO_SNDBUF���ͻ�������С
// SO_RCVLOWAT���ջ��������� SO_SNDLOWAT���ͻ���������
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


// ��ȡ���ͻ�����size
unsigned getSendBufferSize(UsageEnvironment& env, int socket) {
	return getBufferSize(env, SO_SNDBUF, socket);
}
// ��ȡ���ջ�����size
unsigned getReceiveBufferSize(UsageEnvironment& env, int socket) {
	return getBufferSize(env, SO_RCVBUF, socket);
}

static unsigned setBufferTo(UsageEnvironment& env, int bufOptName,
	int socket, unsigned requestedSize) {
	SOCKLEN_T sizeSize = sizeof requestedSize;
	// ���û�������С
	setsockopt(socket, SOL_SOCKET, bufOptName, (char*)&requestedSize, sizeSize);

	// Get and return the actual, resulting buffer size:
	// ��ȡ������ʵ�ʵģ���������С
	return getBufferSize(env, bufOptName, socket);
}

// ���÷��ͻ�����size
unsigned setSendBufferTo(UsageEnvironment& env,
	int socket, unsigned requestedSize) {
	return setBufferTo(env, SO_SNDBUF, socket, requestedSize);
}

//���ý��ջ�����size
unsigned setReceiveBufferTo(UsageEnvironment& env,
	int socket, unsigned requestedSize) {
	return setBufferTo(env, SO_RCVBUF, socket, requestedSize);
}

static unsigned increaseBufferTo(UsageEnvironment& env, int bufOptName,
	int socket, unsigned requestedSize) {
	// First, get the current buffer size.  If it's already at least
	// as big as what we're requesting, do nothing.
	// ��ȡ��ǰ�Ļ�������С
	unsigned curSize = getBufferSize(env, bufOptName, socket);

	// Next, try to increase the buffer to the requested size,
	// or to some smaller size, if that's not possible:
	// ��ǰ��С��Ҫ�ﵽ��
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

// �������ͻ�����size
unsigned increaseSendBufferTo(UsageEnvironment& env,
	int socket, unsigned requestedSize) {
	return increaseBufferTo(env, SO_SNDBUF, socket, requestedSize);
}
// �������ջ�����size
unsigned increaseReceiveBufferTo(UsageEnvironment& env,
	int socket, unsigned requestedSize) {
	return increaseBufferTo(env, SO_RCVBUF, socket, requestedSize);
}

// �ڱ���ReceivingInterfaceAddr�ϼ���һ���ಥ���ַgroupAddress
Boolean socketJoinGroup(UsageEnvironment& env, int socket,
	netAddressBits groupAddress)
{	// ������Ƕಥ��ַ��ֱ�Ӻ���
	if (!IsMulticastAddress(groupAddress)) return True; // ignore this case
	/*	struct ip_mreq {
			struct in_addr  imr_multiaddr;  // �ಥ��IP��ַ
		struct in_addr  imr_interface;		// ����IP��ַ�Ľӿ�
		};*/
	struct ip_mreq imr;
	imr.imr_multiaddr.s_addr = groupAddress;
	imr.imr_interface.s_addr = ReceivingInterfaceAddr;
	// ��ָ���Ľӿڼ���ಥ
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

// �׽����뿪�ಥ��
Boolean socketLeaveGroup(UsageEnvironment&, int socket,
	netAddressBits groupAddress) 
{
	if (!IsMulticastAddress(groupAddress)) return True; // ignore this case

	struct ip_mreq imr;
	imr.imr_multiaddr.s_addr = groupAddress;
	imr.imr_interface.s_addr = ReceivingInterfaceAddr;
	//�뿪���ؽӿ��ϲ���Դ�Ķಥ��
	if (setsockopt(socket, IPPROTO_IP, IP_DROP_MEMBERSHIP,
		(const char*)&imr, sizeof (struct ip_mreq)) < 0) {
		return False;
	}

	return True;
}

// The source-specific join/leave operations require special setsockopt()
// commands, and a special structure (ip_mreq_source).  If the include files
// didn't define these, we do so here:
// �ض�Դ�ļ���/�뿪������Ҫ�����setsockopt()�����һ������Ľṹ�壨ip_mreq_source����
// ����ļ�û�ж�����Щ��������������
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

// ��ָ���ı��ؽӿ��ϼ���һ���ض�Դ�Ķಥ��
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

// �뿪һ���ض�Դ�Ķಥ��
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

// ��ȡsocket�ı��ع����˿�
static Boolean getSourcePort0(int socket, portNumBits& resultPortNum/*host order*/)
{
	sockaddr_in test; test.sin_port = 0;
	SOCKLEN_T len = sizeof test;
	// getsockname�������ڻ�ȡ��ĳ���׽��ֹ����ı���Э���ַ 
	if (getsockname(socket, (struct sockaddr*)&test, &len) < 0) return False;

	resultPortNum = ntohs(test.sin_port);
	return True;
}

// ��ȡsocket���ع����˿ڣ�����û�У������bind��ϵͳ�ں˷���һ��
Boolean getSourcePort(UsageEnvironment& env, int socket, Port& port) 
{
	portNumBits portNum = 0;
	if (!getSourcePort0(socket, portNum) || portNum == 0) {
		// Hack - call bind(), then try again:
		MAKE_SOCKADDR_IN(name, INADDR_ANY, 0);
		// socketΪ�������ض˿ڣ�����bind����
		bind(socket, (struct sockaddr*)&name, sizeof name);

		if (!getSourcePort0(socket, portNum) || portNum == 0) {
			socketErr(env, "getsockname() error: ");
			return False;
		}
	}

	port = Port(portNum);
	return True;
}

// IP��ַ������
static Boolean badAddressForUs(netAddressBits addr) 
{
	// Check for some possible erroneous addresses:
	// ���һЩ���ܵĵ�ַ����
	netAddressBits nAddr = htonl(addr);
	return (nAddr == 0x7F000001 /* 127.0.0.1 */
		|| nAddr == 0
		|| nAddr == (netAddressBits)(~0));
}

Boolean loopbackWorks = 1;	//���ع�����ʶ

netAddressBits ourIPAddress(UsageEnvironment& env) 
{
	static netAddressBits ourAddress = 0;
	int sock = -1;
	struct in_addr testAddr;

	if (ourAddress == 0) {
		// We need to find our source address
		// ������Ҫ�ҵ����ǵ�Դ��ַ
		struct sockaddr_in fromAddr;
		fromAddr.sin_addr.s_addr = 0;

		// Get our address by sending a (0-TTL) multicast packet,
		// receiving it, and looking at the source address used.
		// (This is kinda bogus, but it provides the best guarantee
		// that other nodes will think our address is the same as we do.)
		do {
			loopbackWorks = 0; // until we learn otherwiseֱ�����ǻ�֪,����...

			testAddr.s_addr = our_inet_addr("228.67.43.91"); // arbitrary����
			Port testPort(15947); // ditto
			// �������ݱ�socket
			sock = setupDatagramSocket(env, testPort);
			if (sock < 0) break;
			// ��������Դ�ಥ��"228.67.43.91"
			if (!socketJoinGroup(env, sock, testAddr.s_addr)) break;

			unsigned char testString[] = "hostIdTest";
			unsigned testStringLength = sizeof testString;
			// ��socketд������"hostIdTest"
			if (!writeSocket(env, sock, testAddr, testPort, 0,
				testString, testStringLength)) break;

			// Block until the socket is readable (with a 5-second timeout):
			// ����ֱ���׽��ֿɶ���5�볬ʱ����
			fd_set rd_set;
			FD_ZERO(&rd_set);
			FD_SET((unsigned)sock, &rd_set);
			const unsigned numFds = sock + 1;
			struct timeval timeout;
			timeout.tv_sec = 5;
			timeout.tv_usec = 0;
			// �����ȴ�sock��Ϊ�ɶ�
			int result = select(numFds, &rd_set, NULL, NULL, &timeout);
			if (result <= 0) break;

			unsigned char readBuffer[20];
			// ��sock��ȡ���ݣ�������Դ������ַ��fromAddr
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
			// �뿪�ಥ��
			socketLeaveGroup(env, sock, testAddr.s_addr);
			closeSocket(sock);
		}

		// ǰ��do while����Ĳ���û�ж��ɹ�
		if (!loopbackWorks) do {
			// We couldn't find our address using multicast loopback,
			// so try instead to look it up directly - by first getting our host name, and then resolving this host name
			char hostname[100];
			hostname[0] = '\0';
			// �ú����ѱ����������������hostname����ָ���Ļ������С�
			int result = gethostname(hostname, sizeof hostname);
			if (result != 0 || hostname[0] == '\0') {
				env.setResultErrMsg("initial gethostname() failed");
				break;
			}

			// Try to resolve "hostname" to an IP address:
			// ���Խ���hostname��IP��ַ
			NetAddressList addresses(hostname);
			NetAddressList::Iterator iter(addresses);
			NetAddress const* address;

			// Take the first address that's not bad:
			// ��ȡ�ĵ�һ���޴�ĵ�ַ
			netAddressBits addr = 0;
			while ((address = iter.nextAddress()) != NULL) {
				netAddressBits a = *(netAddressBits*)(address->data());
				if (!badAddressForUs(a)) {
					addr = a;
					break;
				}
			}

			// Assign the address that we found to "fromAddr" (as if the 'loopback' method had worked), to simplify the code below: 
			// ָ���ĵ�ַ�����Ƿ���fromaddr����ɻ��ط����������򻯴������£�
			fromAddr.sin_addr.s_addr = addr;
		} while (0);

		// Make sure we have a good address:ȷ��������һ�����õĵ�ַ
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
		// ʹ�����ǵ��·��ֵ�IP��ַ���͵�ǰʱ�䣬��ʼ������������������ӣ�
		struct timeval timeNow;
		gettimeofday(&timeNow, NULL);
		unsigned seed = ourAddress^timeNow.tv_sec^timeNow.tv_usec;
		our_srandom(seed);
	}
	return ourAddress;	// �������ǵ�IP��ַ
}

// ѡ�����IPv4 SSM��ַ
netAddressBits chooseRandomIPv4SSMAddress(UsageEnvironment& env)
{
	// First, a hack to ensure that our random number generator is seeded:
	// ���ȣ�һ���ڿͣ�ȷ�����ǵ���������������ӵĳ�ʼ����
	(void)ourIPAddress(env);

	// Choose a random address in the range [232.0.1.0, 232.255.255.255)
	// i.e., [0xE8000100, 0xE8FFFFFF)
	netAddressBits const first = 0xE8000100, lastPlus1 = 0xE8FFFFFF;
	netAddressBits const range = lastPlus1 - first;	//��Χ

	return ntohl(first + ((netAddressBits)our_random()) % range);
}

// ʱ����ַ���
char const* timestampString()
{
	struct timeval tvNow;
	gettimeofday(&tvNow, NULL);	//��ȡ��ǰʱ��

#if !defined(_WIN32_WCE)	//��wince
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
