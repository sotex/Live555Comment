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
// Framed Sources
// C++ header

#ifndef _FRAMED_SOURCE_HH
#define _FRAMED_SOURCE_HH

#ifndef _NET_COMMON_H
#include "NetCommon.h"
#endif
#ifndef _MEDIA_SOURCE_HH
#include "MediaSource.hh"
#endif

// 帧源
class FramedSource : public MediaSource {
public:
	// 从env.liveMedia->mediaTable中查找sourceName代表的FramedSource
	static Boolean lookupByName(UsageEnvironment& env, char const* sourceName,
		FramedSource*& resultSource);

	// 类型定义  (获取后...)
	typedef void (afterGettingFunc)(void* clientData, unsigned frameSize,
		unsigned numTruncatedBytes,	/*截断的字节数*/
	struct timeval presentationTime,	/*显示时间*/
		unsigned durationInMicroseconds);	/*持续时长*/
	// 类型定义	(关闭时...)
	typedef void (onCloseFunc)(void* clientData);

	// 将参数赋值给对应的成员变量(目前还在等待读取时).然后调用doGetNextFrame(纯虚函数，派生类中实现)
	void getNextFrame(unsigned char* to/*目标位置*/, unsigned maxSize,
		afterGettingFunc* afterGettingFunc /*获取后回调*/,
		void* afterGettingClientData /*获取后回调函数的参数*/,
		onCloseFunc* onCloseFunc /* 在关闭时回调 */,
		void* onCloseClientData);

	// 关闭时处理
	static void handleClosure(void* clientData);
	// This should be called (on ourself) if the source is discovered
	// to be closed (i.e., no longer readable)
	// 这将在（在我们自己）发现源(source)被关闭时调用（即，不再可读）

	// 停止获取帧
	void stopGettingFrames();

	virtual unsigned maxFrameSize() const;
	// size of the largest possible frame that we may serve, or 0
	// if no such maximum is known (default)
	// 最大帧size，我们可以serve，或者0，如果没有已知的最大值(默认)

	virtual void doGetNextFrame() = 0;
	// called by getNextFrame() 由getNextFrame调用

	Boolean isCurrentlyAwaitingData() const { return fIsCurrentlyAwaitingData; }

	static void afterGetting(FramedSource* source);
	// doGetNextFrame() should arrange for this to be called after the
	// frame has been read (*iff* it is read successfully)
	// doGetNextFrame()应安排在此被调用，帧已被读取后（*当且仅当*这是成功读取）

protected:
	FramedSource(UsageEnvironment& env); // abstract base class
	virtual ~FramedSource();

	virtual void doStopGettingFrames();

protected:
	// The following variables are typically accessed/set by doGetNextFrame()
	// 通过doGetNextFrame()去访问或设置下面的变量
	unsigned char* fTo; // in
	unsigned fMaxSize; // in	最大size
	unsigned fFrameSize; // out	帧size
	unsigned fNumTruncatedBytes; // out	截断的字节数
	struct timeval fPresentationTime; // out	显示时间
	unsigned fDurationInMicroseconds; // out	持续时间数(微秒)

private:
	// redefined virtual functions:
	virtual Boolean isFramedSource() const;

private:
	afterGettingFunc* fAfterGettingFunc;	//获取之后调用
	void* fAfterGettingClientData;
	onCloseFunc* fOnCloseFunc;	//关闭时调用
	void* fOnCloseClientData;

	Boolean fIsCurrentlyAwaitingData;	//目前正在等待数据？
};

#endif
