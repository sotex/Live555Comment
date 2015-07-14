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
// RTP sink for a common kind of payload format: Those which pack multiple,
// complete codec frames (as many as possible) into each RTP packet.
// C++ header

#ifndef _MULTI_FRAMED_RTP_SINK_HH
#define _MULTI_FRAMED_RTP_SINK_HH

#ifndef _RTP_SINK_HH
#include "RTPSink.hh"
#endif

class MultiFramedRTPSink : public RTPSink {
public:
	// 设置fOutBuf包大小
	void setPacketSizes(unsigned preferredPacketSize, unsigned maxPacketSize);
	// 类型定义
	typedef void (onSendErrorFunc)(void* clientData);
	//设置发生错误时回调
	void setOnSendErrorFunc(onSendErrorFunc* onSendErrorFunc, void* onSendErrorFuncData)
	{
		// Can be used to set a callback function to be called if there's an error sending RTP packets on our socket.
		// 可以用来设置一个回调函数，如果在我们的套接字上发送RTP包有一个错误，将被调用。
		fOnSendErrorFunc = onSendErrorFunc;
		fOnSendErrorData = onSendErrorFuncData;
	}

protected:
	// 构造函数(注意这是protected的)
	MultiFramedRTPSink(UsageEnvironment& env,
		Groupsock* rtpgs, unsigned char rtpPayloadType,
		unsigned rtpTimestampFrequency,
		char const* rtpPayloadFormatName,
		unsigned numChannels = 1);
	// we're a virtual base class我们是一个虚基类

	virtual ~MultiFramedRTPSink();
	// 做特定帧处理
	virtual void doSpecialFrameHandling(unsigned fragmentationOffset,
		unsigned char* frameStart,
		unsigned numBytesInFrame,
	struct timeval framePresentationTime,
		unsigned numRemainingBytes);
	// perform any processing specific to the particular payload format//执行特定的任何处理，以所述特定有效负载格式

	// 运行分片在开始之后
	virtual Boolean allowFragmentationAfterStart() const;
	// whether a frame can be fragmented if other frame(s) appear earlier
	// in the packet (by default: False)
	// 无论是否有其他的帧,较早出现在数据包的帧可以分片（默认：假）

	// 允许其他帧在最后一个片段后?
	virtual Boolean allowOtherFramesAfterLastFragment() const;
	// whether other frames can be packed into a packet following the
	// final fragment of a previous, fragmented frame (by default: False)
	// 其它帧是否可被包装成一个数据包接着前个，分片帧的最后片段（默认：假）

	// 帧可以出现在数据包开始之后？
	virtual Boolean frameCanAppearAfterPacketStart(unsigned char const* frameStart,
		unsigned numBytesInFrame) const;
	// whether this frame can appear in position >1 in a pkt (default: True)
	// 该帧是否可以在数据包位置position>1出现（默认：真）

	// 特定头部大小
	virtual unsigned specialHeaderSize() const;
	// returns the size of any special header used (following the RTP header) (default: 0)返回使用任何特定的头部的大小（下面的RTP头）（默认值：0）

	// 特定帧头部大小
	virtual unsigned frameSpecificHeaderSize() const;
	// returns the size of any frame-specific header used (before each frame
	// within the packet) (default: 0)返回用于的任意特定帧的头部大小（包内的每个帧之前）（默认值：0）

	// 计算新帧溢出
	virtual unsigned computeOverflowForNewFrame(unsigned newFrameSize) const;
	// returns the number of overflow bytes that would be produced by adding a new
	// frame of size "newFrameSize" to the current RTP packet.
	// (By default, this just calls "numOverflowBytes()", but subclasses can redefine
	// this to (e.g.) impose a granularity upon RTP payload fragments.)
	// 返回将添加一个“newFrameSize”大小的新的帧到当前RTP包中产生溢字节数。
	//（默认情况下，这只是调用“numOverflowBytes（）”，但子类可以重新定义这（例如）强加给RTP有效负载分片粒度。）

	// Functions that might be called by doSpecialFrameHandling(), or other subclass virtual functions:函数可能由doSpecialFrameHandling()，或其它子类的虚拟函数调用：

	// 第一个包？
	Boolean isFirstPacket() const { return fIsFirstPacket; }
	// 数据包的第一个帧？
	Boolean isFirstFrameInPacket() const { return fNumFramesUsedSoFar == 0; }
	// 当前分片偏移
	Boolean curFragmentationOffset() const { return fCurFragmentationOffset; }
	// 设置标记位(RTP头部)
	void setMarkerBit();
	// 设置时间戳
	void setTimestamp(struct timeval framePresentationTime);
	// 设置特定头部(4字节)
	void setSpecialHeaderWord(unsigned word, /* 32 bits, in host order */
		unsigned wordPosition = 0);
	// 设置特定头部(numBytes字节)
	void setSpecialHeaderBytes(unsigned char const* bytes, unsigned numBytes,
		unsigned bytePosition = 0);
	// 设定特定帧头部
	void setFrameSpecificHeaderWord(unsigned word, /* 32 bits, in host order */
		unsigned wordPosition = 0);
	// 设定特定帧头部
	void setFrameSpecificHeaderBytes(unsigned char const* bytes, unsigned numBytes,
		unsigned bytePosition = 0);
	// 设置帧填充
	void setFramePadding(unsigned numPaddingBytes);
	// 目前使用帧数
	unsigned numFramesUsedSoFar() const { return fNumFramesUsedSoFar; }
	// 最大包大小
	unsigned ourMaxPacketSize() const { return fOurMaxPacketSize; }

public: // redefined virtual functions:
	// 停止播放
	virtual void stopPlaying();

protected: // redefined virtual functions:
	// 继续播放
	virtual Boolean continuePlaying();

private:
	void buildAndSendPacket(Boolean isFirstPacket);
	// 打包帧(获取下一个帧，添加到packet)
	void packFrame();
	void sendPacketIfNecessary();
	// 下面的两个函数不是重载，一个是成员，一个不是成员
	static void sendNext(void* firstArg);//静态的
	friend void sendNext(void*);	//这是个友元函数

	
	static void afterGettingFrame(void* clientData,
		unsigned numBytesRead, unsigned numTruncatedBytes,
	struct timeval presentationTime,
		unsigned durationInMicroseconds);
	// 在包还未完成组装前，获取下一帧继续，包打包完成后进行发送
	void afterGettingFrame1(unsigned numBytesRead, unsigned numTruncatedBytes,
	struct timeval presentationTime,
		unsigned durationInMicroseconds);
	// 数据是否过多？
	Boolean isTooBigForAPacket(unsigned numBytes) const;
	// 关闭时处理
	static void ourHandleClosure(void* clientData);

private:
	OutPacketBuffer* fOutBuf;	//输出缓冲

	Boolean fNoFramesLeft;	//
	unsigned fNumFramesUsedSoFar;	//目前使用帧数
	// (如果一个信息包太大而无法装载，它就不得不被分成片断)
	unsigned fCurFragmentationOffset;	//当前片段偏移
	Boolean fPreviousFrameEndedFragmentation;	// 上一帧结束分片

	/*
	分片的依据是网络的MTU（Maximum Transmission Units，最大传输单元）。例如，令牌环网（token ring）的MTU是4464，以太网（Ethernet）的MTU是1500，因此，如果一个信息包要从令牌环网传输到以太网，它就要被分裂成一些小的片断，然后再在目的地重建。虽然这样处理会造成效率降低，但是分片的效果还是很好的。黑客将分片视为躲避IDS的方法，另外还有一些DOS攻击也使用分片技术。  
	*/


	Boolean fIsFirstPacket;	//是第一个包？
	struct timeval fNextSendTime;	//下一次发送时间
	unsigned fTimestampPosition;	//时间戳位置
	unsigned fSpecialHeaderPosition;//特定头部位置
	unsigned fSpecialHeaderSize; // size in bytes of any special header used在任何特定头部使用的字节大小
	unsigned fCurFrameSpecificHeaderPosition;	//当前帧特定头部位置
	unsigned fCurFrameSpecificHeaderSize; // size in bytes of cur frame-specific header当前特定帧头部的字节大小
	unsigned fTotalFrameSpecificHeaderSizes; // size of all frame-specific hdrs in pkt所有的特定帧头部在包中的大小
	unsigned fOurMaxPacketSize;	//最大包尺寸

	onSendErrorFunc* fOnSendErrorFunc;	//在发送出错时调用
	void* fOnSendErrorData;
};

#endif
