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
// A class for streaming data from a (static) memory buffer, as if it were a file.
// C++ header

#ifndef _BYTE_STREAM_MEMORY_BUFFER_SOURCE_HH
#define _BYTE_STREAM_MEMORY_BUFFER_SOURCE_HH

#ifndef _FRAMED_SOURCE_HH
#include "FramedSource.hh"
#endif

class ByteStreamMemoryBufferSource : public FramedSource {
public:
	// 在参数buffer!=NULL的时候，创建内存缓存字节流源对象
	static ByteStreamMemoryBufferSource* createNew(UsageEnvironment& env,
		u_int8_t* buffer, u_int64_t bufferSize,
		Boolean deleteBufferOnClose = True,
		unsigned preferredFrameSize = 0,
		unsigned playTimePerFrame = 0);
	// "preferredFrameSize" == 0 means 'no preference'(没有要求)
	// "playTimePerFrame" is in microseconds(微秒单位)

	u_int64_t bufferSize() const { return fBufferSize; }

	// 跳寻到byteNumber位置(绝对位置),更新numBytesToStream
	void seekToByteAbsolute(u_int64_t byteNumber, u_int64_t numBytesToStream = 0);
	// if "numBytesToStream" is >0, then we limit the stream to that number of bytes, before treating it as EOF
	// 如果“numBytesToStream”> 0，那么我们限制流的字节数，将其视为EOF前

	// 跳寻到字节流相对位置
	void seekToByteRelative(int64_t offset);

protected:
	// 用参数初始化成员
	ByteStreamMemoryBufferSource(UsageEnvironment& env,
		u_int8_t* buffer, u_int64_t bufferSize,
		Boolean deleteBufferOnClose,
		unsigned preferredFrameSize,
		unsigned playTimePerFrame);
	// called only by createNew()

	virtual ~ByteStreamMemoryBufferSource();

private:
	// redefined virtual functions:
	virtual void doGetNextFrame();

private:
	u_int8_t* fBuffer;		// 缓冲区
	u_int64_t fBufferSize;	// 缓冲区大小
	u_int64_t fCurIndex;	// 当前索引位置
	Boolean fDeleteBufferOnClose;	//关闭时释放缓冲区
	unsigned fPreferredFrameSize;	//最佳帧大小
	unsigned fPlayTimePerFrame;		//每帧播放时间
	unsigned fLastPlayTime;		// 最后播放时间
	Boolean fLimitNumBytesToStream;	//到流字节数受到限制?(表示获取帧的时候要检查)
	u_int64_t fNumBytesToStream; //可到流字节数 used iff "fLimitNumBytesToStream" is True 当且仅当用于“fLimitNumBytesToStream”是真
};

#endif
