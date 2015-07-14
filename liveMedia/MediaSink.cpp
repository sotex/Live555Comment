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
// Media Sinks
// Implementation

#include "MediaSink.hh"
#include "GroupsockHelper.hh"
#include <string.h>

////////// MediaSink //////////

MediaSink::MediaSink(UsageEnvironment& env)
: Medium(env), fSource(NULL)
{
}

MediaSink::~MediaSink()
{
	stopPlaying();
}

Boolean MediaSink::isSink() const
{
	return True;
}

Boolean MediaSink::lookupByName(UsageEnvironment& env, char const* sinkName,
	MediaSink*& resultSink)
{
	resultSink = NULL; // unless we succeed

	Medium* medium;
	if (!Medium::lookupByName(env, sinkName, medium)) return False;

	if (!medium->isSink()) {
		env.setResultMsg(sinkName, " is not a media sink");
		return False;
	}

	resultSink = (MediaSink*)medium;
	return True;
}

Boolean MediaSink::sourceIsCompatibleWithUs(MediaSource& source)
{
	// We currently support only framed sources.����Ŀǰֻ֧��֡Դ��
	return source.isFramedSource();
}

Boolean MediaSink::startPlaying(MediaSource& source,
	afterPlayingFunc* afterFunc,
	void* afterClientData)
{
	// Make sure we're not already being played://ȷ�����ǲ����Ѿ����ڲ���
	if (fSource != NULL) {
		envir().setResultMsg("This sink is already being played");
		return False;
	}

	// Make sure our source is compatible:ȷ�����ǵ�Դ����
	if (!sourceIsCompatibleWithUs(source)) {
		envir().setResultMsg("MediaSink::startPlaying(): source is not compatible!");
		return False;
	}
	// ����֡Դ
	fSource = (FramedSource*)&source;

	fAfterFunc = afterFunc;	// ���ò��ź����
	fAfterClientData = afterClientData;	//���ò��ź��������
	return continuePlaying();
}

void MediaSink::stopPlaying()
{
	// First, tell the source that we're no longer interested:
	// ���ȣ�֪ͨ���ǲ���ȥ�Ի�ȡ֡Դ
	if (fSource != NULL) fSource->stopGettingFrames();

	// Cancel any pending tasks:ȡ�����д���������
	envir().taskScheduler().unscheduleDelayedTask(nextTask());
	nextTask() = NULL;

	fSource = NULL; // indicates that we can be played again���ǿ����ٴ�ʹ�ò���
	fAfterFunc = NULL;
}

void MediaSink::onSourceClosure(void* clientData)
{
	MediaSink* sink = (MediaSink*)clientData;
	sink->fSource = NULL; // indicates that we can be played again�������ǿ����ٴβ���
	if (sink->fAfterFunc != NULL) {
		// ���ò��ź�ص�
		(*(sink->fAfterFunc))(sink->fAfterClientData);
	}
}

Boolean MediaSink::isRTPSink() const
{
	return False; // default implementation
}

////////// OutPacketBuffer //////////

unsigned OutPacketBuffer::maxSize = 60000; // by default

OutPacketBuffer::OutPacketBuffer(unsigned preferredPacketSize,
	unsigned maxPacketSize)
	: fPreferred(preferredPacketSize), fMax(maxPacketSize),
	fOverflowDataSize(0)
{	//������(maxSize�������������ݳߴ�)������Ľ����maxSize�������ɵİ�������ȡ��
	unsigned maxNumPackets = (maxSize /*60000*/ + (maxPacketSize - 1)) / maxPacketSize;
	// ���ޣ�������*����С
	fLimit = maxNumPackets*maxPacketSize;
	fBuf = new unsigned char[fLimit];	//buffer�Ĵ�С�Ǽ��޴�С
	resetPacketStart();	//�������ʼ
	resetOffset();	//����ƫ��
	resetOverflowData();	//�����������
}

OutPacketBuffer::~OutPacketBuffer()
{
	delete[] fBuf;
}

void OutPacketBuffer::enqueue(unsigned char const* from, unsigned numBytes)
{
	// ������ӵĳߴ����ܵĿ��óߴ�֮��
	if (numBytes > totalBytesAvailable()) {
#ifdef DEBUG
		fprintf(stderr, "OutPacketBuffer::enqueue() warning: %d > %d\n", numBytes, totalBytesAvailable());
#endif
		numBytes = totalBytesAvailable();
	}
	// ��ȫ�Ľ�from���ݿ�����fbuf[fPacketStart+fCurOffset]֮��λ��
	// ע��memmove�ǿ������ص���
	if (curPtr() != from) memmove(curPtr(), from, numBytes);
	// ��������С
	increment(numBytes);
}

void OutPacketBuffer::enqueueWord(u_int32_t word)
{
	u_int32_t nWord = htonl(word);
	enqueue((unsigned char*)&nWord, 4);
}

void OutPacketBuffer::insert(unsigned char const* from, unsigned numBytes,
	unsigned toPosition)
{
	// ��λҪ�����λ��
	unsigned realToPosition = fPacketStart + toPosition;
	if (realToPosition + numBytes > fLimit) {
		if (realToPosition > fLimit) return; // we can't do this����Ϊ֮
		numBytes = fLimit - realToPosition;	//�������������ܳ�������
	}
	// ��ȫ�Ĳ�������
	memmove(&fBuf[realToPosition], from, numBytes);
	// ���ð�β
	if (toPosition + numBytes > fCurOffset) {
		fCurOffset = toPosition + numBytes;
	}
}

void OutPacketBuffer::insertWord(u_int32_t word, unsigned toPosition)
{
	u_int32_t nWord = htonl(word);
	insert((unsigned char*)&nWord, 4, toPosition);
}

void OutPacketBuffer::extract(unsigned char* to, unsigned numBytes,
	unsigned fromPosition)
{
	// ��ʵ����ʼ��ȡλ��
	unsigned realFromPosition = fPacketStart + fromPosition;
	if (realFromPosition + numBytes > fLimit) { // sanity check�����Լ��
		if (realFromPosition > fLimit) return; // we can't do this
		numBytes = fLimit - realFromPosition;	//��ȡ�Ĳ��ֲ����ڼ���λ�ú�
	}
	// ��ʼ��ȡ����ȫ��
	memmove(to, &fBuf[realFromPosition], numBytes);
}

u_int32_t OutPacketBuffer::extractWord(unsigned fromPosition)
{
	u_int32_t nWord;
	extract((unsigned char*)&nWord, 4, fromPosition);
	return ntohl(nWord);
}

void OutPacketBuffer::skipBytes(unsigned numBytes)
{
	if (numBytes > totalBytesAvailable()) {
		numBytes = totalBytesAvailable();
	}

	increment(numBytes);
}

void OutPacketBuffer
::setOverflowData(unsigned overflowDataOffset,
unsigned overflowDataSize,
struct timeval const& presentationTime,
	unsigned durationInMicroseconds)
{
	fOverflowDataOffset = overflowDataOffset;
	fOverflowDataSize = overflowDataSize;
	fOverflowPresentationTime = presentationTime;
	fOverflowDurationInMicroseconds = durationInMicroseconds;
}

void OutPacketBuffer::useOverflowData()
{
	enqueue(&fBuf[fPacketStart + fOverflowDataOffset], fOverflowDataSize);
	fCurOffset -= fOverflowDataSize; // undoes increment performed by "enqueue"��������ӡ�ִ�е�����
	resetOverflowData();
	/*
	���ʹ��ǰ(������ݲ���һ���ڵ�ǰ�����ݺ�Ҳ�����ص���)
	fBuf [_____|��ǰ������ |_____________|�������|_________]
               |         |             |  (�����=fOverflowDataSize)
	fPacketStart  fCurOffset     fOverflowDataOffset

	���ʹ�ú�
	fBuf [_____|��ǰ������ |�������|______________________]
               |         |
	fPacketStart  fCurOffset
	���ƫ��fOverflowDataOffset=0
	�����  fOverflowDataSize=0
	*/
}

void OutPacketBuffer::adjustPacketStart(unsigned numBytes)
{
	fPacketStart += numBytes;
	if (fOverflowDataOffset >= numBytes) {
		fOverflowDataOffset -= numBytes;
	}
	else {
		fOverflowDataOffset = 0;
		fOverflowDataSize = 0; // an error otherwiseһ�����󣬷���
	}
}

void OutPacketBuffer::resetPacketStart()
{
	if (fOverflowDataSize > 0) {
		fOverflowDataOffset += fPacketStart;
	}
	fPacketStart = 0;
}
