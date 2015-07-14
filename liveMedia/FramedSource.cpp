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
// Implementation

#include "FramedSource.hh"
#include <stdlib.h>

////////// FramedSource //////////

FramedSource::FramedSource(UsageEnvironment& env)
: MediaSource(env),
fAfterGettingFunc(NULL), fAfterGettingClientData(NULL),
fOnCloseFunc(NULL), fOnCloseClientData(NULL),
fIsCurrentlyAwaitingData(False)
{
	fPresentationTime.tv_sec = fPresentationTime.tv_usec = 0; // initially
}

FramedSource::~FramedSource()
{
}

Boolean FramedSource::isFramedSource() const
{
	return True;
}

Boolean FramedSource::lookupByName(UsageEnvironment& env, char const* sourceName,
	FramedSource*& resultSource)
{
	resultSource = NULL; // unless we succeed除非我们成功

	MediaSource* source;
	if (!MediaSource::lookupByName(env, sourceName, source)) return False;

	if (!source->isFramedSource()) {
		env.setResultMsg(sourceName, " is not a framed source");
		return False;
	}

	resultSource = (FramedSource*)source;
	return True;
}

void FramedSource::getNextFrame(unsigned char* to, unsigned maxSize,
	afterGettingFunc* afterGettingFunc,
	void* afterGettingClientData,
	onCloseFunc* onCloseFunc,
	void* onCloseClientData)
{
	// Make sure we're not already being read:
	// 确保我们不是正在读取
	if (fIsCurrentlyAwaitingData) {
		envir() << "FramedSource[" << this << "]::getNextFrame(): attempting to read more than once at the same time!\n";
		envir().internalError();
	}

	fTo = to;
	fMaxSize = maxSize;
	fNumTruncatedBytes = 0; // by default; could be changed by doGetNextFrame() 默认，可以调用doGetNextFramed去改变
	fDurationInMicroseconds = 0; // by default; could be changed by doGetNextFrame()
	fAfterGettingFunc = afterGettingFunc;
	fAfterGettingClientData = afterGettingClientData;
	fOnCloseFunc = onCloseFunc;
	fOnCloseClientData = onCloseClientData;
	fIsCurrentlyAwaitingData = True;

	doGetNextFrame();
}

void FramedSource::afterGetting(FramedSource* source)
{
	source->fIsCurrentlyAwaitingData = False;
	// indicates that we can be read again
	//表明我们可以再次读取
	// Note that this needs to be done here, in case the "fAfterFunc"
	// called below tries to read another frame (which it usually will)
	//注意，这需要在这里完成，以防“fAfterFunc”的调用试图读取另一帧（它通常会）

	if (source->fAfterGettingFunc != NULL) {
		(*(source->fAfterGettingFunc))(source->fAfterGettingClientData,
			source->fFrameSize, source->fNumTruncatedBytes,
			source->fPresentationTime,
			source->fDurationInMicroseconds);
	}
}

void FramedSource::handleClosure(void* clientData)
{
	FramedSource* source = (FramedSource*)clientData;
	source->fIsCurrentlyAwaitingData = False; // because we got a close instead	因为我们有一个关闭替代品
	if (source->fOnCloseFunc != NULL) {
		// 函数指针调用函数
		(*(source->fOnCloseFunc))(source->fOnCloseClientData);
	}
}

void FramedSource::stopGettingFrames()
{
	fIsCurrentlyAwaitingData = False; // indicates that we can be read again 表明我们可以再次读取

	// Perform any specialized action now:现在请执行专用操作：
	// 这是个虚函数，本类是抽象类。应该调用派生类的实现
	doStopGettingFrames();
}

void FramedSource::doStopGettingFrames()
{
	// Default implementation: Do nothing	默认什么也不做
	// Subclasses may wish to specialize this so as to ensure that a
	// subsequent reader can pick up where this one left off.
	// 子类可能希望实现这个专门的操作来后续的读取者能够正确的使用
}

unsigned FramedSource::maxFrameSize() const
{
	// By default, this source has no maximum frame size.
	// 默认，这个源没有最大帧大小
	return 0;
}
