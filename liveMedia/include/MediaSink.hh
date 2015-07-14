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
// C++ header

#ifndef _MEDIA_SINK_HH
#define _MEDIA_SINK_HH

#ifndef _FRAMED_SOURCE_HH
#include "FramedSource.hh"
#endif

//	ý���
class MediaSink : public Medium {
public:
	static Boolean lookupByName(UsageEnvironment& env, char const* sinkName,
		MediaSink*& resultSink);

	// �������ͣ����ź���(�ڹر�Դʱ����)
	typedef void (afterPlayingFunc)(void* clientData);

	Boolean startPlaying(MediaSource& source,
		afterPlayingFunc* afterFunc,
		void* afterClientData);

	virtual void stopPlaying();

	// Test for specific types of sink:
	virtual Boolean isRTPSink() const;

	FramedSource* source() const { return fSource; }

protected:
	MediaSink(UsageEnvironment& env); // abstract base class
	virtual ~MediaSink();

	// Դ�ܹ���������?
	virtual Boolean sourceIsCompatibleWithUs(MediaSource& source);
	// called by startPlaying()
	virtual Boolean continuePlaying() = 0;
	// called by startPlaying()

	// �ڹر�Դʱ
	static void onSourceClosure(void* clientData);
	// should be called (on ourselves) by continuePlaying() when it
	// discovers that the source we're playing from has closed.
	//Ӧ��continuePlaying()���ã������Լ����������������ǴӲ���Դ�Ѿ��ر��ˡ�

	FramedSource* fSource;	//֡Դ

private:
	// redefined virtual functions:
	virtual Boolean isSink() const;

private:
	// The following fields are used when we're being played:
	//�����ֶ����ڵ��������ڲ��ţ�
	afterPlayingFunc* fAfterFunc;	//���ź���ú���
	void* fAfterClientData;		//���ź���ú�������
};


//=====================================================================


// A data structure that a sink may use for an output packet:
// һ�����ղۿ�����������ݰ������ݽṹ
// ������ݰ�����
class OutPacketBuffer {
public:
	// ���캯����
	OutPacketBuffer(unsigned preferredPacketSize, unsigned maxPacketSize);
	~OutPacketBuffer();

	static unsigned maxSize;	//���Size(ע��static)
	// ��ǰ����ƫ��λ��
	unsigned char* curPtr() const { return &fBuf[fPacketStart + fCurOffset]; }

	// ���õ����ֽ���(fbuf[fPacketStart+fCurOffset]��Ķ����ڿ����ֽ�)
	unsigned totalBytesAvailable() const
	{
		return fLimit - (fPacketStart + fCurOffset);
	}
	// �ܵ�buffer��С
	unsigned totalBufferSize() const { return fLimit; }
	// ��ǰ��
	unsigned char* packet() const { return &fBuf[fPacketStart]; }
	// ��ǰ����С
	unsigned curPacketSize() const { return fCurOffset; }
	// ������ǰ��fCurOffset
	void increment(unsigned numBytes) { fCurOffset += numBytes; }
	// ���from���ݵ���ǰ��(numBytes�ֽڣ������������ֽڲ��ֱ�����)
	void enqueue(unsigned char const* from, unsigned numBytes);
	// ���word����������ʽ(4Byte)����ǰ��
	void enqueueWord(u_int32_t word);
	// ����from�е�����numBytes����ǰ����toPositionλ�ú󡣳����������ֽڲ��ֱ�����
	void insert(unsigned char const* from, unsigned numBytes, unsigned toPosition);
	// ����word����������ʽ����ǰ����toPositionλ��
	void insertWord(u_int32_t word, unsigned toPosition);
	// ��ȡ��ǰ����fromPositionλ�ú�numBytes���ݵ�to��(�����ȡ��flimitλ��)
	void extract(unsigned char* to, unsigned numBytes, unsigned fromPosition);
	// �ӵ�ǰ����fromPositionλ����ȡ4�ֽ����ݣ�תΪ�����򷵻�
	u_int32_t extractWord(unsigned fromPosition);
	// ����numBytes�Ŀ����ֽ�(fbuf[fPacketStart+fCurOffset]��Ķ����ڿ����ֽ�)
	void skipBytes(unsigned numBytes);
	// ��ǰƫ�ƴﵽ���?
	Boolean isPreferredSize() const { return fCurOffset >= fPreferred; }
	// �������?
	Boolean wouldOverflow(unsigned numBytes) const
	{
		return (fCurOffset + numBytes) > fMax;
	}
	//����ֽ���
	unsigned numOverflowBytes(unsigned numBytes) const
	{
		return (fCurOffset + numBytes) - fMax;
	}
	// ����İ�
	Boolean isTooBigForAPacket(unsigned numBytes) const
	{
		return numBytes > fMax;
	}
	// �����������(��Ϣ)
	void setOverflowData(unsigned overflowDataOffset,
		unsigned overflowDataSize,
	struct timeval const& presentationTime,
		unsigned durationInMicroseconds);
	// ��ȡ���������
	unsigned overflowDataSize() const { return fOverflowDataSize; }
	// ��ȡ�������ʱ��
	struct timeval overflowPresentationTime() const 
	{ return fOverflowPresentationTime; }
	
	unsigned overflowDurationInMicroseconds() const 
	{ return fOverflowDurationInMicroseconds; }
	// ��������ݣ�
	Boolean haveOverflowData() const { return fOverflowDataSize > 0; }
	// ��������ݲ�����ӵ���ǰ����������fCurOffset������
	void useOverflowData();
	// �������ݰ���ʼλ��(�������ƫ��Ҳ����Ӧ�ĵ���������ʼλ�õ��������ƫ��֮��ȡ���������)
	void adjustPacketStart(unsigned numBytes);
	// ���ð���ʼΪ0(����ԭ����Ϊ0�������������ƫ��Ϊ���ֵ)
	void resetPacketStart();
	// ���õ�ǰƫ��
	void resetOffset() { fCurOffset = 0; }
	// �����������Ϊ0
	void resetOverflowData() { fOverflowDataOffset = fOverflowDataSize = 0; }

private:
	unsigned fPacketStart, fCurOffset, fPreferred/*��Ѱ���С*/, fMax/*���*/, fLimit/*����*/;
	unsigned char* fBuf;	//������

	unsigned fOverflowDataOffset, fOverflowDataSize;	//�������ƫ�ƣ�������ݳ���
	struct timeval fOverflowPresentationTime;	//�������ʱ��
	unsigned fOverflowDurationInMicroseconds;	//���ʱ���΢������
};

#endif
