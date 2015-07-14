// ����ֱ��ϵͳ.cpp : �������̨Ӧ�ó������ڵ㡣
// ������
// �й���ý��ѧ/���ֵ��Ӽ���
// leixiaohua1020@126.com
//ע���������к�ʹ�ò�����������VLC Media Player��FFplay�ȣ�
//��URL��rtp ://239.255.42.42:1234�������տ�ֱ������Ƶ��

#if 0
#include "liveMedia.hh"
#include "BasicUsageEnvironment.hh"
#include "GroupsockHelper.hh"

//��Ҫ���뾲̬��
#pragma comment(lib,"ws2_32.lib")	//gethostname
#pragma comment(lib,"BasicUsageEnvironment.lib")
#pragma comment(lib,"groupsock.lib")
#pragma comment(lib,"liveMedia.lib")
#pragma comment(lib,"UsageEnvironment.lib")
#pragma comment(lib,"WindowsAudioInputDevice.lib")


//#define IMPLEMENT_RTSP_SERVER
//#define USE_SSM 1
#ifdef USE_SSM
Boolean const isSSM = True;
#else
Boolean const isSSM = False;
#endif



#define TRANSPORT_PACKET_SIZE 188
#define TRANSPORT_PACKETS_PER_NETWORK_PACKET 7


UsageEnvironment* env;
char const* inputFileName = NULL;
FramedSource* videoSource;
RTPSink* videoSink;

void play(); // forward

int main(int argc, char** argv) {

	if (argc < 2){
		printf("Usage: %s mediaFile\n", argv[0]);
		exit(0);
	}
	inputFileName = argv[1];

	// ���Ƚ���ʹ�û�����
	TaskScheduler* scheduler = BasicTaskScheduler::createNew();
	env = BasicUsageEnvironment::createNew(*scheduler);

	// ���� 'groupsocks' for RTP and RTCP:
	char const* destinationAddressStr
#ifdef USE_SSM
		= "232.255.42.42";
#else
		= "232.255.42.42";
	// Note: ����һ���ಥ��ַ�������ϣ����ʹ�õ�����ַ,Ȼ���滻����ַ����뵥����ַ  
#endif
	const unsigned short rtpPortNum = 1234;
	const unsigned short rtcpPortNum = rtpPortNum + 1;
	const unsigned char ttl = 7; //


	struct in_addr destinationAddress;
	destinationAddress.s_addr = our_inet_addr(destinationAddressStr);
	const Port rtpPort(rtpPortNum);
	const Port rtcpPort(rtcpPortNum);

	Groupsock rtpGroupsock(*env, destinationAddress, rtpPort, ttl);
	Groupsock rtcpGroupsock(*env, destinationAddress, rtcpPort, ttl);
#ifdef USE_SSM
	rtpGroupsock.multicastSendOnly();
	rtcpGroupsock.multicastSendOnly();
#endif

	// ����һ���ʵ��ġ�RTPSink��:

	videoSink =
		SimpleRTPSink::createNew(*env, &rtpGroupsock, 33, 90000, "video", "mp2t",
		1, True, False /*no 'M' bit*/);


	const unsigned estimatedSessionBandwidth = 5000; // in kbps; for RTCP b/w share
	const unsigned maxCNAMElen = 100;
	unsigned char CNAME[maxCNAMElen + 1];
	gethostname((char*)CNAME, maxCNAMElen);
	CNAME[maxCNAMElen] = '\0';
#ifdef IMPLEMENT_RTSP_SERVER
	RTCPInstance* rtcp =
#endif
		RTCPInstance::createNew(*env, &rtcpGroupsock,
		estimatedSessionBandwidth, CNAME,
		videoSink, NULL /* we're a server */, isSSM);
	// ��ʼ�Զ����е�ý��

#ifdef IMPLEMENT_RTSP_SERVER
	RTSPServer* rtspServer = RTSPServer::createNew(*env);

	if (rtspServer == NULL) {
		*env << "Failed to create RTSP server: " << env->getResultMsg() << "\n";
		exit(1);
	}
	ServerMediaSession* sms
		= ServerMediaSession::createNew(*env, "testStream", inputFileName,
		"Session streamed by \"testMPEG2TransportStreamer\"",
		isSSM);
	sms->addSubsession(PassiveServerMediaSubsession::createNew(*videoSink, rtcp));
	rtspServer->addServerMediaSession(sms);

	char* url = rtspServer->rtspURL(sms);
	*env << "Play this stream using the URL \"" << url << "\"\n";
	delete[] url;
#endif


	*env << "��ʼ������ý��...\n";
	play();

	env->taskScheduler().doEventLoop();

	return 0; // ֻ��Ϊ�˷�ֹ����������

}

void afterPlaying(void* /*clientData*/) {
	*env << "...���ļ��ж�ȡ���\n";

	Medium::close(videoSource);
	// ���رմ�Դ��ȡ�������ļ�

	play();
}

void play() {
	unsigned const inputDataChunkSize
		= TRANSPORT_PACKETS_PER_NETWORK_PACKET*TRANSPORT_PACKET_SIZE;

	// �������ļ���Ϊһ����ByteStreamFileSource":

	ByteStreamFileSource* fileSource
		= ByteStreamFileSource::createNew(*env, inputFileName, inputDataChunkSize);
	if (fileSource == NULL) {
		*env << "�޷����ļ� \"" << inputFileName
			<< "\" ��Ϊ file source\n";
		exit(1);
	}


	videoSource = MPEG2TransportStreamFramer::createNew(*env, fileSource);


	*env << "Beginning to read from file...\n";
	videoSink->startPlaying(*videoSource, afterPlaying, videoSink);
}

#endif