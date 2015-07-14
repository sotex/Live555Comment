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
// Implementation

#include "MediaSink.hh"
#include "GroupsockHelper.hh"
#include <string.h>

////////// MediaSink //////////

MediaSink::MediaSink(UsageEnvironment& env)
: Medium(env), fSource(NULL)
{
}

MediaSink::~MediaSink()
{
	stopPlaying();
}

Boolean MediaSink::isSink() const
{
	return True;
}

Boolean MediaSink::lookupByName(UsageEnvironment& env, char const* sinkName,
	MediaSink*& resultSink)
{
	resultSink = NULL; // unless we succeed

	Medium* medium;
	if (!Medium::lookupByName(env, sinkName, medium)) return False;

	if (!medium->isSink()) {
		env.setResultMsg(sinkName, " is not a media sink");
		return False;
	}

	resultSink = (MediaSink*)medium;
	return True;
}

Boolean MediaSink::sourceIsCompatibleWithUs(MediaSource& source)
{
	// We currently support only framed sources.我们目前只支持帧源。
	return source.isFramedSource();
}

Boolean MediaSink::startPlaying(MediaSource& source,
	afterPlayingFunc* afterFunc,
	void* afterClientData)
{
	// Make sure we're not already being played://确保我们不是已经正在播放
	if (fSource != NULL) {
		envir().setResultMsg("This sink is already being played");
		return False;
	}

	// Make sure our source is compatible:确保我们的源兼容
	if (!sourceIsCompatibleWithUs(source)) {
		envir().setResultMsg("MediaSink::startPlaying(): source is not compatible!");
		return False;
	}
	// 设置帧源
	fSource = (FramedSource*)&source;

	fAfterFunc = afterFunc;	// 设置播放后调用
	fAfterClientData = afterClientData;	//设置播放后调用数据
	return continuePlaying();
}

void MediaSink::stopPlaying()
{
	// First, tell the source that we're no longer interested:
	// 首先，通知我们不再去对获取帧源
	if (fSource != NULL) fSource->stopGettingFrames();

	// Cancel any pending tasks:取消所有待处理任务
	envir().taskScheduler().unscheduleDelayedTask(nextTask());
	nextTask() = NULL;

	fSource = NULL; // indicates that we can be played again我们可以再次使用播放
	fAfterFunc = NULL;
}

void MediaSink::onSourceClosure(void* clientData)
{
	MediaSink* sink = (MediaSink*)clientData;
	sink->fSource = NULL; // indicates that we can be played again表明我们可以再次播放
	if (sink->fAfterFunc != NULL) {
		// 调用播放后回调
		(*(sink->fAfterFunc))(sink->fAfterClientData);
	}
}

Boolean MediaSink::isRTPSink() const
{
	return False; // default implementation
}

////////// OutPacketBuffer //////////

unsigned OutPacketBuffer::maxSize = 60000; // by default

OutPacketBuffer::OutPacketBuffer(unsigned preferredPacketSize,
	unsigned maxPacketSize)
	: fPreferred(preferredPacketSize), fMax(maxPacketSize),
	fOverflowDataSize(0)
{	//最大包数(maxSize定义了最大的数据尺寸)，计算的结果是maxSize可用容纳的包数向上取整
	unsigned maxNumPackets = (maxSize /*60000*/ + (maxPacketSize - 1)) / maxPacketSize;
	// 极限，最大包数*包大小
	fLimit = maxNumPackets*maxPacketSize;
	fBuf = new unsigned char[fLimit];	//buffer的大小是极限大小
	resetPacketStart();	//重设包起始
	resetOffset();	//重设偏移
	resetOverflowData();	//重设溢出数据
}

OutPacketBuffer::~OutPacketBuffer()
{
	delete[] fBuf;
}

void OutPacketBuffer::enqueue(unsigned char const* from, unsigned numBytes)
{
	// 控制入队的尺寸在总的可用尺寸之内
	if (numBytes > totalBytesAvailable()) {
#ifdef DEBUG
		fprintf(stderr, "OutPacketBuffer::enqueue() warning: %d > %d\n", numBytes, totalBytesAvailable());
#endif
		numBytes = totalBytesAvailable();
	}
	// 安全的将from内容拷贝到fbuf[fPacketStart+fCurOffset]之后位置
	// 注意memmove是考虑了重叠的
	if (curPtr() != from) memmove(curPtr(), from, numBytes);
	// 增量包大小
	increment(numBytes);
}

void OutPacketBuffer::enqueueWord(u_int32_t word)
{
	u_int32_t nWord = htonl(word);
	enqueue((unsigned char*)&nWord, 4);
}

void OutPacketBuffer::insert(unsigned char const* from, unsigned numBytes,
	unsigned toPosition)
{
	// 定位要插入的位置
	unsigned realToPosition = fPacketStart + toPosition;
	if (realToPosition + numBytes > fLimit) {
		if (realToPosition > fLimit) return; // we can't do this不可为之
		numBytes = fLimit - realToPosition;	//插入数据量不能超过极限
	}
	// 安全的插入数据
	memmove(&fBuf[realToPosition], from, numBytes);
	// 设置包尾
	if (toPosition + numBytes > fCurOffset) {
		fCurOffset = toPosition + numBytes;
	}
}

void OutPacketBuffer::insertWord(u_int32_t word, unsigned toPosition)
{
	u_int32_t nWord = htonl(word);
	insert((unsigned char*)&nWord, 4, toPosition);
}

void OutPacketBuffer::extract(unsigned char* to, unsigned numBytes,
	unsigned fromPosition)
{
	// 真实的起始提取位置
	unsigned realFromPosition = fPacketStart + fromPosition;
	if (realFromPosition + numBytes > fLimit) { // sanity check完整性检查
		if (realFromPosition > fLimit) return; // we can't do this
		numBytes = fLimit - realFromPosition;	//提取的部分不能在极限位置后
	}
	// 开始提取，安全的
	memmove(to, &fBuf[realFromPosition], numBytes);
}

u_int32_t OutPacketBuffer::extractWord(unsigned fromPosition)
{
	u_int32_t nWord;
	extract((unsigned char*)&nWord, 4, fromPosition);
	return ntohl(nWord);
}

void OutPacketBuffer::skipBytes(unsigned numBytes)
{
	if (numBytes > totalBytesAvailable()) {
		numBytes = totalBytesAvailable();
	}

	increment(numBytes);
}

void OutPacketBuffer
::setOverflowData(unsigned overflowDataOffset,
unsigned overflowDataSize,
struct timeval const& presentationTime,
	unsigned durationInMicroseconds)
{
	fOverflowDataOffset = overflowDataOffset;
	fOverflowDataSize = overflowDataSize;
	fOverflowPresentationTime = presentationTime;
	fOverflowDurationInMicroseconds = durationInMicroseconds;
}

void OutPacketBuffer::useOverflowData()
{
	enqueue(&fBuf[fPacketStart + fOverflowDataOffset], fOverflowDataSize);
	fCurOffset -= fOverflowDataSize; // undoes increment performed by "enqueue"撤销“入队”执行的增量
	resetOverflowData();
	/*
	溢出使用前(溢出数据并不一定在当前包内容后，也许有重叠等)
	fBuf [_____|当前包内容 |_____________|溢出数据|_________]
               |         |             |  (溢出量=fOverflowDataSize)
	fPacketStart  fCurOffset     fOverflowDataOffset

	溢出使用后
	fBuf [_____|当前包内容 |溢出数据|______________________]
               |         |
	fPacketStart  fCurOffset
	溢出偏移fOverflowDataOffset=0
	溢出量  fOverflowDataSize=0
	*/
}

void OutPacketBuffer::adjustPacketStart(unsigned numBytes)
{
	fPacketStart += numBytes;
	if (fOverflowDataOffset >= numBytes) {
		fOverflowDataOffset -= numBytes;
	}
	else {
		fOverflowDataOffset = 0;
		fOverflowDataSize = 0; // an error otherwise一个错误，否则
	}
}

void OutPacketBuffer::resetPacketStart()
{
	if (fOverflowDataSize > 0) {
		fOverflowDataOffset += fPacketStart;
	}
	fPacketStart = 0;
}
