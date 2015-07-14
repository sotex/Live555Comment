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
	//���¿ɵ����ֽ���
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
#ifndef READ_FROM_FILES_SYNCHRONOUSLY	//������ģʽ
	makeSocketNonBlocking(fileno(fFid));
#endif

	// Test whether the file is seekable�����ļ��Ƿ�Ϊ��seek��λ
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
	envir().taskScheduler().turnOffBackgroundReadHandling(fileno(fFid));	//������첽ģʽ���رպ�̨���������
#endif

	CloseInputFile(fFid);	//�ر��ļ�(��stdin)
}

void ByteStreamFileSource::doGetNextFrame()
{
	if (feof(fFid) || ferror(fFid) || (fLimitNumBytesToStream && fNumBytesToStream == 0)) {
		handleClosure(this);	//�ļ���ȡ���ˣ�����ر�ʱ����
		return;
	}

#ifdef READ_FROM_FILES_SYNCHRONOUSLY	//����ģʽ
	doReadFromFile();	//ִ�ж��ļ�
#else	//������ģʽ
	if (!fHaveStartedReading) {
		// Await readable data from the file:�ȴ����ļ���ȡ���ݣ�
		envir().taskScheduler().turnOnBackgroundReadHandling(fileno(fFid)/*mask*/,
			(TaskScheduler::BackgroundHandlerProc*)&fileReadableHandler, this/*source*/);
		fHaveStartedReading = True;
	}
#endif
}

void ByteStreamFileSource::doStopGettingFrames()
{
#ifndef READ_FROM_FILES_SYNCHRONOUSLY
	// �����������ģʽ����ô������������йرն�fileno(fFid)�ĺ�̨������
	envir().taskScheduler().turnOffBackgroundReadHandling(fileno(fFid));
	fHaveStartedReading = False;
#endif
}

void ByteStreamFileSource::fileReadableHandler(ByteStreamFileSource* source, int /*mask*/)
{
	if (!source->isCurrentlyAwaitingData()) {
		// source��ǰû�����ڶ�ȡ���ݣ�ֹͣ��ȡ֡
		source->doStopGettingFrames(); // we're not ready for the data yet Ŀǰ���ǻ�û��׼��������
		return;
	}
	// ���ļ���ȡ����
	source->doReadFromFile();
}

void ByteStreamFileSource::doReadFromFile()
{
	// Try to read as many bytes as will fit in the buffer provided (or "fPreferredFrameSize" if less)
	// ���Զ�ȡ���ֽ����ܹ��������ṩ��buffer(����С�����֡�ߴ�)
	if (fLimitNumBytesToStream && fNumBytesToStream < (u_int64_t)fMaxSize) {
		fMaxSize = (unsigned)fNumBytesToStream;
	}
	if (fPreferredFrameSize > 0 && fPreferredFrameSize < fMaxSize) {
		fMaxSize = fPreferredFrameSize;
	}
#ifdef READ_FROM_FILES_SYNCHRONOUSLY	//������ȡ
	fFrameSize = fread(fTo, 1, fMaxSize, fFid);
#else
	if (fFidIsSeekable) {
		fFrameSize = fread(fTo, 1, fMaxSize, fFid);
	} else {
		// For non-seekable files (e.g., pipes), call "read()" rather than "fread()", to ensure that the read doesn't block:
		// ���ڷǿ�seek��λ���ļ�(���磺�ܵ�),����"read()"������"fread()"��ȷ���ö�����������
		fFrameSize = read(fileno(fFid), fTo, fMaxSize);
	}
#endif
	if (fFrameSize == 0) {	//û�ж�ȡ������(�ļ���β)
		handleClosure(this);
		return;
	}
	fNumBytesToStream -= fFrameSize;	//�ɵ������ֽ�������

	// Set the 'presentation time':����'����'ʱ��
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
	// Ϊ�˱�����ܳ��ֵ����޵ݹ飬������Ҫ�ص��¼�ѭ��ִ�д˲�����
	nextTask() = envir().taskScheduler().scheduleDelayedTask(0,
		(TaskFunc*)FramedSource::afterGetting, this);
#else
	// Because the file read was done from the event loop, we can call the
	// 'after getting' function directly, without risk of infinite recursion:
	//��Ϊ�ļ��Ķ�ȡ�����¼�ѭ����ɺ����ǿ���ֱ�ӵ��ú���afterGetting��û�����޵ݹ��Σ�գ�
	FramedSource::afterGetting(this);
#endif
}
