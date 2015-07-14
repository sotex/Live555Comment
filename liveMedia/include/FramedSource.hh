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

// ֡Դ
class FramedSource : public MediaSource {
public:
	// ��env.liveMedia->mediaTable�в���sourceName�����FramedSource
	static Boolean lookupByName(UsageEnvironment& env, char const* sourceName,
		FramedSource*& resultSource);

	// ���Ͷ���  (��ȡ��...)
	typedef void (afterGettingFunc)(void* clientData, unsigned frameSize,
		unsigned numTruncatedBytes,	/*�ضϵ��ֽ���*/
	struct timeval presentationTime,	/*��ʾʱ��*/
		unsigned durationInMicroseconds);	/*����ʱ��*/
	// ���Ͷ���	(�ر�ʱ...)
	typedef void (onCloseFunc)(void* clientData);

	// ��������ֵ����Ӧ�ĳ�Ա����(Ŀǰ���ڵȴ���ȡʱ).Ȼ�����doGetNextFrame(���麯������������ʵ��)
	void getNextFrame(unsigned char* to/*Ŀ��λ��*/, unsigned maxSize,
		afterGettingFunc* afterGettingFunc /*��ȡ��ص�*/,
		void* afterGettingClientData /*��ȡ��ص������Ĳ���*/,
		onCloseFunc* onCloseFunc /* �ڹر�ʱ�ص� */,
		void* onCloseClientData);

	// �ر�ʱ����
	static void handleClosure(void* clientData);
	// This should be called (on ourself) if the source is discovered
	// to be closed (i.e., no longer readable)
	// �⽫�ڣ��������Լ�������Դ(source)���ر�ʱ���ã��������ٿɶ���

	// ֹͣ��ȡ֡
	void stopGettingFrames();

	virtual unsigned maxFrameSize() const;
	// size of the largest possible frame that we may serve, or 0
	// if no such maximum is known (default)
	// ���֡size�����ǿ���serve������0�����û����֪�����ֵ(Ĭ��)

	virtual void doGetNextFrame() = 0;
	// called by getNextFrame() ��getNextFrame����

	Boolean isCurrentlyAwaitingData() const { return fIsCurrentlyAwaitingData; }

	static void afterGetting(FramedSource* source);
	// doGetNextFrame() should arrange for this to be called after the
	// frame has been read (*iff* it is read successfully)
	// doGetNextFrame()Ӧ�����ڴ˱����ã�֡�ѱ���ȡ��*���ҽ���*���ǳɹ���ȡ��

protected:
	FramedSource(UsageEnvironment& env); // abstract base class
	virtual ~FramedSource();

	virtual void doStopGettingFrames();

protected:
	// The following variables are typically accessed/set by doGetNextFrame()
	// ͨ��doGetNextFrame()ȥ���ʻ���������ı���
	unsigned char* fTo; // in
	unsigned fMaxSize; // in	���size
	unsigned fFrameSize; // out	֡size
	unsigned fNumTruncatedBytes; // out	�ضϵ��ֽ���
	struct timeval fPresentationTime; // out	��ʾʱ��
	unsigned fDurationInMicroseconds; // out	����ʱ����(΢��)

private:
	// redefined virtual functions:
	virtual Boolean isFramedSource() const;

private:
	afterGettingFunc* fAfterGettingFunc;	//��ȡ֮�����
	void* fAfterGettingClientData;
	onCloseFunc* fOnCloseFunc;	//�ر�ʱ����
	void* fOnCloseClientData;

	Boolean fIsCurrentlyAwaitingData;	//Ŀǰ���ڵȴ����ݣ�
};

#endif
