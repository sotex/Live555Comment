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
	// Try to use a large receive buffer (in the OS):尽量使用大接收缓冲区（在OS中）：
	increaseReceiveBufferTo(env, inputGS->socketNum(), 50 * 1024);

	// Make the socket non-blocking, even though it will be read from only asynchronously, when packets arrive.
	//使套接字非阻塞，即使它只会从异步读取，在数据包到达时。
	// The reason for this is that, in some OSs, reads on a blocking socket can (allegedly) sometimes block,
	// even if the socket was previously reported (e.g., by "select()") as having data available.
	//这样做的原因是，在一些操作系统，读取上阻塞套接字会（据说）有一些时间阻塞，即使以前套接字report（例如，通过select（））其有可用数据。
	// (This can supposedly happen if the UDP checksum fails, for example.)
	//（例如：如果UDP校验失败，可以推测发生。）
	makeSocketNonBlocking(fInputGS->socketNum());
}

BasicUDPSource::~BasicUDPSource()
{
	// 取消对fInputGS->socketNum()的后台读操作
	envir().taskScheduler().turnOffBackgroundReadHandling(fInputGS->socketNum());
}

void BasicUDPSource::doGetNextFrame()
{
	if (!fHaveStartedReading) {
		// Await incoming packets:等待传入包
		// 将对传入UDP数据包的处理程序incomingPacketHandler添加到任务队列
		envir().taskScheduler().turnOnBackgroundReadHandling(fInputGS->socketNum(),
			(TaskScheduler::BackgroundHandlerProc*)&incomingPacketHandler, this);
		fHaveStartedReading = True;	//标识已经开始读取数据
	}
}

void BasicUDPSource::doStopGettingFrames()
{
	// 移除对fInputGS->socketNum()的可读监控操作
	envir().taskScheduler().turnOffBackgroundReadHandling(fInputGS->socketNum());
	fHaveStartedReading = False;
}

// 传入包处理(static修饰，作为回调)
void BasicUDPSource::incomingPacketHandler(BasicUDPSource* source, int /*mask*/)
{
	source->incomingPacketHandler1();
}

// 实际的传入包处理
void BasicUDPSource::incomingPacketHandler1()
{
	// 当前没有数据正在被读取
	if (!isCurrentlyAwaitingData()) return; // we're not ready for the data yet

	// Read the packet into our desired destination:
	// 读取数据包到我们期望的目标处
	struct sockaddr_in fromAddress;
	if (!fInputGS->handleRead(fTo/*读取到的数据存放*/, fMaxSize, fFrameSize/*读到字节数*/, fromAddress)) return;

	// Tell our client that we have new data:告诉我们的客户，我们有新的数据：
	afterGetting(this); // we're preceded by a net read; no infinite recursion我们之前有一个网络读取;没有无限递归
}
