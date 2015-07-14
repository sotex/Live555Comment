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
// A source that consists of multiple byte-stream files, read sequentially
// C++ header

#ifndef _BYTE_STREAM_MULTI_FILE_SOURCE_HH
#define _BYTE_STREAM_MULTI_FILE_SOURCE_HH

#ifndef _BYTE_STREAM_FILE_SOURCE_HH
#include "ByteStreamFileSource.hh"
#endif

// 多文件字节流源
class ByteStreamMultiFileSource : public FramedSource {
public:
	static ByteStreamMultiFileSource*
		createNew(UsageEnvironment& env, char const** fileNameArray,
		unsigned preferredFrameSize = 0, unsigned playTimePerFrame = 0);
	// A 'filename' of NULL indicates the end of the array
	// fileNameArray数组的结束是一个为NULL的元素

	Boolean haveStartedNewFile() const { return fHaveStartedNewFile; }
	// True iff the most recently delivered frame was the first from a newly-opened file
	// true 如果在最近提供帧是首次从一个新打开的文件来的

protected:
	ByteStreamMultiFileSource(UsageEnvironment& env, char const** fileNameArray,
		unsigned preferredFrameSize, unsigned playTimePerFrame);
	// called only by createNew()

	virtual ~ByteStreamMultiFileSource();

private:
	// redefined virtual functions:
	virtual void doGetNextFrame();

private:
	//	关闭时调用，注意是static方法(原因还是因为作为回调函数)
	static void onSourceClosure(void* clientData);
	// 关闭时的真实调用
	void onSourceClosure1();
	// 获取一帧后调用，作为回调使用
	static void afterGettingFrame(void* clientData,
		unsigned frameSize, unsigned numTruncatedBytes,
	struct timeval presentationTime,
		unsigned durationInMicroseconds);

private:
	unsigned fPreferredFrameSize;	//最佳帧大小
	unsigned fPlayTimePerFrame;		//每帧播放时间
	unsigned fNumSources;	//源编号
	unsigned fCurrentlyReadSourceNumber;	//当前读取源编号
	Boolean fHaveStartedNewFile;	//开始新文件？
	char const** fFileNameArray;	//文件名数组
	ByteStreamFileSource** fSourceArray;	//文件字节流源数组
};

#endif
