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
// "mTunnel" multicast access service
// Copyright (c) 1996-2012 Live Networks, Inc.  All rights reserved.
// IO event handlers
// Implementation

#include "IOHandlers.hh"
#include "TunnelEncaps.hh"

//##### TEMP: Use a single buffer, sized for UDP tunnels:
//##### TEMP：使用单个缓冲区，大小为UDP隧道：
//##### This assumes that the I/O handlers are non-reentrant
//#####这假定I / O处理程序是不可重入
static unsigned const maxPacketLength = 50 * 1024; // bytes
// This is usually overkill, because UDP packets are usually no larger
// than the typical Ethernet MTU (1500 bytes).  However, I've seen
// reports of Windows Media Servers sending UDP packets as large as
// 27 kBytes.  These will probably undego lots of IP-level
// fragmentation, but that occurs below us.  We just have to hope that
// fragments don't get lost.
// 这通常是矫枉过正，因为UDP数据包比典型的以太网MTU（1500字节）通常不会较大。不过，我见过Windows Media服务器发送UDP数据包的报告大小为27KB的。
// 上述很可能发生大量的IP级别的分片，而出现在我们下面。我们只是希望，片段不要丢失。

static unsigned const ioBufferSize	/*IO缓冲区尺寸=最大包长度+隧道最大尺寸*/
= maxPacketLength + TunnelEncapsulationTrailerMaxSize;
static unsigned char ioBuffer[ioBufferSize];	/*IO缓冲区*/


void socketReadHandler(Socket* sock, int /*mask*/)
{
	unsigned bytesRead;
	struct sockaddr_in fromAddress;
	UsageEnvironment& saveEnv = sock->env();	//备份env的引用
	// because handleRead(), if it fails, may delete "sock"
	// 因为如果handleRead()失败，可能会删除“_sock”

	// 从sock指定目标fromAddress获取数据到iobuffer(注意iobuffer是static的，只在此处使用)
	if (!sock->handleRead(ioBuffer, ioBufferSize, bytesRead, fromAddress)) {
		saveEnv.reportBackgroundError();	//报告错误
	}
}
