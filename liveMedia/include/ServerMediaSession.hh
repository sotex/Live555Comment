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
// A data structure that represents a session that consists of
// potentially multiple (audio and/or video) sub-sessions
// (This data structure is used for media *streamers* - i.e., servers.
//  For media receivers, use "MediaSession" instead.)
// C++ header

#ifndef _SERVER_MEDIA_SESSION_HH
#define _SERVER_MEDIA_SESSION_HH

#ifndef _MEDIA_HH
#include "Media.hh"
#endif
#ifndef _FRAMED_SOURCE_HH
#include "FramedSource.hh"
#endif
#ifndef _GROUPEID_HH
#include "GroupEId.hh"
#endif
#ifndef _RTP_INTERFACE_HH
#include "RTPInterface.hh" // for ServerRequestAlternativeByteHandler
#endif

class ServerMediaSubsession; // forward

// ������ý��Ự(�����ڵ�����ServerMediaSubsession*)
class ServerMediaSession : public Medium {
public:
	static ServerMediaSession* createNew(UsageEnvironment& env,
		char const* streamName = NULL,
		char const* info = NULL,
		char const* description = NULL,
		Boolean isSSM = False,
		char const* miscSDPLines = NULL);

	virtual ~ServerMediaSession();

	static Boolean lookupByName(UsageEnvironment& env,
		char const* mediumName,
		ServerMediaSession*& resultSession);

	char* generateSDPDescription(); // based on the entire session���������Ự
	// Note: The caller is responsible for freeing the returned string�����߸����ͷŷ��ص��ַ���

	char const* streamName() const { return fStreamName; }

	// ����ӻỰ
	Boolean addSubsession(ServerMediaSubsession* subsession);
	// ���Ա�������
	void testScaleFactor(float& scale); // sets "scale" to the actual supported scale����"scale"��ʵ�ʵ�scale
	// ����ʱ��
	float duration() const;
	// a result == 0 means an unbounded session (the default)
	// a result < 0 means: subsession durations differ; the result is -(the largest)
	// a result > 0 means: this is the duration of a bounded session

	// ��ȡ���ü���
	unsigned referenceCount() const { return fReferenceCount; }
	// �������ü���
	void incrementReferenceCount() { ++fReferenceCount; }
	// �Լ����ü���
	void decrementReferenceCount() { if (fReferenceCount > 0) --fReferenceCount; }
	Boolean& deleteWhenUnreferenced() { return fDeleteWhenUnreferenced; }

protected:
	ServerMediaSession(UsageEnvironment& env, char const* streamName,
		char const* info, char const* description,
		Boolean isSSM, char const* miscSDPLines);
	// called only by "createNew()" ����createNew()����

private: // redefined virtual functions
	virtual Boolean isServerMediaSession() const;

private:
	Boolean fIsSSM;	//���ض�Դ�ಥ��

	// Linkage fields:�����ֶ�
	friend class ServerMediaSubsessionIterator;	//�ӻỰ���������
	ServerMediaSubsession* fSubsessionsHead;	//�ӻỰͷָ��
	ServerMediaSubsession* fSubsessionsTail;	//�ӻỰβָ��
	unsigned fSubsessionCounter;	//�ӻỰ����

	char* fStreamName;	//������
	char* fInfoSDPString;	//SDP��Ϣ�ַ���(SDP:�Ự����Э��Session Description Protocol)
	char* fDescriptionSDPString;//SDP�����ַ���
	char* fMiscSDPLines;	//����SDP��·
	struct timeval fCreationTime;	//����ʱ��
	unsigned fReferenceCount;	//���ü���
	Boolean fDeleteWhenUnreferenced;	//ɾ��ʱ������
};

//======================================================================

class ServerMediaSubsessionIterator {
public:
	ServerMediaSubsessionIterator(ServerMediaSession& session);
	virtual ~ServerMediaSubsessionIterator();

	ServerMediaSubsession* next(); // NULL if none
	void reset();

private:
	ServerMediaSession& fOurSession;	// ������
	ServerMediaSubsession* fNextPtr;	// ָ��ڵ�
};

//========================================================================


class ServerMediaSubsession : public Medium {
public:
	virtual ~ServerMediaSubsession();

	unsigned trackNumber() const { return fTrackNumber; }
	// ����׷��ID
	char const* trackId();
	virtual char const* sdpLines() = 0;
	// ��ȡ������
	virtual void getStreamParameters(unsigned clientSessionId, // in
		netAddressBits clientAddress, // in
		Port const& clientRTPPort, // in
		Port const& clientRTCPPort, // in
		int tcpSocketNum, // in (-1 means use UDP, not TCP)
		unsigned char rtpChannelId, // in (used if TCP)
		unsigned char rtcpChannelId, // in (used if TCP)
		netAddressBits& destinationAddress, // in out
		u_int8_t& destinationTTL, // in out
		Boolean& isMulticast, // out
		Port& serverRTPPort, // out
		Port& serverRTCPPort, // out
		void*& streamToken // out
		) = 0;

	// ��ʼ��
	virtual void startStream(unsigned clientSessionId, void* streamToken,
		TaskFunc* rtcpRRHandler,
		void* rtcpRRHandlerClientData,
		unsigned short& rtpSeqNum,
		unsigned& rtpTimestamp,
		ServerRequestAlternativeByteHandler* serverRequestAlternativeByteHandler,
		void* serverRequestAlternativeByteHandlerClientData) = 0;

	// ��ͣ��
	virtual void pauseStream(unsigned clientSessionId, void* streamToken);
	// ��Ѱ��λ��
	virtual void seekStream(unsigned clientSessionId, void* streamToken, double& seekNPT, double streamDuration, u_int64_t& numBytes);
	// "streamDuration", if >0.0, specifies how much data to stream, past "seekNPT".  (If <=0.0, all remaining data is streamed.)
	// ������ʱ��(streamDuration)�������0��ָ������"seekNPT"���ݵ��������С��0������ʣ����������
	// "numBytes" returns the size (in bytes) of the data to be streamed, or 0 if unknown or unlimited.

	// ���������ű���
	virtual void setStreamScale(unsigned clientSessionId, void* streamToken, float scale);
	// ��ȡ��Դ
	virtual FramedSource* getStreamSource(void* streamToken);
	// ɾ����
	virtual void deleteStream(unsigned clientSessionId, void*& streamToken);
	// ������������
	virtual void testScaleFactor(float& scale); // sets "scale" to the actual supported scale
	// ����ʱ��
	virtual float duration() const;
	// returns 0 for an unbounded session (the default)
	// returns > 0 for a bounded session

	// The following may be called by (e.g.) SIP servers, for which the
	// address and port number fields in SDP descriptions need to be non-zero:
	// ����SDP(�Ự����Э��)��������ַ�Ͷ˿�
	void setServerAddressAndPortForSDP(netAddressBits addressBits,
		portNumBits portBits);

protected: // we're a virtual base class
	ServerMediaSubsession(UsageEnvironment& env);
	// ��ȡSDP��ΧLine�ַ���("a=range:npt=0-%.3f\r\n",Duration)
	char const* rangeSDPLine() const;
	// returns a string to be delete[]d

	ServerMediaSession* fParentSession;		// ���Ự(����this)
	netAddressBits fServerAddressForSDP;	// SDP(�Ự����Э��)����ip��ַ
	portNumBits fPortNumForSDP;				// SDP�˿�

private:
	friend class ServerMediaSession;
	friend class ServerMediaSubsessionIterator;
	ServerMediaSubsession* fNext;	// ��һ���ӻỰ

	unsigned fTrackNumber;	// ׷�ٺ� within an enclosing ServerMediaSession�������
	char const* fTrackId;	// ׷��ID
};

#endif
