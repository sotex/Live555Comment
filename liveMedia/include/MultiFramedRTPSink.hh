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
// RTP sink for a common kind of payload format: Those which pack multiple,
// complete codec frames (as many as possible) into each RTP packet.
// C++ header

#ifndef _MULTI_FRAMED_RTP_SINK_HH
#define _MULTI_FRAMED_RTP_SINK_HH

#ifndef _RTP_SINK_HH
#include "RTPSink.hh"
#endif

class MultiFramedRTPSink : public RTPSink {
public:
	// ����fOutBuf����С
	void setPacketSizes(unsigned preferredPacketSize, unsigned maxPacketSize);
	// ���Ͷ���
	typedef void (onSendErrorFunc)(void* clientData);
	//���÷�������ʱ�ص�
	void setOnSendErrorFunc(onSendErrorFunc* onSendErrorFunc, void* onSendErrorFuncData)
	{
		// Can be used to set a callback function to be called if there's an error sending RTP packets on our socket.
		// ������������һ���ص���������������ǵ��׽����Ϸ���RTP����һ�����󣬽������á�
		fOnSendErrorFunc = onSendErrorFunc;
		fOnSendErrorData = onSendErrorFuncData;
	}

protected:
	// ���캯��(ע������protected��)
	MultiFramedRTPSink(UsageEnvironment& env,
		Groupsock* rtpgs, unsigned char rtpPayloadType,
		unsigned rtpTimestampFrequency,
		char const* rtpPayloadFormatName,
		unsigned numChannels = 1);
	// we're a virtual base class������һ�������

	virtual ~MultiFramedRTPSink();
	// ���ض�֡����
	virtual void doSpecialFrameHandling(unsigned fragmentationOffset,
		unsigned char* frameStart,
		unsigned numBytesInFrame,
	struct timeval framePresentationTime,
		unsigned numRemainingBytes);
	// perform any processing specific to the particular payload format//ִ���ض����κδ����������ض���Ч���ظ�ʽ

	// ���з�Ƭ�ڿ�ʼ֮��
	virtual Boolean allowFragmentationAfterStart() const;
	// whether a frame can be fragmented if other frame(s) appear earlier
	// in the packet (by default: False)
	// �����Ƿ���������֡,������������ݰ���֡���Է�Ƭ��Ĭ�ϣ��٣�

	// ��������֡�����һ��Ƭ�κ�?
	virtual Boolean allowOtherFramesAfterLastFragment() const;
	// whether other frames can be packed into a packet following the
	// final fragment of a previous, fragmented frame (by default: False)
	// ����֡�Ƿ�ɱ���װ��һ�����ݰ�����ǰ������Ƭ֡�����Ƭ�Σ�Ĭ�ϣ��٣�

	// ֡���Գ��������ݰ���ʼ֮��
	virtual Boolean frameCanAppearAfterPacketStart(unsigned char const* frameStart,
		unsigned numBytesInFrame) const;
	// whether this frame can appear in position >1 in a pkt (default: True)
	// ��֡�Ƿ���������ݰ�λ��position>1���֣�Ĭ�ϣ��棩

	// �ض�ͷ����С
	virtual unsigned specialHeaderSize() const;
	// returns the size of any special header used (following the RTP header) (default: 0)����ʹ���κ��ض���ͷ���Ĵ�С�������RTPͷ����Ĭ��ֵ��0��

	// �ض�֡ͷ����С
	virtual unsigned frameSpecificHeaderSize() const;
	// returns the size of any frame-specific header used (before each frame
	// within the packet) (default: 0)�������ڵ������ض�֡��ͷ����С�����ڵ�ÿ��֮֡ǰ����Ĭ��ֵ��0��

	// ������֡���
	virtual unsigned computeOverflowForNewFrame(unsigned newFrameSize) const;
	// returns the number of overflow bytes that would be produced by adding a new
	// frame of size "newFrameSize" to the current RTP packet.
	// (By default, this just calls "numOverflowBytes()", but subclasses can redefine
	// this to (e.g.) impose a granularity upon RTP payload fragments.)
	// ���ؽ����һ����newFrameSize����С���µ�֡����ǰRTP���в������ֽ�����
	//��Ĭ������£���ֻ�ǵ��á�numOverflowBytes��������������������¶����⣨���磩ǿ�Ӹ�RTP��Ч���ط�Ƭ���ȡ���

	// Functions that might be called by doSpecialFrameHandling(), or other subclass virtual functions:����������doSpecialFrameHandling()����������������⺯�����ã�

	// ��һ������
	Boolean isFirstPacket() const { return fIsFirstPacket; }
	// ���ݰ��ĵ�һ��֡��
	Boolean isFirstFrameInPacket() const { return fNumFramesUsedSoFar == 0; }
	// ��ǰ��Ƭƫ��
	Boolean curFragmentationOffset() const { return fCurFragmentationOffset; }
	// ���ñ��λ(RTPͷ��)
	void setMarkerBit();
	// ����ʱ���
	void setTimestamp(struct timeval framePresentationTime);
	// �����ض�ͷ��(4�ֽ�)
	void setSpecialHeaderWord(unsigned word, /* 32 bits, in host order */
		unsigned wordPosition = 0);
	// �����ض�ͷ��(numBytes�ֽ�)
	void setSpecialHeaderBytes(unsigned char const* bytes, unsigned numBytes,
		unsigned bytePosition = 0);
	// �趨�ض�֡ͷ��
	void setFrameSpecificHeaderWord(unsigned word, /* 32 bits, in host order */
		unsigned wordPosition = 0);
	// �趨�ض�֡ͷ��
	void setFrameSpecificHeaderBytes(unsigned char const* bytes, unsigned numBytes,
		unsigned bytePosition = 0);
	// ����֡���
	void setFramePadding(unsigned numPaddingBytes);
	// Ŀǰʹ��֡��
	unsigned numFramesUsedSoFar() const { return fNumFramesUsedSoFar; }
	// ������С
	unsigned ourMaxPacketSize() const { return fOurMaxPacketSize; }

public: // redefined virtual functions:
	// ֹͣ����
	virtual void stopPlaying();

protected: // redefined virtual functions:
	// ��������
	virtual Boolean continuePlaying();

private:
	void buildAndSendPacket(Boolean isFirstPacket);
	// ���֡(��ȡ��һ��֡����ӵ�packet)
	void packFrame();
	void sendPacketIfNecessary();
	// ��������������������أ�һ���ǳ�Ա��һ�����ǳ�Ա
	static void sendNext(void* firstArg);//��̬��
	friend void sendNext(void*);	//���Ǹ���Ԫ����

	
	static void afterGettingFrame(void* clientData,
		unsigned numBytesRead, unsigned numTruncatedBytes,
	struct timeval presentationTime,
		unsigned durationInMicroseconds);
	// �ڰ���δ�����װǰ����ȡ��һ֡�������������ɺ���з���
	void afterGettingFrame1(unsigned numBytesRead, unsigned numTruncatedBytes,
	struct timeval presentationTime,
		unsigned durationInMicroseconds);
	// �����Ƿ���ࣿ
	Boolean isTooBigForAPacket(unsigned numBytes) const;
	// �ر�ʱ����
	static void ourHandleClosure(void* clientData);

private:
	OutPacketBuffer* fOutBuf;	//�������

	Boolean fNoFramesLeft;	//
	unsigned fNumFramesUsedSoFar;	//Ŀǰʹ��֡��
	// (���һ����Ϣ��̫����޷�װ�أ����Ͳ��ò����ֳ�Ƭ��)
	unsigned fCurFragmentationOffset;	//��ǰƬ��ƫ��
	Boolean fPreviousFrameEndedFragmentation;	// ��һ֡������Ƭ

	/*
	��Ƭ�������������MTU��Maximum Transmission Units������䵥Ԫ�������磬���ƻ�����token ring����MTU��4464����̫����Ethernet����MTU��1500����ˣ����һ����Ϣ��Ҫ�����ƻ������䵽��̫��������Ҫ�����ѳ�һЩС��Ƭ�ϣ�Ȼ������Ŀ�ĵ��ؽ�����Ȼ������������Ч�ʽ��ͣ����Ƿ�Ƭ��Ч�����Ǻܺõġ��ڿͽ���Ƭ��Ϊ���IDS�ķ��������⻹��һЩDOS����Ҳʹ�÷�Ƭ������  
	*/


	Boolean fIsFirstPacket;	//�ǵ�һ������
	struct timeval fNextSendTime;	//��һ�η���ʱ��
	unsigned fTimestampPosition;	//ʱ���λ��
	unsigned fSpecialHeaderPosition;//�ض�ͷ��λ��
	unsigned fSpecialHeaderSize; // size in bytes of any special header used���κ��ض�ͷ��ʹ�õ��ֽڴ�С
	unsigned fCurFrameSpecificHeaderPosition;	//��ǰ֡�ض�ͷ��λ��
	unsigned fCurFrameSpecificHeaderSize; // size in bytes of cur frame-specific header��ǰ�ض�֡ͷ�����ֽڴ�С
	unsigned fTotalFrameSpecificHeaderSizes; // size of all frame-specific hdrs in pkt���е��ض�֡ͷ���ڰ��еĴ�С
	unsigned fOurMaxPacketSize;	//�����ߴ�

	onSendErrorFunc* fOnSendErrorFunc;	//�ڷ��ͳ���ʱ����
	void* fOnSendErrorData;
};

#endif
