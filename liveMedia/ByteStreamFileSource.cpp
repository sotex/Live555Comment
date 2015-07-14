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
// A file source that is a plain byte stream (rather than frames)
// Implementation

#if (defined(__WIN32__) || defined(_WIN32)) && !defined(_WIN32_WCE)
#include <io.h>
#include <fcntl.h>
#define READ_FROM_FILES_SYNCHRONOUSLY 1
// Because Windows is a silly toy operating system that doesn't (reliably) treat
// open files as being readable sockets (which can be handled within the default
// "BasicTaskScheduler" event loop, using "select()"), we implement file reading
// in Windows using synchronous, rather than asynchronous, I/O.  This can severely
// limit the scalability of servers using this code that run on Windows.
// If this is a problem for you, then either use a better operating system,
// or else write your own Windows-specific event loop ("TaskScheduler" subclass)
// that can handle readable data in Windows open files as an event.
#endif

#include "ByteStreamFileSource.hh"
#include "InputFile.hh"
#include "GroupsockHelper.hh"

////////// ByteStreamFileSource //////////

ByteStreamFileSource*
ByteStreamFileSource::createNew(UsageEnvironment& env, char const* fileName,
unsigned preferredFrameSize,
unsigned playTimePerFrame)
{
	FILE* fid = OpenInputFile(env, fileName);
	if (fid == NULL) return NULL;

	ByteStreamFileSource* newSource
		= new ByteStreamFileSource(env, fid, preferredFrameSize, playTimePerFrame);
	newSource->fFileSize = GetFileSize(fileName, fid);

	return newSource;
}

ByteStreamFileSource*
ByteStreamFileSource::createNew(UsageEnvironment& env, FILE* fid,
unsigned preferredFrameSize,
unsigned playTimePerFrame)
{
	if (fid == NULL) return NULL;

	ByteStreamFileSource* newSource = new ByteStreamFileSource(env, fid, preferredFrameSize, playTimePerFrame);
	newSource->fFileSize = GetFileSize(NULL, fid);

	return newSource;
}

void ByteStreamFileSource::seekToByteAbsolute(u_int64_t byteNumber, u_int64_t numBytesToStream)
{
	SeekFile64(fFid, (int64_t)byteNumber, SEEK_SET);
	//更新可到流字节数
	fNumBytesToStream = numBytesToStream;
	fLimitNumBytesToStream = fNumBytesToStream > 0;
}

void ByteStreamFileSource::seekToByteRelative(int64_t offset)
{
	SeekFile64(fFid, offset, SEEK_CUR);
}

void ByteStreamFileSource::seekToEnd()
{
	SeekFile64(fFid, 0, SEEK_END);
}

ByteStreamFileSource::ByteStreamFileSource(UsageEnvironment& env, FILE* fid,
	unsigned preferredFrameSize,
	unsigned playTimePerFrame)
	: FramedFileSource(env, fid), fFileSize(0), fPreferredFrameSize(preferredFrameSize),
	fPlayTimePerFrame(playTimePerFrame), fLastPlayTime(0),
	fHaveStartedReading(False), fLimitNumBytesToStream(False), fNumBytesToStream(0)
{
#ifndef READ_FROM_FILES_SYNCHRONOUSLY	//阻塞读模式
	makeSocketNonBlocking(fileno(fFid));
#endif

	// Test whether the file is seekable测试文件是否为可seek定位
	if (SeekFile64(fFid, 1, SEEK_CUR) >= 0) {
		fFidIsSeekable = True;
		SeekFile64(fFid, -1, SEEK_CUR);
	}
	else {
		fFidIsSeekable = False;
	}
}

ByteStreamFileSource::~ByteStreamFileSource()
{
	if (fFid == NULL) return;

#ifndef READ_FROM_FILES_SYNCHRONOUSLY
	envir().taskScheduler().turnOffBackgroundReadHandling(fileno(fFid));	//如果是异步模式，关闭后台处理读操作
#endif

	CloseInputFile(fFid);	//关闭文件(非stdin)
}

void ByteStreamFileSource::doGetNextFrame()
{
	if (feof(fFid) || ferror(fFid) || (fLimitNumBytesToStream && fNumBytesToStream == 0)) {
		handleClosure(this);	//文件读取完了，处理关闭时操作
		return;
	}

#ifdef READ_FROM_FILES_SYNCHRONOUSLY	//阻塞模式
	doReadFromFile();	//执行读文件
#else	//非阻塞模式
	if (!fHaveStartedReading) {
		// Await readable data from the file:等待从文件读取数据：
		envir().taskScheduler().turnOnBackgroundReadHandling(fileno(fFid)/*mask*/,
			(TaskScheduler::BackgroundHandlerProc*)&fileReadableHandler, this/*source*/);
		fHaveStartedReading = True;
	}
#endif
}

void ByteStreamFileSource::doStopGettingFrames()
{
#ifndef READ_FROM_FILES_SYNCHRONOUSLY
	// 如果不是阻塞模式，那么从任务调度器中关闭对fileno(fFid)的后台读处理
	envir().taskScheduler().turnOffBackgroundReadHandling(fileno(fFid));
	fHaveStartedReading = False;
#endif
}

void ByteStreamFileSource::fileReadableHandler(ByteStreamFileSource* source, int /*mask*/)
{
	if (!source->isCurrentlyAwaitingData()) {
		// source当前没有正在读取数据，停止获取帧
		source->doStopGettingFrames(); // we're not ready for the data yet 目前我们还没有准备好数据
		return;
	}
	// 从文件读取数据
	source->doReadFromFile();
}

void ByteStreamFileSource::doReadFromFile()
{
	// Try to read as many bytes as will fit in the buffer provided (or "fPreferredFrameSize" if less)
	// 尝试读取的字节数能够容纳在提供的buffer(或它小于最佳帧尺寸)
	if (fLimitNumBytesToStream && fNumBytesToStream < (u_int64_t)fMaxSize) {
		fMaxSize = (unsigned)fNumBytesToStream;
	}
	if (fPreferredFrameSize > 0 && fPreferredFrameSize < fMaxSize) {
		fMaxSize = fPreferredFrameSize;
	}
#ifdef READ_FROM_FILES_SYNCHRONOUSLY	//阻塞读取
	fFrameSize = fread(fTo, 1, fMaxSize, fFid);
#else
	if (fFidIsSeekable) {
		fFrameSize = fread(fTo, 1, fMaxSize, fFid);
	} else {
		// For non-seekable files (e.g., pipes), call "read()" rather than "fread()", to ensure that the read doesn't block:
		// 对于非可seek定位的文件(例如：管道),调用"read()"而不是"fread()"以确保该读不会阻塞：
		fFrameSize = read(fileno(fFid), fTo, fMaxSize);
	}
#endif
	if (fFrameSize == 0) {	//没有读取到数据(文件结尾)
		handleClosure(this);
		return;
	}
	fNumBytesToStream -= fFrameSize;	//可到流的字节数更新

	// Set the 'presentation time':设置'呈现'时间
	if (fPlayTimePerFrame > 0 && fPreferredFrameSize > 0) {
		if (fPresentationTime.tv_sec == 0 && fPresentationTime.tv_usec == 0) {
			// This is the first frame, so use the current time:
			gettimeofday(&fPresentationTime, NULL);
		}
		else {
			// Increment by the play time of the previous data:
			unsigned uSeconds = fPresentationTime.tv_usec + fLastPlayTime;
			fPresentationTime.tv_sec += uSeconds / 1000000;
			fPresentationTime.tv_usec = uSeconds % 1000000;
		}

		// Remember the play time of this data:
		fLastPlayTime = (fPlayTimePerFrame*fFrameSize) / fPreferredFrameSize;
		fDurationInMicroseconds = fLastPlayTime;
	}
	else {
		// We don't know a specific play time duration for this data,
		// so just record the current time as being the 'presentation time':
		gettimeofday(&fPresentationTime, NULL);
	}

	// Inform the reader that he has data:
#ifdef READ_FROM_FILES_SYNCHRONOUSLY
	// To avoid possible infinite recursion, we need to return to the event loop to do this:
	// 为了避免可能出现的无限递归，我们需要回到事件循环执行此操作：
	nextTask() = envir().taskScheduler().scheduleDelayedTask(0,
		(TaskFunc*)FramedSource::afterGetting, this);
#else
	// Because the file read was done from the event loop, we can call the
	// 'after getting' function directly, without risk of infinite recursion:
	//因为文件的读取是在事件循环完成后，我们可以直接调用函数afterGetting，没有无限递归的危险：
	FramedSource::afterGetting(this);
#endif
}
