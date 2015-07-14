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
// A data structure that represents a session that consists of
// potentially multiple (audio and/or video) sub-sessions
// (This data structure is used for media *streamers* - i.e., servers.
//  For media receivers, use "MediaSession" instead.)
// C++ header

#ifndef _SERVER_MEDIA_SESSION_HH
#define _SERVER_MEDIA_SESSION_HH

#ifndef _MEDIA_HH
#include "Media.hh"
#endif
#ifndef _FRAMED_SOURCE_HH
#include "FramedSource.hh"
#endif
#ifndef _GROUPEID_HH
#include "GroupEId.hh"
#endif
#ifndef _RTP_INTERFACE_HH
#include "RTPInterface.hh" // for ServerRequestAlternativeByteHandler
#endif

class ServerMediaSubsession; // forward

// 服务器媒体会话(链表，节点类型ServerMediaSubsession*)
class ServerMediaSession : public Medium {
public:
	static ServerMediaSession* createNew(UsageEnvironment& env,
		char const* streamName = NULL,
		char const* info = NULL,
		char const* description = NULL,
		Boolean isSSM = False,
		char const* miscSDPLines = NULL);

	virtual ~ServerMediaSession();

	static Boolean lookupByName(UsageEnvironment& env,
		char const* mediumName,
		ServerMediaSession*& resultSession);

	char* generateSDPDescription(); // based on the entire session基于整个会话
	// Note: The caller is responsible for freeing the returned string调用者负责释放返回的字符串

	char const* streamName() const { return fStreamName; }

	// 添加子会话
	Boolean addSubsession(ServerMediaSubsession* subsession);
	// 测试比例因子
	void testScaleFactor(float& scale); // sets "scale" to the actual supported scale设置"scale"到实际的scale
	// 持续时间
	float duration() const;
	// a result == 0 means an unbounded session (the default)
	// a result < 0 means: subsession durations differ; the result is -(the largest)
	// a result > 0 means: this is the duration of a bounded session

	// 获取引用计数
	unsigned referenceCount() const { return fReferenceCount; }
	// 自增引用计数
	void incrementReferenceCount() { ++fReferenceCount; }
	// 自减引用计数
	void decrementReferenceCount() { if (fReferenceCount > 0) --fReferenceCount; }
	Boolean& deleteWhenUnreferenced() { return fDeleteWhenUnreferenced; }

protected:
	ServerMediaSession(UsageEnvironment& env, char const* streamName,
		char const* info, char const* description,
		Boolean isSSM, char const* miscSDPLines);
	// called only by "createNew()" 仅被createNew()调用

private: // redefined virtual functions
	virtual Boolean isServerMediaSession() const;

private:
	Boolean fIsSSM;	//是特定源多播？

	// Linkage fields:连接字段
	friend class ServerMediaSubsessionIterator;	//子会话对象迭代器
	ServerMediaSubsession* fSubsessionsHead;	//子会话头指针
	ServerMediaSubsession* fSubsessionsTail;	//子会话尾指针
	unsigned fSubsessionCounter;	//子会话计数

	char* fStreamName;	//流名称
	char* fInfoSDPString;	//SDP信息字符串(SDP:会话描述协议Session Description Protocol)
	char* fDescriptionSDPString;//SDP描述字符串
	char* fMiscSDPLines;	//其它SDP线路
	struct timeval fCreationTime;	//创建时间
	unsigned fReferenceCount;	//引用计数
	Boolean fDeleteWhenUnreferenced;	//删除时解引用
};

//======================================================================

class ServerMediaSubsessionIterator {
public:
	ServerMediaSubsessionIterator(ServerMediaSession& session);
	virtual ~ServerMediaSubsessionIterator();

	ServerMediaSubsession* next(); // NULL if none
	void reset();

private:
	ServerMediaSession& fOurSession;	// 绑定链表
	ServerMediaSubsession* fNextPtr;	// 指向节点
};

//========================================================================


class ServerMediaSubsession : public Medium {
public:
	virtual ~ServerMediaSubsession();

	unsigned trackNumber() const { return fTrackNumber; }
	// 设置追踪ID
	char const* trackId();
	virtual char const* sdpLines() = 0;
	// 获取流参数
	virtual void getStreamParameters(unsigned clientSessionId, // in
		netAddressBits clientAddress, // in
		Port const& clientRTPPort, // in
		Port const& clientRTCPPort, // in
		int tcpSocketNum, // in (-1 means use UDP, not TCP)
		unsigned char rtpChannelId, // in (used if TCP)
		unsigned char rtcpChannelId, // in (used if TCP)
		netAddressBits& destinationAddress, // in out
		u_int8_t& destinationTTL, // in out
		Boolean& isMulticast, // out
		Port& serverRTPPort, // out
		Port& serverRTCPPort, // out
		void*& streamToken // out
		) = 0;

	// 开始流
	virtual void startStream(unsigned clientSessionId, void* streamToken,
		TaskFunc* rtcpRRHandler,
		void* rtcpRRHandlerClientData,
		unsigned short& rtpSeqNum,
		unsigned& rtpTimestamp,
		ServerRequestAlternativeByteHandler* serverRequestAlternativeByteHandler,
		void* serverRequestAlternativeByteHandlerClientData) = 0;

	// 暂停流
	virtual void pauseStream(unsigned clientSessionId, void* streamToken);
	// 跳寻流位置
	virtual void seekStream(unsigned clientSessionId, void* streamToken, double& seekNPT, double streamDuration, u_int64_t& numBytes);
	// "streamDuration", if >0.0, specifies how much data to stream, past "seekNPT".  (If <=0.0, all remaining data is streamed.)
	// 流持续时间(streamDuration)如果大于0，指定跳过"seekNPT"数据到流。如果小于0，所有剩余数据是流
	// "numBytes" returns the size (in bytes) of the data to be streamed, or 0 if unknown or unlimited.

	// 设置流缩放比例
	virtual void setStreamScale(unsigned clientSessionId, void* streamToken, float scale);
	// 获取流源
	virtual FramedSource* getStreamSource(void* streamToken);
	// 删除流
	virtual void deleteStream(unsigned clientSessionId, void*& streamToken);
	// 测试缩放因子
	virtual void testScaleFactor(float& scale); // sets "scale" to the actual supported scale
	// 持续时间
	virtual float duration() const;
	// returns 0 for an unbounded session (the default)
	// returns > 0 for a bounded session

	// The following may be called by (e.g.) SIP servers, for which the
	// address and port number fields in SDP descriptions need to be non-zero:
	// 设置SDP(会话描述协议)服务器地址和端口
	void setServerAddressAndPortForSDP(netAddressBits addressBits,
		portNumBits portBits);

protected: // we're a virtual base class
	ServerMediaSubsession(UsageEnvironment& env);
	// 获取SDP范围Line字符串("a=range:npt=0-%.3f\r\n",Duration)
	char const* rangeSDPLine() const;
	// returns a string to be delete[]d

	ServerMediaSession* fParentSession;		// 父会话(链表this)
	netAddressBits fServerAddressForSDP;	// SDP(会话描述协议)服务ip地址
	portNumBits fPortNumForSDP;				// SDP端口

private:
	friend class ServerMediaSession;
	friend class ServerMediaSubsessionIterator;
	ServerMediaSubsession* fNext;	// 下一个子会话

	unsigned fTrackNumber;	// 追踪号 within an enclosing ServerMediaSession封闭在内
	char const* fTrackId;	// 追踪ID
};

#endif
