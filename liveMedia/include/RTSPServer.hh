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
// "liveMedia"
// Copyright (c) 1996-2012 Live Networks, Inc.  All rights reserved.
// A RTSP server
// C++ header

#ifndef _RTSP_SERVER_HH
#define _RTSP_SERVER_HH

#ifndef _SERVER_MEDIA_SESSION_HH
#include "ServerMediaSession.hh"
#endif
#ifndef _NET_ADDRESS_HH
#include <NetAddress.hh>
#endif
#ifndef _DIGEST_AUTHENTICATION_HH
#include "DigestAuthentication.hh"
#endif

// A data structure used for optional user/password authentication:
// 用于可选的用户/密码认证的数据结构：
// 用户认证数据库
class UserAuthenticationDatabase {
public:
	UserAuthenticationDatabase(char const* realm = NULL,
		Boolean passwordsAreMD5 = False);
	// If "passwordsAreMD5" is True, then each password stored into, or removed from,
	// the database is actually the value computed
	// by md5(<username>:<realm>:<actual-password>)
	// 如果“passwordsAreMD5”为真，那么每个密码存入，或从中删除，数据库实际上是计算md5(<username>:<realm>:<actual-password>)的值

	virtual ~UserAuthenticationDatabase();
	// 添加用户记录
	virtual void addUserRecord(char const* username, char const* password);
	// 移除用户记录
	virtual void removeUserRecord(char const* username);
	// 查找用户记录
	virtual char const* lookupPassword(char const* username);
	// returns NULL if the user name was not present

	char const* realm() { return fRealm; }
	Boolean passwordsAreMD5() { return fPasswordsAreMD5; }

protected:
	HashTable* fTable;
	char* fRealm;
	Boolean fPasswordsAreMD5;
};

#define RTSP_BUFFER_SIZE 10000 // for incoming requests, and outgoing responses

class RTSPServer : public Medium {
public:
	static RTSPServer* createNew(UsageEnvironment& env, Port ourPort = 554,
		UserAuthenticationDatabase* authDatabase = NULL,
		unsigned reclamationTestSeconds = 65);
	// If ourPort.num() == 0, we'll choose the port number 
	// Note: The caller is responsible for reclaiming "authDatabase"
	// 注：调用者负责回收“authDatabase”
	// If "reclamationTestSeconds" > 0, then the "RTSPClientSession" state for
	//     each client will get reclaimed (and the corresponding RTP stream(s)
	//     torn down) if no RTSP commands - or RTCP "RR" packets - from the
	//     client are received in at least "reclamationTestSeconds" seconds.

	static Boolean lookupByName(UsageEnvironment& env, char const* name,
		RTSPServer*& resultServer);

	void addServerMediaSession(ServerMediaSession* serverMediaSession);
	virtual ServerMediaSession* lookupServerMediaSession(char const* streamName);
	void removeServerMediaSession(ServerMediaSession* serverMediaSession);
	void removeServerMediaSession(char const* streamName);

	char* rtspURL(ServerMediaSession const* serverMediaSession, int clientSocket = -1) const;
	// returns a "rtsp://" URL that could be used to access the
	// specified session (which must already have been added to
	// us using "addServerMediaSession()".
	// This string is dynamically allocated; caller should delete[]
	// (If "clientSocket" is non-negative, then it is used (by calling "getsockname()") to determine
	//  the IP address to be used in the URL.)
	//返回"的rtsp://"，可以被用于访问指定的会话（其必须已经使用"addServerMediaSession()"已被添加到我们的URL。
	//这个字符串是动态分配的;调用者应delete[]（如果"ClientSocket"是非负的，则它被使用（通过调用"getsockname()"），以确定该IP地址在URL中使用。）

	// RTSP URL前缀
	char* rtspURLPrefix(int clientSocket = -1) const;
	// like "rtspURL()", except that it returns just the common prefix used by
	// each session's "rtsp://" URL.
	// This string is dynamically allocated; caller should delete[]
	//类似"rtspURL()"，但它只是返回由每个会话的公共前缀"rtsp://"的URL。
	//这个字符串是动态分配的;调用者应该delete []

	// 设置新的用户认证数据库，返回旧的数据库
	UserAuthenticationDatabase* setAuthenticationDatabase(UserAuthenticationDatabase* newDB);
	// Changes the server's authentication database to "newDB", returning a pointer to the old database (if there was one).
	// "newDB" may be NULL (you can use this to disable authentication at runtime, if desired).

	// 创建一个socket，绑定httpPort和INADDR_ANY，添加到envir().taskScheduler()后台处理传入连接
	Boolean setUpTunnelingOverHTTP(Port httpPort);
	// (Attempts to) enable RTSP-over-HTTP tunneling on the specified port.
	//（试图）使RTSP在指定的端口上的HTTP隧道传输。
	// Returns True iff the specified port can be used in this way (i.e., it's not already being used for a separate HTTP server).
	//返回true如果指定端口可以以这种方式使用（即，它不是已被用于一个单独的HTTP服务器）。
	// Note: RTSP-over-HTTP tunneling is described in http://developer.apple.com/quicktime/icefloe/dispatch028.html
	//注意：RTSP通过HTTP隧道传输的描述http://developer.apple.com/quicktime/icefloe/dispatch028.html

	portNumBits httpServerPortNum() const; // in host byte order.  (Returns 0 if not present.)在主机字节顺序(返回0，如果不存在)

protected:
	RTSPServer(UsageEnvironment& env,
		int ourSocket, Port ourPort,
		UserAuthenticationDatabase* authDatabase,
		unsigned reclamationTestSeconds);
	// called only by createNew();
	// 仅被createNew调用

	virtual ~RTSPServer();

	// 创建socket套接字(流式、绑定ourPort或内核分配端口、50K大缓冲区)
	static int setUpOurSocket(UsageEnvironment& env, Port& ourPort);
	// 特定客户端访问检查
	virtual Boolean specialClientAccessCheck(int clientSocket, struct sockaddr_in& clientAddr,
		char const* urlSuffix);
	// a hook that allows subclassed servers to do server-specific access checking
	//一个钩子，让子类的服务器做服务器特定的访问检查
	// on each client (e.g., based on client IP address), without using
	// digest authentication.
	//每个客户端上（例如，基于客户端的IP地址），而无需使用摘要身份验证。

private: // redefined virtual functions
	virtual Boolean isRTSPServer() const;

public: // should be protected, but some old compilers complain otherwise
	// The state of each individual session handled by a RTSP server:
	// 通过RTSP服务器处理的每个单独的会话的状态：
	class RTSPClientSession {
	public:
		RTSPClientSession(RTSPServer& ourServer, unsigned sessionId,
			int clientSocket, struct sockaddr_in clientAddr);
		virtual ~RTSPClientSession();
	protected:
		// Make the handler functions for each command virtual, to allow subclasses to redefine them:
		virtual void handleCmd_bad(char const* cseq);
		virtual void handleCmd_notSupported(char const* cseq);
		virtual void handleCmd_notFound(char const* cseq);
		virtual void handleCmd_unsupportedTransport(char const* cseq);
		virtual void handleCmd_OPTIONS(char const* cseq);
		virtual void handleCmd_DESCRIBE(char const* cseq,
			char const* urlPreSuffix, char const* urlSuffix,
			char const* fullRequestStr);
		virtual void handleCmd_SETUP(char const* cseq,
			char const* urlPreSuffix, char const* urlSuffix,
			char const* fullRequestStr);
		virtual void handleCmd_withinSession(char const* cmdName,
			char const* urlPreSuffix, char const* urlSuffix,
			char const* cseq, char const* fullRequestStr);
		virtual void handleCmd_TEARDOWN(ServerMediaSubsession* subsession,
			char const* cseq);
		virtual void handleCmd_PLAY(ServerMediaSubsession* subsession,
			char const* cseq, char const* fullRequestStr);
		virtual void handleCmd_PAUSE(ServerMediaSubsession* subsession,
			char const* cseq);
		virtual void handleCmd_GET_PARAMETER(ServerMediaSubsession* subsession,
			char const* cseq, char const* fullRequestStr);
		virtual void handleCmd_SET_PARAMETER(ServerMediaSubsession* subsession,
			char const* cseq, char const* fullRequestStr);
		// Support for optional RTSP-over-HTTP tunneling:支持可选的RTSP-over-HTTP隧道
		virtual Boolean parseHTTPRequestString(char* resultCmdName, unsigned resultCmdNameMaxSize,
			char* urlSuffix, unsigned urlSuffixMaxSize,
			char* sessionCookie, unsigned sessionCookieMaxSize,
			char* acceptStr, unsigned acceptStrMaxSize);
		virtual void handleHTTPCmd_notSupported();
		virtual void handleHTTPCmd_notFound();
		virtual void handleHTTPCmd_TunnelingGET(char const* sessionCookie);
		virtual Boolean handleHTTPCmd_TunnelingPOST(char const* sessionCookie, unsigned char const* extraData, unsigned extraDataSize);
		virtual void handleHTTPCmd_StreamingGET(char const* urlSuffix, char const* fullRequestStr);
	protected:
		UsageEnvironment& envir() { return fOurServer.envir(); }
		void closeSockets();
		void reclaimStreamStates();
		void resetRequestBuffer();
		Boolean authenticationOK(char const* cmdName, char const* cseq,
			char const* urlSuffix,
			char const* fullRequestStr);
		Boolean isMulticast() const { return fIsMulticast; }
		static void incomingRequestHandler(void*, int /*mask*/);
		void incomingRequestHandler1();
		static void handleAlternativeRequestByte(void*, u_int8_t requestByte);
		void handleAlternativeRequestByte1(u_int8_t requestByte);
		void handleRequestBytes(int newBytesRead);
		void noteLiveness();
		static void noteClientLiveness(RTSPClientSession* clientSession);
		static void livenessTimeoutTask(RTSPClientSession* clientSession);
		void changeClientInputSocket(int newSocketNum, unsigned char const* extraData, unsigned extraDataSize);
	protected:
		RTSPServer& fOurServer;	//客户端会话要绑定一个服务端对象
		unsigned fOurSessionId;	//会话ID
		ServerMediaSession* fOurServerMediaSession;
		int fClientInputSocket, fClientOutputSocket;
		struct sockaddr_in fClientAddr;
		char* fSessionCookie; // used for optional RTSP-over-HTTP tunneling
		TaskToken fLivenessCheckTask;
		unsigned char fRequestBuffer[RTSP_BUFFER_SIZE];
		unsigned fRequestBytesAlreadySeen, fRequestBufferBytesLeft;
		unsigned char* fLastCRLF;
		unsigned fBase64RemainderCount; // used for optional RTSP-over-HTTP tunneling (possible values: 0,1,2,3)用于可选的RTSP-over-HTTP隧道（可能的值：0,1,2,3）
		unsigned char fResponseBuffer[RTSP_BUFFER_SIZE];	//响应缓冲区
		Boolean fIsMulticast, fSessionIsActive, fStreamAfterSETUP;
		Authenticator fCurrentAuthenticator; // used if access control is needed
		unsigned char fTCPStreamIdCount; // used for (optional) RTP/TCP
		unsigned fNumStreamStates;
		struct streamState {
			ServerMediaSubsession* subsession;
			void* streamToken;
		} *fStreamStates;
		unsigned fRecursionCount;
	};

protected:
	// If you subclass "RTSPClientSession", then you should also redefine this virtual function in order
	// to create new objects of your subclass:
	// 如果你继承“RTSPClientSession”，那么你也应该重新定义为了这个虚函数来创建子类的新对象
	virtual RTSPClientSession*
		createNewClientSession(unsigned sessionId, int clientSocket, struct sockaddr_in clientAddr);

	// An iterator over our "ServerMediaSession" objects:
	class ServerMediaSessionIterator {
	public:
		ServerMediaSessionIterator(RTSPServer& server);	//绑定的是RTSPServer的fServerMediaSessions
		virtual ~ServerMediaSessionIterator();
		ServerMediaSession* next();	//注意返回类型
	private:
		HashTable::Iterator* fOurIterator;
		ServerMediaSession* fNextPtr;
	};

private:
	// 传入的RTSP连接处理
	static void incomingConnectionHandlerRTSP(void*, int /*mask*/);
	void incomingConnectionHandlerRTSP1();
	// 传入的HTTP连接处理
	static void incomingConnectionHandlerHTTP(void*, int /*mask*/);
	void incomingConnectionHandlerHTTP1();
	// 传入连接处理
	void incomingConnectionHandler(int serverSocket);

private:
	friend class RTSPClientSession;
	friend class ServerMediaSessionIterator;
	int		fRTSPServerSocket;
	Port	fRTSPServerPort;
	int		fHTTPServerSocket; // for optional RTSP-over-HTTP tunneling
	Port	fHTTPServerPort; // ditto
	HashTable* fClientSessionsForHTTPTunneling; // ditto (maps 'session cookie' strings to "RTSPClientSession"s)
	UserAuthenticationDatabase* fAuthDB;	//用户认证数据库
	unsigned fReclamationTestSeconds;	//设置最大等待回收连接时间reclamationTestSeconds，超过这个时间从客户端没有RTSP命令或者RTSP的RR包则收回其RTSPClientSession
	HashTable* fServerMediaSessions;
};

#endif
