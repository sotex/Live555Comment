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
// A 'ServerMediaSubsession' object that creates new, unicast, "RTPSink"s
// on demand.
// C++ header

#ifndef _ON_DEMAND_SERVER_MEDIA_SUBSESSION_HH
#define _ON_DEMAND_SERVER_MEDIA_SUBSESSION_HH

#ifndef _SERVER_MEDIA_SESSION_HH
#include "ServerMediaSession.hh"
#endif
#ifndef _RTP_SINK_HH
#include "RTPSink.hh"
#endif
#ifndef _BASIC_UDP_SINK_HH
#include "BasicUDPSink.hh"
#endif
#ifndef _RTCP_HH
#include "RTCP.hh"
#endif

// 点播服务器媒体子会话
class OnDemandServerMediaSubsession : public ServerMediaSubsession {
protected: // we're a virtual base class
	OnDemandServerMediaSubsession(UsageEnvironment& env, Boolean reuseFirstSource,
		portNumBits initialPortNum = 6970);
	virtual ~OnDemandServerMediaSubsession();

protected: // redefined virtual functions
	virtual char const* sdpLines();
	virtual void getStreamParameters(unsigned clientSessionId,
		netAddressBits clientAddress,
		Port const& clientRTPPort,
		Port const& clientRTCPPort,
		int tcpSocketNum,
		unsigned char rtpChannelId,
		unsigned char rtcpChannelId,
		netAddressBits& destinationAddress,
		u_int8_t& destinationTTL,
		Boolean& isMulticast,
		Port& serverRTPPort,
		Port& serverRTCPPort,
		void*& streamToken);
	// 开始流
	virtual void startStream(unsigned clientSessionId, void* streamToken,
		TaskFunc* rtcpRRHandler,
		void* rtcpRRHandlerClientData,
		unsigned short& rtpSeqNum,
		unsigned& rtpTimestamp,
		ServerRequestAlternativeByteHandler* serverRequestAlternativeByteHandler,
		void* serverRequestAlternativeByteHandlerClientData);
	// 暂停(挂起)流
	virtual void pauseStream(unsigned clientSessionId, void* streamToken);
	// 跳寻流
	virtual void seekStream(unsigned clientSessionId, void* streamToken, double& seekNPT, double streamDuration, u_int64_t& numBytes);
	// 设置流缩放
	virtual void setStreamScale(unsigned clientSessionId, void* streamToken, float scale);
	// 获取流源
	virtual FramedSource* getStreamSource(void* streamToken);
	// 删除流
	virtual void deleteStream(unsigned clientSessionId, void*& streamToken);

protected: // new virtual functions, possibly redefined by subclasses新的虚函数，可能通过子类重新定义
	virtual char const* getAuxSDPLine(RTPSink* rtpSink,
		FramedSource* inputSource);
	virtual void seekStreamSource(FramedSource* inputSource, double& seekNPT, double streamDuration, u_int64_t& numBytes);
	// "streamDuration", if >0.0, specifies how much data to stream, past "seekNPT".  (If <=0.0, all remaining data is streamed.)
	virtual void setStreamSourceScale(FramedSource* inputSource, float scale);
	virtual void closeStreamSource(FramedSource *inputSource);

protected: // new virtual functions, defined by all subclasses
	virtual FramedSource* createNewStreamSource(unsigned clientSessionId,
		unsigned& estBitrate) = 0;
	// "estBitrate" is the stream's estimated bitrate, in kbps
	virtual RTPSink* createNewRTPSink(Groupsock* rtpGroupsock,
		unsigned char rtpPayloadTypeIfDynamic,
		FramedSource* inputSource) = 0;

private:
	void setSDPLinesFromRTPSink(RTPSink* rtpSink, FramedSource* inputSource,
		unsigned estBitrate);
	// used to implement "sdpLines()"

protected:
	char* fSDPLines;	//会话描述协议行

private:
	Boolean fReuseFirstSource;	// 重用第一个源
	portNumBits fInitialPortNum;	//初始端口号
	HashTable* fDestinationsHashTable; // 目标地址哈希表 indexed by client session id通过客户端的会话ID索引
	void* fLastStreamToken;		// 最近流标识
	char fCNAME[100]; // for RTCP
	friend class StreamState;
};

//========================================================================

// A class that represents the state of an ongoing stream.  This is used only internally, in the implementation of
// "OnDemandServerMediaSubsession", but we expose the definition here, in case subclasses of "OnDemandServerMediaSubsession"
// want to access it.
// 一个代表一个持续流状态的类。这个仅在内部使用，在"OnDemandServerMediaSubsession"中实现。
// 但是在这里暴露出它的定义，在"OnDemandServerMediaSubsession"的子类想访问它的情况下

// 目标地址类
class Destinations {
public:
	Destinations(struct in_addr const& destAddr,
		Port const& rtpDestPort,
		Port const& rtcpDestPort)
		: isTCP(False), addr(destAddr), rtpPort(rtpDestPort), rtcpPort(rtcpDestPort)
	{
	}
	Destinations(int tcpSockNum, unsigned char rtpChanId, unsigned char rtcpChanId)
		: isTCP(True), rtpPort(0) /*dummy*/, rtcpPort(0) /*dummy*/,
		tcpSocketNum(tcpSockNum), rtpChannelId(rtpChanId), rtcpChannelId(rtcpChanId)
	{
	}

public:
	Boolean isTCP;	//是TCP传输吗？
	struct in_addr addr;	// 地址
	Port rtpPort;	// RTP端口
	Port rtcpPort;	// RTCP端口
	int tcpSocketNum;	//TCP socket号
	unsigned char rtpChannelId, rtcpChannelId;	//Channel 通道
};

//========================================================================

// 流状态
class StreamState {
public:
	StreamState(OnDemandServerMediaSubsession& master,
		Port const& serverRTPPort, Port const& serverRTCPPort,
		RTPSink* rtpSink, BasicUDPSink* udpSink,
		unsigned totalBW, FramedSource* mediaSource,
		Groupsock* rtpGS, Groupsock* rtcpGS);
	virtual ~StreamState();

	// 开始播放
	void startPlaying(Destinations* destinations /* 目标地址 */,
		TaskFunc* rtcpRRHandler, void* rtcpRRHandlerClientData,
		ServerRequestAlternativeByteHandler* serverRequestAlternativeByteHandler/*服务器请求备选字节处理程序*/,
		void* serverRequestAlternativeByteHandlerClientData);
	// 暂停
	void pause();
	// 结束播放
	void endPlaying(Destinations* destinations);
	// 自我回收
	void reclaim();

	unsigned& referenceCount() { return fReferenceCount; }

	Port const& serverRTPPort() const { return fServerRTPPort; }
	Port const& serverRTCPPort() const { return fServerRTCPPort; }

	RTPSink* rtpSink() const { return fRTPSink; }

	float streamDuration() const { return fStreamDuration; }

	FramedSource* mediaSource() const { return fMediaSource; }

private:
	OnDemandServerMediaSubsession& fMaster;	// 绑定一个主人
	Boolean fAreCurrentlyPlaying;	// 当前正在播放的?
	unsigned fReferenceCount;	// 引用计数

	Port fServerRTPPort, fServerRTCPPort;	//RTP/RTCP端口

	RTPSink* fRTPSink;		//RTP槽
	BasicUDPSink* fUDPSink;	//UDP槽

	float fStreamDuration;	//流持续时间
	unsigned fTotalBW;		//总的BW(业务信息仓库Business Information Warehouse)
	RTCPInstance* fRTCPInstance;	// RTCP实体

	FramedSource* fMediaSource;	// 媒体源

	Groupsock* fRTPgs;	// RTP组套接字
	Groupsock* fRTCPgs;	// RTCP组套接字
};

#endif
