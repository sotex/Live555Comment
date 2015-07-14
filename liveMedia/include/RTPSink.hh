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
// RTP Sinks
// C++ header

#ifndef _RTP_SINK_HH
#define _RTP_SINK_HH

#ifndef _MEDIA_SINK_HH
#include "MediaSink.hh"
#endif
#ifndef _RTP_INTERFACE_HH
#include "RTPInterface.hh"
#endif

class RTPTransmissionStatsDB; // forward

//	RTP装载槽
class RTPSink : public MediaSink {
public:
	// 从env.liveMedia->mediaTable中查找sinkName对应的RTPSink对象地址
	static Boolean lookupByName(UsageEnvironment& env, char const* sinkName,
		RTPSink*& resultSink);

	// used by RTCP: RTCP使用
	u_int32_t SSRC() const { return fSSRC; }
	// later need a means of changing the SSRC if there's a collision #####
	// 在以后需要更改SSRC的一种手段，如果有发生碰撞#####

	// 转换tv到RTP时间戳
	u_int32_t convertToRTPTimestamp(struct timeval tv);
	// 获取包计数
	unsigned packetCount() const { return fPacketCount; }
	// 获取8bit计数
	unsigned octetCount() const { return fOctetCount; }

	// used by RTSP servers:用于RTSP服务器
	// 组套接字开始使用
	Groupsock const& groupsockBeingUsed() const/*const重载(供const对象使用)*/ 
	{ return *(fRTPInterface.gs()); }
	// 组套接字开始使用
	Groupsock& groupsockBeingUsed() { return *(fRTPInterface.gs()); }
	//RTP负载类型
	unsigned char rtpPayloadType() const { return fRTPPayloadType; }
	// RTP时间戳频率
	unsigned rtpTimestampFrequency() const { return fTimestampFrequency; }
	// 设置RTP时间戳频率
	void setRTPTimestampFrequency(unsigned freq)
	{
		fTimestampFrequency = freq;
	}
	// RTP负载格式名
	char const* rtpPayloadFormatName() const { return fRTPPayloadFormatName; }
	// 通道号
	unsigned numChannels() const { return fNumChannels; }

	virtual char const* sdpMediaType() const; // for use in SDP m= lines

	// 获取RTP映射格式行，需要自行delete返回
	virtual char* rtpmapLine() const;
	// returns a string to be delete[]d

	// 辅助SDP行
	virtual char const* auxSDPLine();
	// 可选的optional SDP line (e.g. a=fmtp:...)

	// 当前的队列编号
	u_int16_t currentSeqNo() const { return fSeqNo; }

	u_int32_t presetNextTimestamp();
	// ensures that the next timestamp to be used will correspond to
	// the current 'wall clock' time.
	// 确保要使用的下一个时间戳将对应于当前的“墙上的钟”的时间(时刻)。

	RTPTransmissionStatsDB& transmissionStatsDB() const
	{
		return *fTransmissionStatsDB;
	}

	Boolean nextTimestampHasBeenPreset() const { return fNextTimestampHasBeenPreset; }
	// 设置流Socket
	void setStreamSocket(int sockNum, unsigned char streamChannelId)
	{
		fRTPInterface.setStreamSocket(sockNum, streamChannelId);
	}
	// 添加一个流Socket
	void addStreamSocket(int sockNum, unsigned char streamChannelId)
	{
		fRTPInterface.addStreamSocket(sockNum, streamChannelId);
	}
	void removeStreamSocket(int sockNum, unsigned char streamChannelId)
	{
		fRTPInterface.removeStreamSocket(sockNum, streamChannelId);
	}
	// 设置服务器请求备选字节处理程序
	void setServerRequestAlternativeByteHandler(int socketNum, ServerRequestAlternativeByteHandler* handler, void* clientData)
	{
		fRTPInterface.setServerRequestAlternativeByteHandler(socketNum, handler, clientData);
	}
	// hacks to allow sending RTP over TCP (RFC 2236, section 10.12)
	// 允许发送的RTP基于TCP（RFC2236节10.12）

	// 获取总的比特率	Elapsed过去，已经，已播放
	void getTotalBitrate(unsigned& outNumBytes, double& outElapsedTime);
	// returns the number of bytes sent since the last time that we
	// were called, and resets the counter.
	// 返回自从上次我们被调用发送的字节数，并复位计数器。

protected:
	RTPSink(UsageEnvironment& env,
		Groupsock* rtpGS, unsigned char rtpPayloadType,
		u_int32_t rtpTimestampFrequency,
		char const* rtpPayloadFormatName,
		unsigned numChannels);
	// abstract base class

	virtual ~RTPSink();

	RTPInterface fRTPInterface;		//RTP接口
	unsigned char fRTPPayloadType;	//RTP负载类型
	unsigned fPacketCount/*包计数*/, fOctetCount/*8bit计数*/, 
		fTotalOctetCount /*包含RTP头部incl RTP hdr*/;
	struct timeval fTotalOctetCountStartTime;	//总的起始时间(Octet 8bit)
	u_int32_t fCurrentTimestamp;	//当前时间戳
	u_int16_t fSeqNo;	//队列号(Sequeue Numero)

private:
	// redefined virtual functions:
	virtual Boolean isRTPSink() const;

private:
	/*同步信源是指产生媒体流的信源，它通过RTP报头中的一个32位数字SSRC标识符来标识,
	而不依赖于网络地址，接收者将根据SSRC标识符来区分不同的信源，进行RTP报文的分组。*/
	u_int32_t fSSRC;/* 同步信源(SSRC)标识符。*/
	u_int32_t fTimestampBase;	/*时间戳基数*/
	unsigned fTimestampFrequency;	/*时间戳频率*/
	Boolean fNextTimestampHasBeenPreset;	//下一步时间戳已经预设？
	char const* fRTPPayloadFormatName;	//RTP负载格式名称
	unsigned fNumChannels;	//通道号
	struct timeval fCreationTime;	// 创建时间

	RTPTransmissionStatsDB* fTransmissionStatsDB;	// 传送状态数据库
};


class RTPTransmissionStats; // forward

//=====================================================================


//	RTP传送状态数据库
class RTPTransmissionStatsDB {
public:
	unsigned numReceivers() const { return fNumReceivers; }

	class Iterator {
	public:
		Iterator(RTPTransmissionStatsDB& receptionStatsDB);
		virtual ~Iterator();

		RTPTransmissionStats* next();
		// NULL if none

	private:
		HashTable::Iterator* fIter;
	};

	/*
	RTCP的五种分组类型
	类型  缩写表示                            意义
	200  SR（Sender Report）                发送端报告
	201  RR（Receiver Roport）              接收端报告
	202  SDES（Source Descripition Items）  源点
	203  BYE                               结束
	204  APP（Application）                 特定应用

	结束分组  BYE  表示关闭一个数据流。
	特定应用分组  APP  使应用程序能够定义新的分组类型、。
	接收端报告分组  RR  用来使接收端周期性地向所有的点用多播方式进行报告。接收端每收到一个RTP流（一次会话包含多个RTP流）就产生一个接收端报告分组RR。RR分组的内容有：所收到的RTP流的SSRC；该RTP流的分组丢失率（若分组丢失率太高，发送端就应该适当地降低发送分组的速率）；在该RTP流中的最后一个RTP分组的序号；分组到达时间间隔的抖动等；
	发送RR分组有两个目的。第一，可以使所有的接收端和发送端了解当前网络的状态。第二，可以使所有发送RTCP分组的站点自适应地调整自己发送RTCP分组的速率，使得起控制作用的RTCP分组不要过多地影响传送应用数据的RTP分组在网络中的传输。通常是使RTCP分组的通信量不超过网络中工大数据分组的数据量的5%，而接收端的通信量又应小于所有RTCP分组的通信量的75%。
	发送端报告分组  SR  用来使发送端周期性地向所有接收端用多播方式进行报告。发送端每发送一个RTP流就要发送一个发送端报告分组SR。SR分组的内容有：该RTP流的SSRC；该RTP流中最新产生的RTP分组的时间戳和绝对时钟时间（或墙上时钟时间wall clock time）；该RTP流包含的分组数；该RTP流包含的字节数。
	绝对时钟时间是必要的。因为RTP要求每一种媒体使用一个流。例如，要传送视频图像和相应的声音就需要传送两个流。有了绝对时钟时间就可以进行图像和声音的同步。
	源点描述分组  SDES  给出会话中参加者的描述，它包含参加者的规范名CNAME（Canonical NAME）。规范名是参加者的电子邮件地址的字符串。
*/

	// The following is called whenever a RTCP RR packet is received:
	// 以下每当接收到的RTCP RR(Receiver Roport接收端报告)分组时调用：
	// 注意到传入RR(从哈希表中查找SSRC对应的对象来处理)
	void noteIncomingRR(u_int32_t SSRC, struct sockaddr_in const& lastFromAddress,
		unsigned lossStats, unsigned lastPacketNumReceived,
		unsigned jitter/*抖动*/, unsigned lastSRTime, unsigned diffSR_RRTime);


	// The following is called when a RTCP BYE packet is received:
	// 以下收到RTCP BYE包时调用：
	// 删除记录
	void removeRecord(u_int32_t SSRC);

	// 从fTable中查找SSRC关联的RTPTransmissionStats对象地址
	RTPTransmissionStats* lookup(u_int32_t SSRC) const;

private: // constructor and destructor, called only by RTPSink:
	// 构造和析构，仅被RTPSink调用
	friend class RTPSink;
	RTPTransmissionStatsDB(RTPSink& rtpSink);
	virtual ~RTPTransmissionStatsDB();

private:
	// 添加记录(key:SSRC+value:stats)
	void add(u_int32_t SSRC, RTPTransmissionStats* stats);

private:
	friend class Iterator;
	unsigned	fNumReceivers;	//接收数
	RTPSink&	fOurRTPSink;	//RTP槽
	HashTable*	fTable;			//数据库表
};

//=====================================================================


// RTP传输状态
class RTPTransmissionStats {
public:
	u_int32_t SSRC() const { return fSSRC; }
	struct sockaddr_in const& lastFromAddress() const { return fLastFromAddress; }
	unsigned lastPacketNumReceived() const { return fLastPacketNumReceived; }
	unsigned firstPacketNumReported() const { return fFirstPacketNumReported; }
	unsigned totNumPacketsLost() const { return fTotNumPacketsLost; }
	unsigned jitter() const { return fJitter; }
	unsigned lastSRTime() const { return fLastSRTime; }
	unsigned diffSR_RRTime() const { return fDiffSR_RRTime; }

	unsigned roundTripDelay() const;

	/*
	RTT(Round-Trip Time): 往返时延。在计算机网络中它是一个重要的性能指标，表示从发送端发送数据开始，到发送端收到来自接收端的确认（接收端收到数据后便立即发送确认），总共经历的时延。
	往返延时(RTT)由三个部分决定：即链路的传播时间、末端系统的处理时间以及路由器的缓存中的排队和处理时间。其中，前面两个部分的值作为一个TCP连接相对固定，路由器的缓存中的排队和处理时间会随着整个网络拥塞程度的变化而变化。所以RTT的变化在一定程度上反映了网络拥塞程度的变化。简单来说就是发送方从发送数据开始，到收到来自接受方的确认信息所经历的时间。
	*/

	// The round-trip delay (in units of 1/65536 seconds) computed from
	// the most recently-received RTCP RR packet.
	// 在往返延迟（以1/65536秒为单位）根据最新接收到的RTCP RR数据包计算的。
	struct timeval timeCreated() const { return fTimeCreated; }
	struct timeval lastTimeReceived() const { return fTimeReceived; }
	void getTotalOctetCount(u_int32_t& hi, u_int32_t& lo);
	void getTotalPacketCount(u_int32_t& hi, u_int32_t& lo);

	// Information which requires at least two RRs to have been received:
	// 这信息要求至少两个的RR已经收到：
	Boolean oldValid() const { return fOldValid; } // Have two RRs been received?有两个的RR已收到？

	// 自上次RR接收的数据包
	unsigned packetsReceivedSinceLastRR() const;
	u_int8_t packetLossRatio() const { return fPacketLossRatio; }
	// as an 8-bit fixed-point number作为一个8位定点数
	int packetsLostBetweenRR() const;

private:
	// called only by RTPTransmissionStatsDB:
	// 仅被RTPTransmissionStatsDB调用
	friend class RTPTransmissionStatsDB;
	RTPTransmissionStats(RTPSink& rtpSink, u_int32_t SSRC);
	virtual ~RTPTransmissionStats();

	void noteIncomingRR(struct sockaddr_in const& lastFromAddress,
		unsigned lossStats, unsigned lastPacketNumReceived,
		unsigned jitter,
		unsigned lastSRTime, unsigned diffSR_RRTime);

private:
	RTPSink& fOurRTPSink;	//RTP槽
	u_int32_t fSSRC;		//同步信源标识
	struct sockaddr_in fLastFromAddress;	//上次来源地址
	unsigned fLastPacketNumReceived;	//上次包接收数
	u_int8_t fPacketLossRatio;	//丢包率
	unsigned fTotNumPacketsLost;	// 总丢包数
	unsigned fJitter;	//抖动
	unsigned fLastSRTime;	//上次 SR 时间
	unsigned fDiffSR_RRTime;	//SR和RR差异时间
	struct timeval fTimeCreated, fTimeReceived;	//创建时间/接收时间
	Boolean fOldValid;	//旧的有效?
	unsigned fOldLastPacketNumReceived;	//上上次的接收包数
	unsigned fOldTotNumPacketsLost;	//旧的丢包总数
	Boolean fFirstPacket;	//第一个包？
	unsigned fFirstPacketNumReported;	//
	u_int32_t fLastOctetCount/*上次八位计数*/, fTotalOctetCount_hi, fTotalOctetCount_lo;
	u_int32_t fLastPacketCount, fTotalPacketCount_hi, fTotalPacketCount_lo;
};

#endif
