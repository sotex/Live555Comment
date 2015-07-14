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
// Implementation

#include "ServerMediaSession.hh"
#include <GroupsockHelper.hh>
#include <math.h>

////////// ServerMediaSession //////////

ServerMediaSession* ServerMediaSession
::createNew(UsageEnvironment& env,
char const* streamName, char const* info,
char const* description, Boolean isSSM, char const* miscSDPLines)
{	// ���ù��캯�����ڶ�(heap)�ϴ���������Ҫ�ֶ��ͷ�
	return new ServerMediaSession(env, streamName, info, description,
		isSSM, miscSDPLines);
}

Boolean ServerMediaSession
::lookupByName(UsageEnvironment& env, char const* mediumName,
ServerMediaSession*& resultSession)
{
	resultSession = NULL; // unless we succeed ������ɹ�

	Medium* medium;
	if (!Medium::lookupByName(env, mediumName, medium)) return False;

	if (!medium->isServerMediaSession()) {
		env.setResultMsg(mediumName, " is not a 'ServerMediaSession' object");
		return False;
	}
	// �ɹ��ҵ�
	resultSession = (ServerMediaSession*)medium;
	return True;
}

// ȱʡ�� SDP��Ϣ�ַ���
static char const* const libNameStr = "LIVE555 Streaming Media v";
// ȱʡ�� �汾��Ϣ�ַ���
char const* const libVersionStr = LIVEMEDIA_LIBRARY_VERSION_STRING;

ServerMediaSession::ServerMediaSession(UsageEnvironment& env,
	char const* streamName,
	char const* info,
	char const* description,
	Boolean isSSM, char const* miscSDPLines)
	: Medium(env), fIsSSM(isSSM), fSubsessionsHead(NULL),
	fSubsessionsTail(NULL), fSubsessionCounter(0),
	fReferenceCount(0), fDeleteWhenUnreferenced(False)
{
	// ����������
	fStreamName = strDup(streamName == NULL ? "" : streamName);
	// ����SDP��Ϣ
	fInfoSDPString = strDup(info == NULL ? libNameStr : info);
	// ����SDP����
	fDescriptionSDPString
		= strDup(description == NULL ? libNameStr : description);
	// ����������SDP��
	fMiscSDPLines = strDup(miscSDPLines == NULL ? "" : miscSDPLines);
	// ���ô���ʱ��Ϊ��ǰʱ��
	gettimeofday(&fCreationTime, NULL);
}

ServerMediaSession::~ServerMediaSession()
{
	// ��medium->envir().liveMedia->mediaTable�в���medium->name()��Ӧ����Ŀ�Ƴ�
	Medium::close(fSubsessionsHead);
	//��remove��ʱ������delete fSubsessionHead���ᵼ��������Ӧ���ͷ���������


	// �ͷ��ڴ�
	delete[] fStreamName;
	delete[] fInfoSDPString;
	delete[] fDescriptionSDPString;
	delete[] fMiscSDPLines;
}

Boolean
ServerMediaSession::addSubsession(ServerMediaSubsession* subsession)
{
	if (subsession->fParentSession != NULL) return False; // it's already used���Ѿ���ʹ��
	// ����һ������β�巨
	if (fSubsessionsTail == NULL) {	// �ӻỰβָ��ΪNULL
		fSubsessionsHead = subsession;	//����Ϊͷָ��
	}
	else {
		fSubsessionsTail->fNext = subsession;	//�ӻỰβ�������һ��
	}
	fSubsessionsTail = subsession;

	subsession->fParentSession = this;
	subsession->fTrackNumber = ++fSubsessionCounter;	//����׷�ٺ�
	return True;

	/*
	1������ʱ	Head-->NULL <--Tail
	2�����һ��Ԫ��Session1��
		Head-->Session1<--Tail
	2�����Ԫ��Session2
		Head-->Session1--fNext-->Session2 <--Tail
	3�����Ԫ��Session3
		Head-->Session1--fNext-->Session2--fNext-->Session3 <--Tail
	*/
}

void ServerMediaSession::testScaleFactor(float& scale)
{
	// First, try setting all subsessions to the desired scale.
	// If the subsessions' actual scales differ from each other, choose the
	// value that's closest to 1, and then try re-setting all subsessions to that
	// value.  If the subsessions' actual scales still differ, re-set them all to 1.
	float minSSScale = 1.0;
	float maxSSScale = 1.0;
	float bestSSScale = 1.0;
	float bestDistanceTo1 = 0.0;
	ServerMediaSubsession* subsession;
	for (subsession = fSubsessionsHead; subsession != NULL;
		subsession = subsession->fNext) {
		float ssscale = scale;
		subsession->testScaleFactor(ssscale);
		if (subsession == fSubsessionsHead) { // this is the first subsession
			minSSScale = maxSSScale = bestSSScale = ssscale;
			bestDistanceTo1 = (float)fabs(ssscale - 1.0f);
		}
		else {
			if (ssscale < minSSScale) {
				minSSScale = ssscale;
			}
			else if (ssscale > maxSSScale) {
				maxSSScale = ssscale;
			}

			float distanceTo1 = (float)fabs(ssscale - 1.0f);
			if (distanceTo1 < bestDistanceTo1) {
				bestSSScale = ssscale;
				bestDistanceTo1 = distanceTo1;
			}
		}
	}
	if (minSSScale == maxSSScale) {
		// All subsessions are at the same scale: minSSScale == bestSSScale == maxSSScale
		scale = minSSScale;
		return;
	}

	// The scales for each subsession differ.  Try to set each one to the value
	// that's closest to 1:
	for (subsession = fSubsessionsHead; subsession != NULL;
		subsession = subsession->fNext) {
		float ssscale = bestSSScale;
		subsession->testScaleFactor(ssscale);
		if (ssscale != bestSSScale) break; // no luck
	}
	if (subsession == NULL) {
		// All subsessions are at the same scale: bestSSScale
		scale = bestSSScale;
		return;
	}

	// Still no luck.  Set each subsession's scale to 1:
	for (subsession = fSubsessionsHead; subsession != NULL;
		subsession = subsession->fNext) {
		float ssscale = 1;
		subsession->testScaleFactor(ssscale);
	}
	scale = 1;
}

float ServerMediaSession::duration() const
{
	float minSubsessionDuration = 0.0;
	float maxSubsessionDuration = 0.0;
	for (ServerMediaSubsession* subsession = fSubsessionsHead; subsession != NULL;
		subsession = subsession->fNext) {
		float ssduration = subsession->duration();
		if (subsession == fSubsessionsHead) { // this is the first subsession
			minSubsessionDuration = maxSubsessionDuration = ssduration;
		}
		else if (ssduration < minSubsessionDuration) {
			minSubsessionDuration = ssduration;
		}
		else if (ssduration > maxSubsessionDuration) {
			maxSubsessionDuration = ssduration;
		}
	}

	if (maxSubsessionDuration != minSubsessionDuration) {
		return -maxSubsessionDuration; // because subsession durations differ
	}
	else {
		return maxSubsessionDuration; // all subsession durations are the same
	}
}

Boolean ServerMediaSession::isServerMediaSession() const
{
	return True;
}

char* ServerMediaSession::generateSDPDescription()
{
	AddressString ipAddressStr(ourIPAddress(envir()));	//��ȡ����IP��ַ

	unsigned ipAddressStrSize = strlen(ipAddressStr.val());

	// For a SSM sessions, we need a "a=source-filter: incl ..." line also:
	// ����SSM�Ự��������Ҫһ��"a=source-filter: incl ..." ������
	char* sourceFilterLine;
	if (fIsSSM) {
		char const* const sourceFilterFmt =
			"a=source-filter: incl IN IP4 * %s\r\n"
			"a=rtcp-unicast: reflection\r\n";
		unsigned const sourceFilterFmtSize = strlen(sourceFilterFmt) + ipAddressStrSize + 1;

		sourceFilterLine = new char[sourceFilterFmtSize];
		sprintf(sourceFilterLine, sourceFilterFmt, ipAddressStr.val());
		// ���磺"a=source-filter: incl IN IP4 * 192.168.1.1\r\na=rtcp-unicast: reflection\r\n"
	}
	else {
		sourceFilterLine = strDup("");
	}

	char* rangeLine = NULL; // for now
	char* sdp = NULL; // for now

	do {
		// Count the lengths of each subsession's media-level SDP lines.
		// ����ÿ���ӻỰ��ý�弶SDP�еĳ��ȡ�
		// (We do this first, because the call to "subsession->sdpLines()"
		// causes correct subsession 'duration()'s to be calculated later.)
		unsigned sdpLength = 0;
		ServerMediaSubsession* subsession;
		for (subsession = fSubsessionsHead; subsession != NULL;
			subsession = subsession->fNext) {
			char const* sdpLines = subsession->sdpLines();
			if (sdpLines == NULL) break; // the media's not available
			sdpLength += strlen(sdpLines);
		}
		if (subsession != NULL) break; // an error occurred

		// Unless subsessions have differing durations, we also have a "a=range:" line:
		float dur = duration();
		if (dur == 0.0) {
			rangeLine = strDup("a=range:npt=0-\r\n");
		}
		else if (dur > 0.0) {
			char buf[100];
			sprintf(buf, "a=range:npt=0-%.3f\r\n", dur);
			rangeLine = strDup(buf);
		}
		else { // subsessions have differing durations, so "a=range:" lines go there
			rangeLine = strDup("");
		}
		// SDPǰ׺��ʽ
		char const* const sdpPrefixFmt =
			"v=0\r\n"
			"o=- %ld%06ld %d IN IP4 %s\r\n"
			"s=%s\r\n"
			"i=%s\r\n"
			"t=0 0\r\n"
			"a=tool:%s%s\r\n"
			"a=type:broadcast\r\n"
			"a=control:*\r\n"
			"%s"
			"%s"
			"a=x-qt-text-nam:%s\r\n"
			"a=x-qt-text-inf:%s\r\n"
			"%s";
		sdpLength += strlen(sdpPrefixFmt)
			+ 20 + 6 + 20 + ipAddressStrSize
			+ strlen(fDescriptionSDPString)
			+ strlen(fInfoSDPString)
			+ strlen(libNameStr) + strlen(libVersionStr)
			+ strlen(sourceFilterLine)
			+ strlen(rangeLine)
			+ strlen(fDescriptionSDPString)
			+ strlen(fInfoSDPString)
			+ strlen(fMiscSDPLines);
		sdp = new char[sdpLength];
		if (sdp == NULL) break;

		// Generate the SDP prefix (session-level lines):
		sprintf(sdp, sdpPrefixFmt,
			fCreationTime.tv_sec, fCreationTime.tv_usec, // o= <session id>
			1, // o= <version> // (needs to change if params are modified)
			ipAddressStr.val(), // o= <address>
			fDescriptionSDPString, // s= <description>
			fInfoSDPString, // i= <info>
			libNameStr, libVersionStr, // a=tool:
			sourceFilterLine, // a=source-filter: incl (if a SSM session)
			rangeLine, // a=range: line
			fDescriptionSDPString, // a=x-qt-text-nam: line
			fInfoSDPString, // a=x-qt-text-inf: line
			fMiscSDPLines); // miscellaneous session SDP lines (if any)

		// Then, add the (media-level) lines for each subsession:
		char* mediaSDP = sdp;
		for (subsession = fSubsessionsHead; subsession != NULL;
			subsession = subsession->fNext) {
			mediaSDP += strlen(mediaSDP);
			sprintf(mediaSDP, "%s", subsession->sdpLines());
		}
	} while (0);

	delete[] rangeLine; delete[] sourceFilterLine;
	return sdp;
}


////////// ServerMediaSessionIterator //////////

ServerMediaSubsessionIterator
::ServerMediaSubsessionIterator(ServerMediaSession& session)
: fOurSession(session)
{
	reset();
}

ServerMediaSubsessionIterator::~ServerMediaSubsessionIterator()
{
}

ServerMediaSubsession* ServerMediaSubsessionIterator::next()
{
	ServerMediaSubsession* result = fNextPtr;

	if (fNextPtr != NULL) fNextPtr = fNextPtr->fNext;

	return result;
}

void ServerMediaSubsessionIterator::reset()
{
	fNextPtr = fOurSession.fSubsessionsHead;
}


////////// ServerMediaSubsession //////////

ServerMediaSubsession::ServerMediaSubsession(UsageEnvironment& env)
: Medium(env),
fParentSession(NULL), fServerAddressForSDP(0), fPortNumForSDP(0),
fNext(NULL), fTrackNumber(0), fTrackId(NULL)
{
}

ServerMediaSubsession::~ServerMediaSubsession()
{
	delete[](char*)fTrackId;
	Medium::close(fNext);	// �ر���һ������(��������ͷ�)
}

char const* ServerMediaSubsession::trackId()
{
	// ��δ����׷�ٺ�(��ServerMediaSession::addSubsession)
	if (fTrackNumber == 0) return NULL; // not yet in a ServerMediaSession��δ��ServerMediaSession

	if (fTrackId == NULL) {
		char buf[100];
		sprintf(buf, "track%d", fTrackNumber);
		fTrackId = strDup(buf);
	}
	return fTrackId;
}

void ServerMediaSubsession::pauseStream(unsigned /*clientSessionId*/,
	void* /*streamToken*/)
{
	// default implementation: do nothing
}
void ServerMediaSubsession::seekStream(unsigned /*clientSessionId*/,
	void* /*streamToken*/, double& /*seekNPT*/, double /*streamDuration*/, u_int64_t& numBytes)
{
	// default implementation: do nothing
	numBytes = 0;
}
FramedSource* ServerMediaSubsession::getStreamSource(void* /*streamToken*/)
{
	// default implementation: return NULL
	return NULL;
}
void ServerMediaSubsession::setStreamScale(unsigned /*clientSessionId*/,
	void* /*streamToken*/, float /*scale*/)
{
	// default implementation: do nothing
}
void ServerMediaSubsession::deleteStream(unsigned /*clientSessionId*/,
	void*& /*streamToken*/)
{
	// default implementation: do nothing
}

void ServerMediaSubsession::testScaleFactor(float& scale)
{
	// default implementation: Support scale = 1 only
	scale = 1;
}

float ServerMediaSubsession::duration() const
{
	// default implementation: assume an unbounded session:
	return 0.0;
}

void ServerMediaSubsession::setServerAddressAndPortForSDP(netAddressBits addressBits,
	portNumBits portBits)
{
	fServerAddressForSDP = addressBits;	// ����IP
	fPortNumForSDP = portBits;	// ���ö˿�
}

char const*
ServerMediaSubsession::rangeSDPLine() const
{
	// û�б����뵽�����ʱ��
	if (fParentSession == NULL) return NULL;

	// If all of our parent's subsessions have the same duration
	// ������Ǹ�ĸ�����е��ӻỰ��ʱ����ͬ
	// (as indicated by "fParentSession->duration() >= 0"), there's no "a=range:" line:
	if (fParentSession->duration() >= 0.0) return strDup("");

	// Use our own duration for a "a=range:" line:
	//  "a=range:" line:ʹ�������Լ��ĳ���ʱ��
	float ourDuration = duration();
	if (ourDuration == 0.0) {
		return strDup("a=range:npt=0-\r\n");
	}
	else {
		char buf[100];
		sprintf(buf, "a=range:npt=0-%.3f\r\n", ourDuration);
		return strDup(buf);
	}
}
