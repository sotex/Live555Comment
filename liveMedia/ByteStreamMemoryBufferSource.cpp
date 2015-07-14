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
// Implementation

#include "ByteStreamMemoryBufferSource.hh"
#include "GroupsockHelper.hh"

////////// ByteStreamMemoryBufferSource //////////

ByteStreamMemoryBufferSource*
ByteStreamMemoryBufferSource::createNew(UsageEnvironment& env,
u_int8_t* buffer, u_int64_t bufferSize,
Boolean deleteBufferOnClose,
unsigned preferredFrameSize,
unsigned playTimePerFrame)
{
	if (buffer == NULL) return NULL;

	return new ByteStreamMemoryBufferSource(env, buffer, bufferSize, deleteBufferOnClose, preferredFrameSize, playTimePerFrame);
}

ByteStreamMemoryBufferSource::ByteStreamMemoryBufferSource(UsageEnvironment& env,
	u_int8_t* buffer, u_int64_t bufferSize,
	Boolean deleteBufferOnClose,
	unsigned preferredFrameSize,
	unsigned playTimePerFrame)
	: FramedSource(env), fBuffer(buffer), fBufferSize(bufferSize), fCurIndex(0), fDeleteBufferOnClose(deleteBufferOnClose),
	fPreferredFrameSize(preferredFrameSize), fPlayTimePerFrame(playTimePerFrame), fLastPlayTime(0)
{
}

ByteStreamMemoryBufferSource::~ByteStreamMemoryBufferSource()
{
	if (fDeleteBufferOnClose) delete[] fBuffer;
}

void ByteStreamMemoryBufferSource::seekToByteAbsolute(u_int64_t byteNumber, u_int64_t numBytesToStream)
{
	fCurIndex = byteNumber;	// 跳到byteNumber位置
	// 不能跳出fBuffer
	if (fCurIndex > fBufferSize) fCurIndex = fBufferSize;
	// 设置 可到流字节数
	fNumBytesToStream = numBytesToStream;
	// 如果字节到流数大于0，表明是受到限制的
	fLimitNumBytesToStream = fNumBytesToStream > 0;
}

void ByteStreamMemoryBufferSource::seekToByteRelative(int64_t offset)
{
	// 偏移当前位置
	int64_t newIndex = fCurIndex + offset;
	// 不能那个跳出fBuffer
	if (newIndex < 0) {
		fCurIndex = 0;
	}
	else {
		fCurIndex = (u_int64_t)offset;
		if (fCurIndex > fBufferSize) fCurIndex = fBufferSize;
	}
}

void ByteStreamMemoryBufferSource::doGetNextFrame()
{
	// 检查是否还有数据
	if (fCurIndex >= fBufferSize || (fLimitNumBytesToStream && fNumBytesToStream == 0)) {
		handleClosure(this);
		return;
	}

	// Try to read as many bytes as will fit in the buffer provided (or "fPreferredFrameSize" if less)
	// 尝试读取的字节数能够容纳在提供的buffer(或它小于最佳帧尺寸)
	fFrameSize = fMaxSize;	// 输出 帧尺寸=输入 最大尺寸
	// 如果字节到流数受到限制，那么一帧的大小不能超过这个值
	if (fLimitNumBytesToStream && fNumBytesToStream < (u_int64_t)fFrameSize) {
		fFrameSize = (unsigned)fNumBytesToStream;
	}
	// 也不能超过最佳帧大小
	if (fPreferredFrameSize > 0 && fPreferredFrameSize < fFrameSize) {
		fFrameSize = fPreferredFrameSize;
	}
	// 最多能获取到fBuffer结束的位置
	if (fCurIndex + fFrameSize > fBufferSize) {
		fFrameSize = (unsigned)(fBufferSize - fCurIndex);
	}
	// 开始获取数据了
	// fTo是FrameSource的成员，由getNextFrame方法设置
	memmove(fTo, &fBuffer[fCurIndex], fFrameSize);
	fCurIndex += fFrameSize;
	fNumBytesToStream -= fFrameSize;

	// Set the 'presentation time':设置“呈现时间”：
	if (fPlayTimePerFrame > 0 && fPreferredFrameSize > 0) {
		if (fPresentationTime.tv_sec == 0 && fPresentationTime.tv_usec == 0) {
			// This is the first frame, so use the current time:
			// 这是第一帧，那么使用当前时间：
			gettimeofday(&fPresentationTime, NULL);
		}
		else {
			// Increment by the play time of the previous data:
			// //增量，根据前面数据的播放时间：
			unsigned uSeconds = fPresentationTime.tv_usec + fLastPlayTime;
			fPresentationTime.tv_sec += uSeconds / 1000000;
			fPresentationTime.tv_usec = uSeconds % 1000000;
		}

		// Remember the play time of this data:记住这个数据的播放时间
		// 获取本次帧的播放时间
		fLastPlayTime = (fPlayTimePerFrame*fFrameSize) / fPreferredFrameSize;
		fDurationInMicroseconds = fLastPlayTime;
	}
	else {
		// We don't know a specific play time duration for this data,
		// so just record the current time as being the 'presentation time':
		// 对于这个数据，我们不知道具体的播放的持续时间，所以只能记录当前的时间作为“呈现时间”：
		gettimeofday(&fPresentationTime, NULL);
	}

	// Inform the downstream object that it has data:通知下游对象，它具有数据：
	// 调用 获取后处理函数
	FramedSource::afterGetting(this);
}
