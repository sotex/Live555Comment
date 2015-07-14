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
// Implementation

#include "MultiFramedRTPSink.hh"
#include "GroupsockHelper.hh"

////////// MultiFramedRTPSink //////////

void MultiFramedRTPSink::setPacketSizes(unsigned preferredPacketSize,
	unsigned maxPacketSize)
{
	if (preferredPacketSize > maxPacketSize || preferredPacketSize == 0) return;
	// sanity check �����Լ��

	delete fOutBuf;
	fOutBuf = new OutPacketBuffer(preferredPacketSize, maxPacketSize);
	fOurMaxPacketSize = maxPacketSize; // save value, in case subclasses need it ����ֵ�����������Ҫ��
}

MultiFramedRTPSink::MultiFramedRTPSink(UsageEnvironment& env,
	Groupsock* rtpGS,
	unsigned char rtpPayloadType,
	unsigned rtpTimestampFrequency,
	char const* rtpPayloadFormatName,
	unsigned numChannels)
	: RTPSink(env, rtpGS, rtpPayloadType, rtpTimestampFrequency,
	rtpPayloadFormatName, numChannels)/*���û��๹��*/,
	fOutBuf(NULL), fCurFragmentationOffset(0), fPreviousFrameEndedFragmentation(False),
	fOnSendErrorFunc(NULL), fOnSendErrorData(NULL)
{
	setPacketSizes(1000, 1448);
	// Default max packet size (1500, minus allowance for IP, UDP, UMTP headers)
	// Ĭ�ϵ�������ݰ���С��MTU1500����ȥIP(20)��UDP��UMTPͷ������
	// (Also, make it a multiple of 4 bytes, just in case that matters.)
	// �����⣬ʹ֮Ϊ4�ֽڵı������Է���һ�������
}

MultiFramedRTPSink::~MultiFramedRTPSink()
{
	delete fOutBuf;	//�ͷ����������
}

void MultiFramedRTPSink
::doSpecialFrameHandling(unsigned /*fragmentationOffset*/,
unsigned char* /*frameStart*/,
unsigned /*numBytesInFrame*/,
struct timeval framePresentationTime,
	unsigned /*numRemainingBytes*/)
{
	// default implementation: If this is the first frame in the packet,
	// use its presentationTime for the RTP timestamp:
	// ȱʡ��ʵ�֣���������ڷ���ĵ�һ֡�У�ʹ�����ĳ��ֵ�ʱ��ΪRTPʱ�����
	if (isFirstFrameInPacket()) {
		setTimestamp(framePresentationTime);
	}
}

Boolean MultiFramedRTPSink::allowFragmentationAfterStart() const
{
	return False; // by default
}

Boolean MultiFramedRTPSink::allowOtherFramesAfterLastFragment() const
{
	return False; // by default
}

Boolean MultiFramedRTPSink
::frameCanAppearAfterPacketStart(unsigned char const* /*frameStart*/,
unsigned /*numBytesInFrame*/) const
{
	return True; // by default
}

unsigned MultiFramedRTPSink::specialHeaderSize() const
{
	// default implementation: Assume no special header:
	return 0;
}

unsigned MultiFramedRTPSink::frameSpecificHeaderSize() const
{
	// default implementation: Assume no frame-specific header:
	return 0;
}

unsigned MultiFramedRTPSink::computeOverflowForNewFrame(unsigned newFrameSize) const
{
	// default implementation: Just call numOverflowBytes()
	return fOutBuf->numOverflowBytes(newFrameSize);
}

void MultiFramedRTPSink::setMarkerBit()
{
	unsigned rtpHdr = fOutBuf->extractWord(0);
	rtpHdr |= 0x00800000;
	fOutBuf->insertWord(rtpHdr, 0);
}

void MultiFramedRTPSink::setTimestamp(struct timeval framePresentationTime)
{
	// First, convert the presentatoin time to a 32-bit RTP timestamp:
	fCurrentTimestamp = convertToRTPTimestamp(framePresentationTime);

	// Then, insert it into the RTP packet:
	fOutBuf->insertWord(fCurrentTimestamp, fTimestampPosition);
}

void MultiFramedRTPSink::setSpecialHeaderWord(unsigned word,
	unsigned wordPosition)
{
	fOutBuf->insertWord(word, fSpecialHeaderPosition + 4 * wordPosition);
}

void MultiFramedRTPSink::setSpecialHeaderBytes(unsigned char const* bytes,
	unsigned numBytes,
	unsigned bytePosition)
{
	fOutBuf->insert(bytes, numBytes, fSpecialHeaderPosition + bytePosition);
}

void MultiFramedRTPSink::setFrameSpecificHeaderWord(unsigned word,
	unsigned wordPosition)
{
	fOutBuf->insertWord(word, fCurFrameSpecificHeaderPosition + 4 * wordPosition);
}

void MultiFramedRTPSink::setFrameSpecificHeaderBytes(unsigned char const* bytes,
	unsigned numBytes,
	unsigned bytePosition)
{
	fOutBuf->insert(bytes, numBytes, fCurFrameSpecificHeaderPosition + bytePosition);
}

void MultiFramedRTPSink::setFramePadding(unsigned numPaddingBytes)
{
	if (numPaddingBytes > 0) {
		// Add the padding bytes (with the last one being the padding size):
		// �������ֽ�(���������һ���ֽ��������ֽ���ֵ)
		unsigned char paddingBuffer[255]; //max padding������
		memset(paddingBuffer, 0, numPaddingBytes);
		paddingBuffer[numPaddingBytes - 1] = numPaddingBytes;
		// ��ӵ��������
		fOutBuf->enqueue(paddingBuffer, numPaddingBytes);

		// Set the RTP padding bit:����RTP���λ
		unsigned rtpHdr = fOutBuf->extractWord(0);
		rtpHdr |= 0x20000000;
		fOutBuf->insertWord(rtpHdr, 0);
	}
}

Boolean MultiFramedRTPSink::continuePlaying()
{
	// Send the first packet.������һ����
	// (This will also schedule any future sends.)��Ҳ�����Ⱥ������κη���
	buildAndSendPacket(True);	//�����ͷ���(��һ��)��
	return True;
}

void MultiFramedRTPSink::stopPlaying()
{
	// �����������
	fOutBuf->resetPacketStart();
	fOutBuf->resetOffset();
	fOutBuf->resetOverflowData();

	// Then call the default "stopPlaying()" function:
	// Ȼ�����Ĭ�ϵ�stopPlaying()����

	// 1\ֹͣ��ȡ֡Դ��2\ֹͣ���д���������3\����Ϊ������
	MediaSink::stopPlaying();
}

void MultiFramedRTPSink::buildAndSendPacket(Boolean isFirstPacket)
{
	fIsFirstPacket = isFirstPacket;

	// Set up the RTP header:������RTP��ͷ��
	unsigned rtpHdr = 0x80000000; // RTP version 2; marker ('M') bit not set (by default; it can be set later)RTP�汾2;��ǣ�'M'��λδ���ã�Ĭ������£��������ں������ã�

	rtpHdr |= (fRTPPayloadType << 16);	// ������������
	rtpHdr |= fSeqNo; // sequence number ���к�
	fOutBuf->enqueueWord(rtpHdr);	//��RTPͷ����ӵ��������

	// Note where the RTP timestamp will go.
	// ע�����ڵ�RTPʱ������Թ���
	// (We can't fill this in until we start packing payload frames.)
	// �����ǲ�����������ֱ�����ǿ�ʼ�����Ч����֡����
	fTimestampPosition = fOutBuf->curPacketSize();	//��¼ʱ����ڰ���λ��
	fOutBuf->skipBytes(4); // leave a hole for the timestamp����һ����Ѩ����ʱ���

	fOutBuf->enqueueWord(SSRC());	// ���ͬ����Դ��ʶ������������������Դ

	// Allow for a special, payload-format-specific header following the
	// ����һ������ģ��ض����ظ�ʽ��ͷ����
	// RTP header:
	fSpecialHeaderPosition = fOutBuf->curPacketSize();	//��¼�ض�ͷ�ڰ���λ��
	fSpecialHeaderSize = specialHeaderSize();	// ��¼�ض�ͷ��С
	fOutBuf->skipBytes(fSpecialHeaderSize);	//���������¿ն�

	// Begin packing as many (complete) frames into the packet as we can:
	// ��ʼ��������ܶ�ģ������ģ�֡�����ݰ����������ܣ�
	fTotalFrameSpecificHeaderSizes = 0;
	fNoFramesLeft = False;
	fNumFramesUsedSoFar = 0;
	packFrame();	// ���
}

void MultiFramedRTPSink::packFrame()
{
	// Get the next frame.��ȡ��һ��֡

	// First, see if we have an overflow frame that was too big for the last pkt���ȣ����ǿ���������ݰ��Ƿ��й������֡
	if (fOutBuf->haveOverflowData()) {
		// Use this frame before reading a new one from the sourceʹ�ô�֡ǰ��Դ��ȡһ���µ�֡
		unsigned frameSize = fOutBuf->overflowDataSize();
		struct timeval presentationTime = fOutBuf->overflowPresentationTime();
		unsigned durationInMicroseconds = fOutBuf->overflowDurationInMicroseconds();
		fOutBuf->useOverflowData();

		afterGettingFrame1(frameSize, 0, presentationTime, durationInMicroseconds);
	}
	else {
		// Normal case: we need to read a new frame from the source
		// //��������£�������Ҫ��ȡ����Դ����֡
		if (fSource == NULL) return;

		fCurFrameSpecificHeaderPosition = fOutBuf->curPacketSize();
		fCurFrameSpecificHeaderSize = frameSpecificHeaderSize();
		fOutBuf->skipBytes(fCurFrameSpecificHeaderSize);
		fTotalFrameSpecificHeaderSizes += fCurFrameSpecificHeaderSize;
		// ��ȡ��һ��֡
		fSource->getNextFrame(fOutBuf->curPtr(), fOutBuf->totalBytesAvailable(),
			afterGettingFrame, this, ourHandleClosure, this);
	}
}

void MultiFramedRTPSink
::afterGettingFrame(void* clientData, unsigned numBytesRead,
unsigned numTruncatedBytes,
struct timeval presentationTime,
	unsigned durationInMicroseconds)
{
	MultiFramedRTPSink* sink = (MultiFramedRTPSink*)clientData;
	sink->afterGettingFrame1(numBytesRead, numTruncatedBytes,
		presentationTime, durationInMicroseconds);
}

void MultiFramedRTPSink
::afterGettingFrame1(unsigned frameSize, unsigned numTruncatedBytes,
struct timeval presentationTime,
	unsigned durationInMicroseconds)
{
	if (fIsFirstPacket) {
		// Record the fact that we're starting to play now:
		// ��¼�����ʵ�����Ǵ����ڿ�ʼ����
		gettimeofday(&fNextSendTime, NULL);
	}

	if (numTruncatedBytes > 0) {
		unsigned const bufferSize = fOutBuf->totalBytesAvailable();
		envir() << "MultiFramedRTPSink::afterGettingFrame1(): The input frame data was too large for our buffer size ("
			<< bufferSize << ").  "
			<< numTruncatedBytes << " bytes of trailing data was dropped!  Correct this by increasing \"OutPacketBuffer::maxSize\" to at least "
			<< OutPacketBuffer::maxSize + numTruncatedBytes << ", *before* creating this 'RTPSink'.  (Current value is "
			<< OutPacketBuffer::maxSize << ".)\n";
	}
	unsigned curFragmentationOffset = fCurFragmentationOffset;
	unsigned numFrameBytesToUse = frameSize;
	unsigned overflowBytes = 0;

	// If we have already packed one or more frames into this packet,
	// check whether this new frame is eligible to be packed after them.
	// ��������Ѿ����һ������֡��������ݰ����������µ�֡�Ƿ����ʸ��������ǽ��д����
	// (This is independent of whether the packet has enough room for this
	// new frame; that check comes later.)
	//�����Ƕ��������ݰ��Ƿ����㹻�Ŀռ�������һ�µ�֡�������󻰡���
	if (fNumFramesUsedSoFar > 0) {
		// �޷��������֡
		if ((fPreviousFrameEndedFragmentation
			&& !allowOtherFramesAfterLastFragment())
			|| !frameCanAppearAfterPacketStart(fOutBuf->curPtr(), frameSize)) {
			// Save away this frame for next time:
			// �����뿪�����֡����һ�Σ���ʾ������Ѿ����ˣ���һ���ٷ����֡��
			numFrameBytesToUse = 0;
			fOutBuf->setOverflowData(fOutBuf->curPacketSize(), frameSize,
				presentationTime, durationInMicroseconds);
		}
	}
	fPreviousFrameEndedFragmentation = False;	//��һ֡������Ƭ������Ϊ��

	if (numFrameBytesToUse > 0) {
		// Check whether this frame overflows the packet����֡�Ƿ�������ݰ�
		if (fOutBuf->wouldOverflow(frameSize)) {
			// Don't use this frame now; instead, save it as overflow data, and
			// send it in the next packet instead.  However, if the frame is too
			// big to fit in a packet by itself, then we need to fragment it (and
			// use some of it in this packet, if the payload format permits this.)
			// ���ڲ�����ʹ�����֡�����Ǳ�����Ϊ������ݣ�������һ�����з�������Ȼ����������֡������̫��ȥ��Ӧһ��������ô������Ҫȥ��Ƭ��������ʹ���������ݰ���һ���֣����������ظ�ʽ����
			if (isTooBigForAPacket(frameSize)
				&& (fNumFramesUsedSoFar == 0 || allowFragmentationAfterStart())) {
				// We need to fragment this frame, and use some of it now:
				overflowBytes = computeOverflowForNewFrame(frameSize);
				numFrameBytesToUse -= overflowBytes;
				fCurFragmentationOffset += numFrameBytesToUse;
			}
			else {
				// We don't use any of this frame now:
				overflowBytes = frameSize;
				numFrameBytesToUse = 0;
			}
			fOutBuf->setOverflowData(fOutBuf->curPacketSize() + numFrameBytesToUse,
				overflowBytes, presentationTime, durationInMicroseconds);
		}
		else if (fCurFragmentationOffset > 0) {
			// This is the last fragment of a frame that was fragmented over
			// more than one packet.  Do any special handling for this case:
			// �����ѷ�Ƭ�ڶ���һ�����е�֡�����һ��Ƭ�Ρ�ִ�������κ��������⴦�������
			fCurFragmentationOffset = 0;
			fPreviousFrameEndedFragmentation = True;
		}
	}

	if (numFrameBytesToUse == 0 && frameSize > 0) {
		// Send our packet now, because we have filled it up:
		// �������Ƿ������ݰ�����Ϊ�����Ѿ��������
		sendPacketIfNecessary();
	}
	else {
		// Use this frame in our outgoing packet:ʹ�����֡�����ǵ�������ݰ���
		unsigned char* frameStart = fOutBuf->curPtr();
		fOutBuf->increment(numFrameBytesToUse);
		// do this now, in case "doSpecialFrameHandling()" calls "setFramePadding()" to append padding bytes
		// ��������������һ��doSpecialFrameHandling���������á�setFramePadding��������׷������ֽ�

		// Here's where any payload format specific processing gets done:
		// �ڴ˴����κθ��ظ�ʽ�ض�������򣬵õ�ִ�У�
		doSpecialFrameHandling(curFragmentationOffset, frameStart,
			numFrameBytesToUse, presentationTime,
			overflowBytes);

		++fNumFramesUsedSoFar;

		// Update the time at which the next packet should be sent, based
		// on the duration of the frame that we just packed into it.
		// �ڴ˸�����һ�����ݰ�Ӧ�����͵�ʱ�䣬�������Ǹո���䵽���е�֡�ĳ���ʱ�䡣
		// However, if this frame has overflow data remaining, then don't
		// count its duration yet.
		// Ȼ����������֡��������ݲ��࣬��ô��Ҫͳ����Ŀǰ����ʱ��
		if (overflowBytes == 0) {
			fNextSendTime.tv_usec += durationInMicroseconds;
			fNextSendTime.tv_sec += fNextSendTime.tv_usec / 1000000;
			fNextSendTime.tv_usec %= 1000000;
		}

		// Send our packet now if (i) it's already at our preferred size, or
		// (ii) (heuristic) another frame of the same size as the one we just
		//      read would overflow the packet, or
		// (iii) it contains the last fragment of a fragmented frame, and we
		//      don't allow anything else to follow this or
		// (iv) one frame per packet is allowed:
		// �������(1)���Ѿ���������(��)��С����(2)(��̽)���Ǹոն�ȡ����ͬ����С֡����������������(3)������һ����Ƭ֡�����һ����Ƭ���������ǲ������κ������ĸ�������֮�󣬻�(4)һ�������֡(Ԫ/����)��
		if (fOutBuf->isPreferredSize()
			|| fOutBuf->wouldOverflow(numFrameBytesToUse)
			|| (fPreviousFrameEndedFragmentation &&
			!allowOtherFramesAfterLastFragment())
			|| !frameCanAppearAfterPacketStart(fOutBuf->curPtr() - frameSize,
			frameSize)) {
			// The packet is ready to be sent now �����ݰ�������׼��Ҫ����
			sendPacketIfNecessary();	//���Ͱ����������
		}
		else {
			// There's room for more frames; try getting another:
			// ���пռ��Ÿ����֡;���Ի����һ����
			packFrame();
		}
	}
}

static unsigned const rtpHeaderSize = 12;

Boolean MultiFramedRTPSink::isTooBigForAPacket(unsigned numBytes) const
{
	// Check whether a 'numBytes'-byte frame - together with a RTP header and
	// (possible) special headers - would be too big for an output packet:
	// (Later allow for RTP extension header!) #####
	numBytes += rtpHeaderSize + specialHeaderSize() + frameSpecificHeaderSize();
	return fOutBuf->isTooBigForAPacket(numBytes);
}

void MultiFramedRTPSink::sendPacketIfNecessary()
{
	if (fNumFramesUsedSoFar > 0) {
		// Send the packet:
#ifdef TEST_LOSS
		if ((our_random()%10) != 0) // simulate 10% packet loss #####
#endif
		if (!fRTPInterface.sendPacket(fOutBuf->packet(), fOutBuf->curPacketSize())) {
			// if failure handler has been specified, call it
			if (fOnSendErrorFunc != NULL) (*fOnSendErrorFunc)(fOnSendErrorData);
		}
		++fPacketCount;
		fTotalOctetCount += fOutBuf->curPacketSize();
		fOctetCount += fOutBuf->curPacketSize()
			- rtpHeaderSize - fSpecialHeaderSize - fTotalFrameSpecificHeaderSizes;

		++fSeqNo; // for next time
	}

	if (fOutBuf->haveOverflowData()
		&& fOutBuf->totalBytesAvailable() > fOutBuf->totalBufferSize() / 2) {
		// Efficiency hack: Reset the packet start pointer to just in front of
		// the overflow data (allowing for the RTP header and special headers),
		// so that we probably don't have to "memmove()" the overflow data
		// into place when building the next packet:
		unsigned newPacketStart = fOutBuf->curPacketSize()
			- (rtpHeaderSize + fSpecialHeaderSize + frameSpecificHeaderSize());
		fOutBuf->adjustPacketStart(newPacketStart);
	}
	else {
		// Normal case: Reset the packet start pointer back to the start:
		fOutBuf->resetPacketStart();
	}
	fOutBuf->resetOffset();
	fNumFramesUsedSoFar = 0;

	if (fNoFramesLeft) {
		// We're done:
		onSourceClosure(this);
	}
	else {
		// We have more frames left to send.  Figure out when the next frame
		// is due to start playing, then make sure that we wait this long before
		// sending the next packet.
		struct timeval timeNow;
		gettimeofday(&timeNow, NULL);
		int secsDiff = fNextSendTime.tv_sec - timeNow.tv_sec;
		int64_t uSecondsToGo = secsDiff * 1000000 + (fNextSendTime.tv_usec - timeNow.tv_usec);
		if (uSecondsToGo < 0 || secsDiff < 0) { // sanity check: Make sure that the time-to-delay is non-negative:
			uSecondsToGo = 0;
		}

		// Delay this amount of time:
		nextTask() = envir().taskScheduler().scheduleDelayedTask(uSecondsToGo, (TaskFunc*)sendNext, this);
	}
}

// The following is called after each delay between packet sends:
void MultiFramedRTPSink::sendNext(void* firstArg)
{
	MultiFramedRTPSink* sink = (MultiFramedRTPSink*)firstArg;
	sink->buildAndSendPacket(False);
}

void MultiFramedRTPSink::ourHandleClosure(void* clientData)
{
	MultiFramedRTPSink* sink = (MultiFramedRTPSink*)clientData;
	// There are no frames left, but we may have a partially built packet
	//  to send
	// û��ʣ���֡�������ǿ����в������ɵ����ݰ�����
	sink->fNoFramesLeft = True;
	sink->sendPacketIfNecessary();
}
