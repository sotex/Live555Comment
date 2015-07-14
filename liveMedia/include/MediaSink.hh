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
// Media Sinks
// C++ header

#ifndef _MEDIA_SINK_HH
#define _MEDIA_SINK_HH

#ifndef _FRAMED_SOURCE_HH
#include "FramedSource.hh"
#endif

//	媒体槽
class MediaSink : public Medium {
public:
	static Boolean lookupByName(UsageEnvironment& env, char const* sinkName,
		MediaSink*& resultSink);

	// 数据类型，播放后函数(在关闭源时调用)
	typedef void (afterPlayingFunc)(void* clientData);

	Boolean startPlaying(MediaSource& source,
		afterPlayingFunc* afterFunc,
		void* afterClientData);

	virtual void stopPlaying();

	// Test for specific types of sink:
	virtual Boolean isRTPSink() const;

	FramedSource* source() const { return fSource; }

protected:
	MediaSink(UsageEnvironment& env); // abstract base class
	virtual ~MediaSink();

	// 源能够兼容我们?
	virtual Boolean sourceIsCompatibleWithUs(MediaSource& source);
	// called by startPlaying()
	virtual Boolean continuePlaying() = 0;
	// called by startPlaying()

	// 在关闭源时
	static void onSourceClosure(void* clientData);
	// should be called (on ourselves) by continuePlaying() when it
	// discovers that the source we're playing from has closed.
	//应由continuePlaying()调用（我们自己），当它发现我们从播放源已经关闭了。

	FramedSource* fSource;	//帧源

private:
	// redefined virtual functions:
	virtual Boolean isSink() const;

private:
	// The following fields are used when we're being played:
	//以下字段用于当我们正在播放：
	afterPlayingFunc* fAfterFunc;	//播放后调用函数
	void* fAfterClientData;		//播放后调用函数数据
};


//=====================================================================


// A data structure that a sink may use for an output packet:
// 一个接收槽可用于输出数据包的数据结构
// 输出数据包缓冲
class OutPacketBuffer {
public:
	// 构造函数，
	OutPacketBuffer(unsigned preferredPacketSize, unsigned maxPacketSize);
	~OutPacketBuffer();

	static unsigned maxSize;	//最大Size(注意static)
	// 当前可用偏移位置
	unsigned char* curPtr() const { return &fBuf[fPacketStart + fCurOffset]; }

	// 可用的总字节数(fbuf[fPacketStart+fCurOffset]后的都属于可用字节)
	unsigned totalBytesAvailable() const
	{
		return fLimit - (fPacketStart + fCurOffset);
	}
	// 总的buffer大小
	unsigned totalBufferSize() const { return fLimit; }
	// 当前包
	unsigned char* packet() const { return &fBuf[fPacketStart]; }
	// 当前包大小
	unsigned curPacketSize() const { return fCurOffset; }
	// 增量当前包fCurOffset
	void increment(unsigned numBytes) { fCurOffset += numBytes; }
	// 入队from内容到当前包(numBytes字节，超过最大可用字节部分被丢弃)
	void enqueue(unsigned char const* from, unsigned numBytes);
	// 入队word的网络序形式(4Byte)到当前包
	void enqueueWord(u_int32_t word);
	// 插入from中的内容numBytes到当前包的toPosition位置后。超过最大可用字节部分被丢弃
	void insert(unsigned char const* from, unsigned numBytes, unsigned toPosition);
	// 插入word的网络序形式到当前包的toPosition位置
	void insertWord(u_int32_t word, unsigned toPosition);
	// 提取当前包的fromPosition位置后numBytes数据到to。(最多提取到flimit位置)
	void extract(unsigned char* to, unsigned numBytes, unsigned fromPosition);
	// 从当前包的fromPosition位置提取4字节数据，转为主机序返回
	u_int32_t extractWord(unsigned fromPosition);
	// 跳过numBytes的可用字节(fbuf[fPacketStart+fCurOffset]后的都属于可用字节)
	void skipBytes(unsigned numBytes);
	// 当前偏移达到最佳?
	Boolean isPreferredSize() const { return fCurOffset >= fPreferred; }
	// 将会溢出?
	Boolean wouldOverflow(unsigned numBytes) const
	{
		return (fCurOffset + numBytes) > fMax;
	}
	//溢出字节数
	unsigned numOverflowBytes(unsigned numBytes) const
	{
		return (fCurOffset + numBytes) - fMax;
	}
	// 过大的包
	Boolean isTooBigForAPacket(unsigned numBytes) const
	{
		return numBytes > fMax;
	}
	// 设置溢出数据(信息)
	void setOverflowData(unsigned overflowDataOffset,
		unsigned overflowDataSize,
	struct timeval const& presentationTime,
		unsigned durationInMicroseconds);
	// 获取溢出数据量
	unsigned overflowDataSize() const { return fOverflowDataSize; }
	// 获取溢出出现时间
	struct timeval overflowPresentationTime() const 
	{ return fOverflowPresentationTime; }
	
	unsigned overflowDurationInMicroseconds() const 
	{ return fOverflowDurationInMicroseconds; }
	// 有溢出数据？
	Boolean haveOverflowData() const { return fOverflowDataSize > 0; }
	// 将溢出数据部分入队到当前包，并撤销fCurOffset的增加
	void useOverflowData();
	// 调整数据包起始位置(溢出数据偏移也做相应的调整，若起始位置调整到溢出偏移之后，取消溢出数据)
	void adjustPacketStart(unsigned numBytes);
	// 重置包起始为0(若其原本不为0，设置溢出数据偏移为其旧值)
	void resetPacketStart();
	// 重置当前偏移
	void resetOffset() { fCurOffset = 0; }
	// 重置溢出数据为0
	void resetOverflowData() { fOverflowDataOffset = fOverflowDataSize = 0; }

private:
	unsigned fPacketStart, fCurOffset, fPreferred/*最佳包大小*/, fMax/*最大*/, fLimit/*极限*/;
	unsigned char* fBuf;	//缓冲区

	unsigned fOverflowDataOffset, fOverflowDataSize;	//溢出数据偏移，溢出数据长度
	struct timeval fOverflowPresentationTime;	//溢出出现时间
	unsigned fOverflowDurationInMicroseconds;	//溢出时间的微秒描述
};

#endif
