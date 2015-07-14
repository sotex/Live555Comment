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

// ���ļ��ֽ���Դ
class ByteStreamMultiFileSource : public FramedSource {
public:
	static ByteStreamMultiFileSource*
		createNew(UsageEnvironment& env, char const** fileNameArray,
		unsigned preferredFrameSize = 0, unsigned playTimePerFrame = 0);
	// A 'filename' of NULL indicates the end of the array
	// fileNameArray����Ľ�����һ��ΪNULL��Ԫ��

	Boolean haveStartedNewFile() const { return fHaveStartedNewFile; }
	// True iff the most recently delivered frame was the first from a newly-opened file
	// true ���������ṩ֡���״δ�һ���´򿪵��ļ�����

protected:
	ByteStreamMultiFileSource(UsageEnvironment& env, char const** fileNameArray,
		unsigned preferredFrameSize, unsigned playTimePerFrame);
	// called only by createNew()

	virtual ~ByteStreamMultiFileSource();

private:
	// redefined virtual functions:
	virtual void doGetNextFrame();

private:
	//	�ر�ʱ���ã�ע����static����(ԭ������Ϊ��Ϊ�ص�����)
	static void onSourceClosure(void* clientData);
	// �ر�ʱ����ʵ����
	void onSourceClosure1();
	// ��ȡһ֡����ã���Ϊ�ص�ʹ��
	static void afterGettingFrame(void* clientData,
		unsigned frameSize, unsigned numTruncatedBytes,
	struct timeval presentationTime,
		unsigned durationInMicroseconds);

private:
	unsigned fPreferredFrameSize;	//���֡��С
	unsigned fPlayTimePerFrame;		//ÿ֡����ʱ��
	unsigned fNumSources;	//Դ���
	unsigned fCurrentlyReadSourceNumber;	//��ǰ��ȡԴ���
	Boolean fHaveStartedNewFile;	//��ʼ���ļ���
	char const** fFileNameArray;	//�ļ�������
	ByteStreamFileSource** fSourceArray;	//�ļ��ֽ���Դ����
};

#endif
