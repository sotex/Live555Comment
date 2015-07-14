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
// RTP Sinks
// C++ header

#ifndef _RTP_SINK_HH
#define _RTP_SINK_HH

#ifndef _MEDIA_SINK_HH
#include "MediaSink.hh"
#endif
#ifndef _RTP_INTERFACE_HH
#include "RTPInterface.hh"
#endif

class RTPTransmissionStatsDB; // forward

//	RTPװ�ز�
class RTPSink : public MediaSink {
public:
	// ��env.liveMedia->mediaTable�в���sinkName��Ӧ��RTPSink�����ַ
	static Boolean lookupByName(UsageEnvironment& env, char const* sinkName,
		RTPSink*& resultSink);

	// used by RTCP: RTCPʹ��
	u_int32_t SSRC() const { return fSSRC; }
	// later need a means of changing the SSRC if there's a collision #####
	// ���Ժ���Ҫ����SSRC��һ���ֶΣ�����з�����ײ#####

	// ת��tv��RTPʱ���
	u_int32_t convertToRTPTimestamp(struct timeval tv);
	// ��ȡ������
	unsigned packetCount() const { return fPacketCount; }
	// ��ȡ8bit����
	unsigned octetCount() const { return fOctetCount; }

	// used by RTSP servers:����RTSP������
	// ���׽��ֿ�ʼʹ��
	Groupsock const& groupsockBeingUsed() const/*const����(��const����ʹ��)*/ 
	{ return *(fRTPInterface.gs()); }
	// ���׽��ֿ�ʼʹ��
	Groupsock& groupsockBeingUsed() { return *(fRTPInterface.gs()); }
	//RTP��������
	unsigned char rtpPayloadType() const { return fRTPPayloadType; }
	// RTPʱ���Ƶ��
	unsigned rtpTimestampFrequency() const { return fTimestampFrequency; }
	// ����RTPʱ���Ƶ��
	void setRTPTimestampFrequency(unsigned freq)
	{
		fTimestampFrequency = freq;
	}
	// RTP���ظ�ʽ��
	char const* rtpPayloadFormatName() const { return fRTPPayloadFormatName; }
	// ͨ����
	unsigned numChannels() const { return fNumChannels; }

	virtual char const* sdpMediaType() const; // for use in SDP m= lines

	// ��ȡRTPӳ���ʽ�У���Ҫ����delete����
	virtual char* rtpmapLine() const;
	// returns a string to be delete[]d

	// ����SDP��
	virtual char const* auxSDPLine();
	// ��ѡ��optional SDP line (e.g. a=fmtp:...)

	// ��ǰ�Ķ��б��
	u_int16_t currentSeqNo() const { return fSeqNo; }

	u_int32_t presetNextTimestamp();
	// ensures that the next timestamp to be used will correspond to
	// the current 'wall clock' time.
	// ȷ��Ҫʹ�õ���һ��ʱ�������Ӧ�ڵ�ǰ�ġ�ǽ�ϵ��ӡ���ʱ��(ʱ��)��

	RTPTransmissionStatsDB& transmissionStatsDB() const
	{
		return *fTransmissionStatsDB;
	}

	Boolean nextTimestampHasBeenPreset() const { return fNextTimestampHasBeenPreset; }
	// ������Socket
	void setStreamSocket(int sockNum, unsigned char streamChannelId)
	{
		fRTPInterface.setStreamSocket(sockNum, streamChannelId);
	}
	// ���һ����Socket
	void addStreamSocket(int sockNum, unsigned char streamChannelId)
	{
		fRTPInterface.addStreamSocket(sockNum, streamChannelId);
	}
	void removeStreamSocket(int sockNum, unsigned char streamChannelId)
	{
		fRTPInterface.removeStreamSocket(sockNum, streamChannelId);
	}
	// ���÷���������ѡ�ֽڴ������
	void setServerRequestAlternativeByteHandler(int socketNum, ServerRequestAlternativeByteHandler* handler, void* clientData)
	{
		fRTPInterface.setServerRequestAlternativeByteHandler(socketNum, handler, clientData);
	}
	// hacks to allow sending RTP over TCP (RFC 2236, section 10.12)
	// �����͵�RTP����TCP��RFC2236��10.12��

	// ��ȡ�ܵı�����	Elapsed��ȥ���Ѿ����Ѳ���
	void getTotalBitrate(unsigned& outNumBytes, double& outElapsedTime);
	// returns the number of bytes sent since the last time that we
	// were called, and resets the counter.
	// �����Դ��ϴ����Ǳ����÷��͵��ֽ���������λ��������

protected:
	RTPSink(UsageEnvironment& env,
		Groupsock* rtpGS, unsigned char rtpPayloadType,
		u_int32_t rtpTimestampFrequency,
		char const* rtpPayloadFormatName,
		unsigned numChannels);
	// abstract base class

	virtual ~RTPSink();

	RTPInterface fRTPInterface;		//RTP�ӿ�
	unsigned char fRTPPayloadType;	//RTP��������
	unsigned fPacketCount/*������*/, fOctetCount/*8bit����*/, 
		fTotalOctetCount /*����RTPͷ��incl RTP hdr*/;
	struct timeval fTotalOctetCountStartTime;	//�ܵ���ʼʱ��(Octet 8bit)
	u_int32_t fCurrentTimestamp;	//��ǰʱ���
	u_int16_t fSeqNo;	//���к�(Sequeue Numero)

private:
	// redefined virtual functions:
	virtual Boolean isRTPSink() const;

private:
	/*ͬ����Դ��ָ����ý��������Դ����ͨ��RTP��ͷ�е�һ��32λ����SSRC��ʶ������ʶ,
	���������������ַ�������߽�����SSRC��ʶ�������ֲ�ͬ����Դ������RTP���ĵķ��顣*/
	u_int32_t fSSRC;/* ͬ����Դ(SSRC)��ʶ����*/
	u_int32_t fTimestampBase;	/*ʱ�������*/
	unsigned fTimestampFrequency;	/*ʱ���Ƶ��*/
	Boolean fNextTimestampHasBeenPreset;	//��һ��ʱ����Ѿ�Ԥ�裿
	char const* fRTPPayloadFormatName;	//RTP���ظ�ʽ����
	unsigned fNumChannels;	//ͨ����
	struct timeval fCreationTime;	// ����ʱ��

	RTPTransmissionStatsDB* fTransmissionStatsDB;	// ����״̬���ݿ�
};


class RTPTransmissionStats; // forward

//=====================================================================


//	RTP����״̬���ݿ�
class RTPTransmissionStatsDB {
public:
	unsigned numReceivers() const { return fNumReceivers; }

	class Iterator {
	public:
		Iterator(RTPTransmissionStatsDB& receptionStatsDB);
		virtual ~Iterator();

		RTPTransmissionStats* next();
		// NULL if none

	private:
		HashTable::Iterator* fIter;
	};

	/*
	RTCP�����ַ�������
	����  ��д��ʾ                            ����
	200  SR��Sender Report��                ���Ͷ˱���
	201  RR��Receiver Roport��              ���ն˱���
	202  SDES��Source Descripition Items��  Դ��
	203  BYE                               ����
	204  APP��Application��                 �ض�Ӧ��

	��������  BYE  ��ʾ�ر�һ����������
	�ض�Ӧ�÷���  APP  ʹӦ�ó����ܹ������µķ������͡���
	���ն˱������  RR  ����ʹ���ն������Ե������еĵ��öಥ��ʽ���б��档���ն�ÿ�յ�һ��RTP����һ�λỰ�������RTP�����Ͳ���һ�����ն˱������RR��RR����������У����յ���RTP����SSRC����RTP���ķ��鶪ʧ�ʣ������鶪ʧ��̫�ߣ����Ͷ˾�Ӧ���ʵ��ؽ��ͷ��ͷ�������ʣ����ڸ�RTP���е����һ��RTP�������ţ����鵽��ʱ�����Ķ����ȣ�
	����RR����������Ŀ�ġ���һ������ʹ���еĽ��ն˺ͷ��Ͷ��˽⵱ǰ�����״̬���ڶ�������ʹ���з���RTCP�����վ������Ӧ�ص����Լ�����RTCP��������ʣ�ʹ����������õ�RTCP���鲻Ҫ�����Ӱ�촫��Ӧ�����ݵ�RTP�����������еĴ��䡣ͨ����ʹRTCP�����ͨ���������������й������ݷ������������5%�������ն˵�ͨ������ӦС������RTCP�����ͨ������75%��
	���Ͷ˱������  SR  ����ʹ���Ͷ������Ե������н��ն��öಥ��ʽ���б��档���Ͷ�ÿ����һ��RTP����Ҫ����һ�����Ͷ˱������SR��SR����������У���RTP����SSRC����RTP�������²�����RTP�����ʱ����;���ʱ��ʱ�䣨��ǽ��ʱ��ʱ��wall clock time������RTP�������ķ���������RTP���������ֽ�����
	����ʱ��ʱ���Ǳ�Ҫ�ġ���ΪRTPҪ��ÿһ��ý��ʹ��һ���������磬Ҫ������Ƶͼ�����Ӧ����������Ҫ���������������˾���ʱ��ʱ��Ϳ��Խ���ͼ���������ͬ����
	Դ����������  SDES  �����Ự�вμ��ߵ��������������μ��ߵĹ淶��CNAME��Canonical NAME�����淶���ǲμ��ߵĵ����ʼ���ַ���ַ�����
*/

	// The following is called whenever a RTCP RR packet is received:
	// ����ÿ�����յ���RTCP RR(Receiver Roport���ն˱���)����ʱ���ã�
	// ע�⵽����RR(�ӹ�ϣ���в���SSRC��Ӧ�Ķ���������)
	void noteIncomingRR(u_int32_t SSRC, struct sockaddr_in const& lastFromAddress,
		unsigned lossStats, unsigned lastPacketNumReceived,
		unsigned jitter/*����*/, unsigned lastSRTime, unsigned diffSR_RRTime);


	// The following is called when a RTCP BYE packet is received:
	// �����յ�RTCP BYE��ʱ���ã�
	// ɾ����¼
	void removeRecord(u_int32_t SSRC);

	// ��fTable�в���SSRC������RTPTransmissionStats�����ַ
	RTPTransmissionStats* lookup(u_int32_t SSRC) const;

private: // constructor and destructor, called only by RTPSink:
	// ���������������RTPSink����
	friend class RTPSink;
	RTPTransmissionStatsDB(RTPSink& rtpSink);
	virtual ~RTPTransmissionStatsDB();

private:
	// ��Ӽ�¼(key:SSRC+value:stats)
	void add(u_int32_t SSRC, RTPTransmissionStats* stats);

private:
	friend class Iterator;
	unsigned	fNumReceivers;	//������
	RTPSink&	fOurRTPSink;	//RTP��
	HashTable*	fTable;			//���ݿ��
};

//=====================================================================


// RTP����״̬
class RTPTransmissionStats {
public:
	u_int32_t SSRC() const { return fSSRC; }
	struct sockaddr_in const& lastFromAddress() const { return fLastFromAddress; }
	unsigned lastPacketNumReceived() const { return fLastPacketNumReceived; }
	unsigned firstPacketNumReported() const { return fFirstPacketNumReported; }
	unsigned totNumPacketsLost() const { return fTotNumPacketsLost; }
	unsigned jitter() const { return fJitter; }
	unsigned lastSRTime() const { return fLastSRTime; }
	unsigned diffSR_RRTime() const { return fDiffSR_RRTime; }

	unsigned roundTripDelay() const;

	/*
	RTT(Round-Trip Time): ����ʱ�ӡ��ڼ��������������һ����Ҫ������ָ�꣬��ʾ�ӷ��Ͷ˷������ݿ�ʼ�������Ͷ��յ����Խ��ն˵�ȷ�ϣ����ն��յ����ݺ����������ȷ�ϣ����ܹ�������ʱ�ӡ�
	������ʱ(RTT)���������־���������·�Ĵ���ʱ�䡢ĩ��ϵͳ�Ĵ���ʱ���Լ�·�����Ļ����е��ŶӺʹ���ʱ�䡣���У�ǰ���������ֵ�ֵ��Ϊһ��TCP������Թ̶���·�����Ļ����е��ŶӺʹ���ʱ���������������ӵ���̶ȵı仯���仯������RTT�ı仯��һ���̶��Ϸ�ӳ������ӵ���̶ȵı仯������˵���Ƿ��ͷ��ӷ������ݿ�ʼ�����յ����Խ��ܷ���ȷ����Ϣ��������ʱ�䡣
	*/

	// The round-trip delay (in units of 1/65536 seconds) computed from
	// the most recently-received RTCP RR packet.
	// �������ӳ٣���1/65536��Ϊ��λ���������½��յ���RTCP RR���ݰ�����ġ�
	struct timeval timeCreated() const { return fTimeCreated; }
	struct timeval lastTimeReceived() const { return fTimeReceived; }
	void getTotalOctetCount(u_int32_t& hi, u_int32_t& lo);
	void getTotalPacketCount(u_int32_t& hi, u_int32_t& lo);

	// Information which requires at least two RRs to have been received:
	// ����ϢҪ������������RR�Ѿ��յ���
	Boolean oldValid() const { return fOldValid; } // Have two RRs been received?��������RR���յ���

	// ���ϴ�RR���յ����ݰ�
	unsigned packetsReceivedSinceLastRR() const;
	u_int8_t packetLossRatio() const { return fPacketLossRatio; }
	// as an 8-bit fixed-point number��Ϊһ��8λ������
	int packetsLostBetweenRR() const;

private:
	// called only by RTPTransmissionStatsDB:
	// ����RTPTransmissionStatsDB����
	friend class RTPTransmissionStatsDB;
	RTPTransmissionStats(RTPSink& rtpSink, u_int32_t SSRC);
	virtual ~RTPTransmissionStats();

	void noteIncomingRR(struct sockaddr_in const& lastFromAddress,
		unsigned lossStats, unsigned lastPacketNumReceived,
		unsigned jitter,
		unsigned lastSRTime, unsigned diffSR_RRTime);

private:
	RTPSink& fOurRTPSink;	//RTP��
	u_int32_t fSSRC;		//ͬ����Դ��ʶ
	struct sockaddr_in fLastFromAddress;	//�ϴ���Դ��ַ
	unsigned fLastPacketNumReceived;	//�ϴΰ�������
	u_int8_t fPacketLossRatio;	//������
	unsigned fTotNumPacketsLost;	// �ܶ�����
	unsigned fJitter;	//����
	unsigned fLastSRTime;	//�ϴ� SR ʱ��
	unsigned fDiffSR_RRTime;	//SR��RR����ʱ��
	struct timeval fTimeCreated, fTimeReceived;	//����ʱ��/����ʱ��
	Boolean fOldValid;	//�ɵ���Ч?
	unsigned fOldLastPacketNumReceived;	//���ϴεĽ��հ���
	unsigned fOldTotNumPacketsLost;	//�ɵĶ�������
	Boolean fFirstPacket;	//��һ������
	unsigned fFirstPacketNumReported;	//
	u_int32_t fLastOctetCount/*�ϴΰ�λ����*/, fTotalOctetCount_hi, fTotalOctetCount_lo;
	u_int32_t fLastPacketCount, fTotalPacketCount_hi, fTotalPacketCount_lo;
};

#endif
