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
// Implementation

#include "ByteStreamMultiFileSource.hh"

ByteStreamMultiFileSource
::ByteStreamMultiFileSource(UsageEnvironment& env, char const** fileNameArray,
unsigned preferredFrameSize, unsigned playTimePerFrame)
: FramedSource(env),
fPreferredFrameSize(preferredFrameSize), fPlayTimePerFrame(playTimePerFrame),
fCurrentlyReadSourceNumber(0), fHaveStartedNewFile(False)
{
	// Begin by counting the number of sources:开始通过计算源数
	for (fNumSources = 0;/**/; ++fNumSources) {
		if (fileNameArray[fNumSources] == NULL) break;
	}

	// Next, copy the source file names into our own array:
	// 接下来，复制源文件名到我们自己的数组：
	fFileNameArray = new char const*[fNumSources];
	if (fFileNameArray == NULL) return;
	unsigned i;
	for (i = 0; i < fNumSources; ++i) {
		fFileNameArray[i] = strDup(fileNameArray[i]);
	}

	// Next, set up our array of component ByteStreamFileSources
	// Don't actually create these yet; instead, do this on demand
	// 接下来，建立我们的ByteStreamFileSources的数组。实际上并不创建对象;而是为此随需应变
	fSourceArray = new ByteStreamFileSource*[fNumSources];
	if (fSourceArray == NULL) return;
	for (i = 0; i < fNumSources; ++i) {
		fSourceArray[i] = NULL;
	}
}

ByteStreamMultiFileSource::~ByteStreamMultiFileSource()
{
	unsigned i;
	for (i = 0; i < fNumSources; ++i) {
		Medium::close(fSourceArray[i]);	//关闭源
	}
	delete[] fSourceArray;	//释放源数组

	for (i = 0; i < fNumSources; ++i) {
		delete[](char*)(fFileNameArray[i]);	//释放源名称
	}
	delete[] fFileNameArray;	//释放源名称数组
}

ByteStreamMultiFileSource* ByteStreamMultiFileSource
::createNew(UsageEnvironment& env, char const** fileNameArray,
unsigned preferredFrameSize, unsigned playTimePerFrame)
{
	// 创建多文件字节流源
	ByteStreamMultiFileSource* newSource
		= new ByteStreamMultiFileSource(env, fileNameArray,
		preferredFrameSize, playTimePerFrame);

	return newSource;
}

void ByteStreamMultiFileSource::doGetNextFrame()
{
	do {
		// First, check whether we've run out of sources:首先，检查是否我们已经用完了来源
		if (fCurrentlyReadSourceNumber >= fNumSources) break;

		fHaveStartedNewFile = False;
		ByteStreamFileSource*& source
			= fSourceArray[fCurrentlyReadSourceNumber];
		if (source == NULL) {
			// The current source hasn't been created yet.  Do this now:
			// 当前源尚未创建。现在这样做：(创建文件字节流源)
			source = ByteStreamFileSource::createNew(envir(),
				fFileNameArray[fCurrentlyReadSourceNumber],
				fPreferredFrameSize, fPlayTimePerFrame);
			if (source == NULL) break;
			fHaveStartedNewFile = True;	//指示这是一个新开始的文件
		}

		// (Attempt to) read from the current source.（尝试）从当前源中读取
		source->getNextFrame(fTo, fMaxSize,
			afterGettingFrame/*获取一帧后调用赋值给source->fAfterGettingFunc*/, this,
			onSourceClosure/*关闭源时调用*/, this);
		/*
		这里要详细说一下，当source->getNextFrame调用的时候会
		source->fAfterGettingFunc = this->afterGettingFrame
		source->fAfterGettingClientData = this
		然后调用source->doGetNexFrame成功获取一帧数据的时候(见doReadFromFile()实现)，会调用
		FramedSource::afterGetting(this注意这个this是source)，进而有这样的调用source->fAfterGettingFunc))(source->fAfterGettingClientData,...
		那么就也就是调用了this->afterGettingFrame(this)
		而在this->afterGettingFrame中，对相关的成员进行了赋值，然后调用了FramedSource::afterGetting(source/*这实质是这里的this* /)
		于是就又去调用了this->fAfterGettingFunc))(this->fAfterGettingClientData,...
		应当是没有继续调用了的，因为在多文件字节流源类中没有对fAfterGettingFunc进行修改，其应为NULL
		*/

		return;
	} while (0);

	// An error occurred; consider ourselves closed://发生错误;认为自己该关闭：
	handleClosure(this);
}

void ByteStreamMultiFileSource
::afterGettingFrame(void* clientData,
unsigned frameSize, unsigned numTruncatedBytes,
struct timeval presentationTime,
	unsigned durationInMicroseconds)
{
	ByteStreamMultiFileSource* source
		= (ByteStreamMultiFileSource*)clientData;
	// 相关成员赋值
	source->fFrameSize = frameSize;
	source->fNumTruncatedBytes = numTruncatedBytes;
	source->fPresentationTime = presentationTime;
	source->fDurationInMicroseconds = durationInMicroseconds;
	// 真正的获取后调用(source->fAfterGettingFunc)注意两个fAfterGettingFunc隶属的对象不同
	FramedSource::afterGetting(source);
}

void ByteStreamMultiFileSource::onSourceClosure(void* clientData)
{
	ByteStreamMultiFileSource* source
		= (ByteStreamMultiFileSource*)clientData;
	source->onSourceClosure1();	//关闭当前源，指向下一个源
}

void ByteStreamMultiFileSource::onSourceClosure1()
{
	// This routine was called because the currently-read source was closed
	// (probably due to EOF).  Close this source down, and move to the
	// next one:
	// 这个例程被调用，因为当前读源被关闭（可能是由于EOF）。
	// 关闭这个来源下来，并移动到下一个：
	ByteStreamFileSource*& source
		= fSourceArray[fCurrentlyReadSourceNumber++];
	Medium::close(source);
	source = NULL;

	// Try reading again:尝试再次读取
	doGetNextFrame();
}
