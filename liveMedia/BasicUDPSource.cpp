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
// A simple UDP source, where every UDP payload is a complete frame
// Implementation

#include "BasicUDPSource.hh"
#include <GroupsockHelper.hh>

BasicUDPSource* BasicUDPSource::createNew(UsageEnvironment& env,
	Groupsock* inputGS)
{
	return new BasicUDPSource(env, inputGS);
}

BasicUDPSource::BasicUDPSource(UsageEnvironment& env, Groupsock* inputGS)
: FramedSource(env), fInputGS(inputGS), fHaveStartedReading(False)
{
	// Try to use a large receive buffer (in the OS):����ʹ�ô���ջ���������OS�У���
	increaseReceiveBufferTo(env, inputGS->socketNum(), 50 * 1024);

	// Make the socket non-blocking, even though it will be read from only asynchronously, when packets arrive.
	//ʹ�׽��ַ���������ʹ��ֻ����첽��ȡ�������ݰ�����ʱ��
	// The reason for this is that, in some OSs, reads on a blocking socket can (allegedly) sometimes block,
	// even if the socket was previously reported (e.g., by "select()") as having data available.
	//��������ԭ���ǣ���һЩ����ϵͳ����ȡ�������׽��ֻᣨ��˵����һЩʱ����������ʹ��ǰ�׽���report�����磬ͨ��select���������п������ݡ�
	// (This can supposedly happen if the UDP checksum fails, for example.)
	//�����磺���UDPУ��ʧ�ܣ������Ʋⷢ������
	makeSocketNonBlocking(fInputGS->socketNum());
}

BasicUDPSource::~BasicUDPSource()
{
	// ȡ����fInputGS->socketNum()�ĺ�̨������
	envir().taskScheduler().turnOffBackgroundReadHandling(fInputGS->socketNum());
}

void BasicUDPSource::doGetNextFrame()
{
	if (!fHaveStartedReading) {
		// Await incoming packets:�ȴ������
		// ���Դ���UDP���ݰ��Ĵ������incomingPacketHandler��ӵ��������
		envir().taskScheduler().turnOnBackgroundReadHandling(fInputGS->socketNum(),
			(TaskScheduler::BackgroundHandlerProc*)&incomingPacketHandler, this);
		fHaveStartedReading = True;	//��ʶ�Ѿ���ʼ��ȡ����
	}
}

void BasicUDPSource::doStopGettingFrames()
{
	// �Ƴ���fInputGS->socketNum()�Ŀɶ���ز���
	envir().taskScheduler().turnOffBackgroundReadHandling(fInputGS->socketNum());
	fHaveStartedReading = False;
}

// ���������(static���Σ���Ϊ�ص�)
void BasicUDPSource::incomingPacketHandler(BasicUDPSource* source, int /*mask*/)
{
	source->incomingPacketHandler1();
}

// ʵ�ʵĴ��������
void BasicUDPSource::incomingPacketHandler1()
{
	// ��ǰû���������ڱ���ȡ
	if (!isCurrentlyAwaitingData()) return; // we're not ready for the data yet

	// Read the packet into our desired destination:
	// ��ȡ���ݰ�������������Ŀ�괦
	struct sockaddr_in fromAddress;
	if (!fInputGS->handleRead(fTo/*��ȡ�������ݴ��*/, fMaxSize, fFrameSize/*�����ֽ���*/, fromAddress)) return;

	// Tell our client that we have new data:�������ǵĿͻ����������µ����ݣ�
	afterGetting(this); // we're preceded by a net read; no infinite recursion����֮ǰ��һ�������ȡ;û�����޵ݹ�
}
