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
	// sanity check 完整性检查

	delete fOutBuf;
	fOutBuf = new OutPacketBuffer(preferredPacketSize, maxPacketSize);
	fOurMaxPacketSize = maxPacketSize; // save value, in case subclasses need it 保存值，如果子类需要它
}

MultiFramedRTPSink::MultiFramedRTPSink(UsageEnvironment& env,
	Groupsock* rtpGS,
	unsigned char rtpPayloadType,
	unsigned rtpTimestampFrequency,
	char const* rtpPayloadFormatName,
	unsigned numChannels)
	: RTPSink(env, rtpGS, rtpPayloadType, rtpTimestampFrequency,
	rtpPayloadFormatName, numChannels)/*调用基类构造*/,
	fOutBuf(NULL), fCurFragmentationOffset(0), fPreviousFrameEndedFragmentation(False),
	fOnSendErrorFunc(NULL), fOnSendErrorData(NULL)
{
	setPacketSizes(1000, 1448);
	// Default max packet size (1500, minus allowance for IP, UDP, UMTP headers)
	// 默认的最大数据包大小（MTU1500，减去IP(20)，UDP，UMTP头余量）
	// (Also, make it a multiple of 4 bytes, just in case that matters.)
	// （另外，使之为4字节的倍数，以防万一的事项。）
}

MultiFramedRTPSink::~MultiFramedRTPSink()
{
	delete fOutBuf;	//释放输出缓冲区
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
	// 缺省的实现：如果这是在分组的第一帧中，使用它的呈现的时间为RTP时间戳：
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
		// 添加填充字节(其填充的最后一个字节是填充的字节数值)
		unsigned char paddingBuffer[255]; //max padding最大填充
		memset(paddingBuffer, 0, numPaddingBytes);
		paddingBuffer[numPaddingBytes - 1] = numPaddingBytes;
		// 入队到输出缓冲
		fOutBuf->enqueue(paddingBuffer, numPaddingBytes);

		// Set the RTP padding bit:设置RTP填充位
		unsigned rtpHdr = fOutBuf->extractWord(0);
		rtpHdr |= 0x20000000;
		fOutBuf->insertWord(rtpHdr, 0);
	}
}

Boolean MultiFramedRTPSink::continuePlaying()
{
	// Send the first packet.发生第一个包
	// (This will also schedule any future sends.)这也将调度后续的任何发生
	buildAndSendPacket(True);	//构建和发送(第一个)包
	return True;
}

void MultiFramedRTPSink::stopPlaying()
{
	// 重置输出缓冲
	fOutBuf->resetPacketStart();
	fOutBuf->resetOffset();
	fOutBuf->resetOverflowData();

	// Then call the default "stopPlaying()" function:
	// 然后调用默认的stopPlaying()函数

	// 1\停止获取帧源，2\停止所有待处理任务，3\设置为可再用
	MediaSink::stopPlaying();
}

void MultiFramedRTPSink::buildAndSendPacket(Boolean isFirstPacket)
{
	fIsFirstPacket = isFirstPacket;

	// Set up the RTP header:建立了RTP报头：
	unsigned rtpHdr = 0x80000000; // RTP version 2; marker ('M') bit not set (by default; it can be set later)RTP版本2;标记（'M'）位未设置（默认情况下，它可以在后面设置）

	rtpHdr |= (fRTPPayloadType << 16);	// 负载类型设置
	rtpHdr |= fSeqNo; // sequence number 队列号
	fOutBuf->enqueueWord(rtpHdr);	//将RTP头部入队到输出缓冲

	// Note where the RTP timestamp will go.
	// 注意所在的RTP时间戳将略过。
	// (We can't fill this in until we start packing payload frames.)
	// （我们不能填充这个，直到我们开始打包有效负载帧）。
	fTimestampPosition = fOutBuf->curPacketSize();	//记录时间戳在包中位置
	fOutBuf->skipBytes(4); // leave a hole for the timestamp留下一个空穴用于时间戳

	fOutBuf->enqueueWord(SSRC());	// 入队同步信源标识，接收者用于区分信源

	// Allow for a special, payload-format-specific header following the
	// 允许一个特殊的，特定负载格式的头跟随
	// RTP header:
	fSpecialHeaderPosition = fOutBuf->curPacketSize();	//记录特定头在包中位置
	fSpecialHeaderSize = specialHeaderSize();	// 记录特定头大小
	fOutBuf->skipBytes(fSpecialHeaderSize);	//跳过，留下空洞

	// Begin packing as many (complete) frames into the packet as we can:
	// 开始打包尽可能多的（完整的）帧到数据包，我们所能：
	fTotalFrameSpecificHeaderSizes = 0;
	fNoFramesLeft = False;
	fNumFramesUsedSoFar = 0;
	packFrame();	// 打包
}

void MultiFramedRTPSink::packFrame()
{
	// Get the next frame.获取下一个帧

	// First, see if we have an overflow frame that was too big for the last pkt首先，我们看看最后数据包是否有过大溢出帧
	if (fOutBuf->haveOverflowData()) {
		// Use this frame before reading a new one from the source使用此帧前从源读取一个新的帧
		unsigned frameSize = fOutBuf->overflowDataSize();
		struct timeval presentationTime = fOutBuf->overflowPresentationTime();
		unsigned durationInMicroseconds = fOutBuf->overflowDurationInMicroseconds();
		fOutBuf->useOverflowData();

		afterGettingFrame1(frameSize, 0, presentationTime, durationInMicroseconds);
	}
	else {
		// Normal case: we need to read a new frame from the source
		// //正常情况下：我们需要读取来自源的新帧
		if (fSource == NULL) return;

		fCurFrameSpecificHeaderPosition = fOutBuf->curPacketSize();
		fCurFrameSpecificHeaderSize = frameSpecificHeaderSize();
		fOutBuf->skipBytes(fCurFrameSpecificHeaderSize);
		fTotalFrameSpecificHeaderSizes += fCurFrameSpecificHeaderSize;
		// 获取下一个帧
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
		// 记录这个事实。我们从现在开始播放
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
	// 如果我们已经打包一个或多个帧到这个数据包，检查这个新的帧是否有资格参与后，它们进行打包。
	// (This is independent of whether the packet has enough room for this
	// new frame; that check comes later.)
	//（这是独立的数据包是否有足够的空间容纳这一新的帧，即检查后话。）
	if (fNumFramesUsedSoFar > 0) {
		// 无法容纳这个帧
		if ((fPreviousFrameEndedFragmentation
			&& !allowOtherFramesAfterLastFragment())
			|| !frameCanAppearAfterPacketStart(fOutBuf->curPtr(), frameSize)) {
			// Save away this frame for next time:
			// 保存离开，这个帧在下一次（表示这个包已经满了，下一包再发这个帧）
			numFrameBytesToUse = 0;
			fOutBuf->setOverflowData(fOutBuf->curPacketSize(), frameSize,
				presentationTime, durationInMicroseconds);
		}
	}
	fPreviousFrameEndedFragmentation = False;	//上一帧结束分片先设置为假

	if (numFrameBytesToUse > 0) {
		// Check whether this frame overflows the packet检查此帧是否溢出数据包
		if (fOutBuf->wouldOverflow(frameSize)) {
			// Don't use this frame now; instead, save it as overflow data, and
			// send it in the next packet instead.  However, if the frame is too
			// big to fit in a packet by itself, then we need to fragment it (and
			// use some of it in this packet, if the payload format permits this.)
			// 现在不可以使用这个帧。而是保存它为溢出数据，并在下一个包中发送它。然而，如果这个帧它自身太大，去适应一个包，那么我们需要去分片它（并且使用它在数据包的一部分，如果这个负载格式允许）
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
			// 这是已分片在多于一个包中的帧的最后一个片段。执行以下任何这种特殊处理情况：
			fCurFragmentationOffset = 0;
			fPreviousFrameEndedFragmentation = True;
		}
	}

	if (numFrameBytesToUse == 0 && frameSize > 0) {
		// Send our packet now, because we have filled it up:
		// 现在我们发送数据包，因为我们已经填补起来：
		sendPacketIfNecessary();
	}
	else {
		// Use this frame in our outgoing packet:使用这个帧在我们的输出数据包：
		unsigned char* frameStart = fOutBuf->curPtr();
		fOutBuf->increment(numFrameBytesToUse);
		// do this now, in case "doSpecialFrameHandling()" calls "setFramePadding()" to append padding bytes
		// 现在这样做，万一“doSpecialFrameHandling（）”调用“setFramePadding（）”来追加填充字节

		// Here's where any payload format specific processing gets done:
		// 在此处，任何负载格式特定处理程序，得到执行：
		doSpecialFrameHandling(curFragmentationOffset, frameStart,
			numFrameBytesToUse, presentationTime,
			overflowBytes);

		++fNumFramesUsedSoFar;

		// Update the time at which the next packet should be sent, based
		// on the duration of the frame that we just packed into it.
		// 在此更新下一个数据包应被发送的时间，基于我们刚刚填充到其中的帧的持续时间。
		// However, if this frame has overflow data remaining, then don't
		// count its duration yet.
		// 然而，如果这个帧有溢出数据残余，那么不要统计它目前持续时间
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
		// 如果现在(1)它已经在这个最佳(包)大小，或(2)(试探)我们刚刚读取其他同样大小帧带这个包将溢出，或(3)它包含一个分片帧的最后一个分片，并且我们不允许任何其它的跟随在这之后，或(4)一个允许的帧(元/比率)包
		if (fOutBuf->isPreferredSize()
			|| fOutBuf->wouldOverflow(numFrameBytesToUse)
			|| (fPreviousFrameEndedFragmentation &&
			!allowOtherFramesAfterLastFragment())
			|| !frameCanAppearAfterPacketStart(fOutBuf->curPtr() - frameSize,
			frameSize)) {
			// The packet is ready to be sent now 该数据包是现在准备要发送
			sendPacketIfNecessary();	//发送包，如果必须
		}
		else {
			// There's room for more frames; try getting another:
			// 还有空间存放更多的帧;尝试获得另一个：
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
	// 没有剩余的帧，但我们可能有部分生成的数据包发送
	sink->fNoFramesLeft = True;
	sink->sendPacketIfNecessary();
}
