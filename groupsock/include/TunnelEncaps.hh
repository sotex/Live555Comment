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
// Encapsulation trailer for tunnels
// C++ header

#ifndef _TUNNEL_ENCAPS_HH
#define _TUNNEL_ENCAPS_HH

#ifndef _NET_ADDRESS_HH
#include "NetAddress.hh"
#endif

typedef u_int16_t Cookie;
/* cookie（储存在用户本地终端上的数据）
Cookie，有时也用其复数形式Cookies，指某些网站为了辨别用户身份、进行session跟踪而
储存在用户本地终端上的数据（通常经过加密）。定义于RFC2109和2965都已废弃，最新规范是RFC6265 。
*/

/*tunnel中文译为隧道。网络隧道(Tunnelling)技术是个关键技术。网络隧道技术指的是利用一种网络协议来传输另一种网络协议，它主要利用网络隧道协议来实现这种功能。网络隧道技术涉及了三种网络协议，即网络隧道协议、隧道协议下面的承载协议和隧道协议所承载的被承载协议。
*/

// 这个类很有意思，它内部并无数据成员，其函数成员的返回都是以this为基准进行偏移
// 后，转换这个偏移后的地址为相应的指针类型，再取指针指向内存的内容。
// 所以这个类并不会用来创建对象，而是作为一种类型来使用。可能诸如以下代码
// unsigned long long t= 0x1239874560864216L;
//	cout << ((TunnelEncapsulationTrailer*)&t)->address() << endl;
//	cout << 0x12398745 << endl;

class TunnelEncapsulationTrailer {
	// The trailer is layed out as follows:
	// bytes 0-1:	source 'cookie'			源Cookie
	// bytes 2-3:	destination 'cookie'	目的Cookie
	// bytes 4-7:	address					地址
	// bytes 8-9:	port					端口
	// byte 10:		ttl						TTL
	// byte 11:		command					命令

        // Optionally, there may also be a 4-byte 'auxilliary address'
		// 随意，也可能有一个4字节的"辅助地址"
        // (e.g., for 'source-specific multicast' preceding this)
		// （例如，“特定源组播”在此之前）
        // bytes -4 through -1: auxilliary address
		// -4到-1字节(this之前4个字节)，辅助地址

    public:
	Cookie& srcCookie()
		{ return *(Cookie*)byteOffset(0); }
	Cookie& dstCookie()
		{ return *(Cookie*)byteOffset(2); }
	u_int32_t& address()
		{ return *(u_int32_t*)byteOffset(4); }
	Port& port()
		{ return *(Port*)byteOffset(8); }
	u_int8_t& ttl()
		{ return *(u_int8_t*)byteOffset(10); }
	u_int8_t& command()
		{ return *(u_int8_t*)byteOffset(11); }

		u_int32_t& auxAddress()
				{ return *(u_int32_t*)byteOffset(-4); }

    private:	
	//取this偏移charIndex
	inline char* byteOffset(int charIndex)
		{ return ((char*)this) + charIndex; }
};

const unsigned TunnelEncapsulationTrailerSize = 12; // bytes隧道封装拖车尺寸
const unsigned TunnelEncapsulationTrailerAuxSize = 4; // bytes辅助的尺寸
const unsigned TunnelEncapsulationTrailerMaxSize	//最大尺寸
    = TunnelEncapsulationTrailerSize + TunnelEncapsulationTrailerAuxSize;

// Command codes:命令码
// 0: unused
const u_int8_t TunnelDataCmd = 1;			//隧道的数据命令
const u_int8_t TunnelJoinGroupCmd = 2;		//隧道连接组命令
const u_int8_t TunnelLeaveGroupCmd = 3;		//隧道离开组命令
const u_int8_t TunnelTearDownCmd = 4;		//隧道拆除命令
const u_int8_t TunnelProbeCmd = 5;			//隧道探针命令
const u_int8_t TunnelProbeAckCmd = 6;		//隧道探针ACK命令
const u_int8_t TunnelProbeNackCmd = 7;		//隧道探针NACK命令
const u_int8_t TunnelJoinRTPGroupCmd = 8;	//隧道加入RTP组命令
const u_int8_t TunnelLeaveRTPGroupCmd = 9;	//隧道离开RTP组命令

// 0x0A through 0x10: currently unused.0x0a到0x10：目前未使用
// a flag, not a cmd code一个标识，不是命令码。隧道扩展标识
const u_int8_t TunnelExtensionFlag = 0x80;	//bits:1000 0000

const u_int8_t TunnelDataAuxCmd			//隧道数据辅助命令
    = (TunnelExtensionFlag|TunnelDataCmd);
const u_int8_t TunnelJoinGroupAuxCmd	//隧道连接组辅助命令
    = (TunnelExtensionFlag|TunnelJoinGroupCmd);
const u_int8_t TunnelLeaveGroupAuxCmd	//隧道离开组辅助命令
    = (TunnelExtensionFlag|TunnelLeaveGroupCmd);
// Note: the TearDown, Probe, ProbeAck, ProbeNack cmds have no Aux version
// 注意：TearDown(拆除),Probe(探针),ProbeAck(Ack探针),ProbeNack(NACK探针)没有辅助版命令
// 0x84 through 0x87: currently unused.
const u_int8_t TunnelJoinRTPGroupAuxCmd	//隧道加入RTP组辅助命令
    = (TunnelExtensionFlag|TunnelJoinRTPGroupCmd);
const u_int8_t TunnelLeaveRTPGroupAuxCmd//隧道离开RTP组辅助命令
    = (TunnelExtensionFlag|TunnelLeaveRTPGroupCmd);
// 0x8A through 0xFF: currently unused
	//判断参数cmd是否是辅助命令
inline Boolean TunnelIsAuxCmd(u_int8_t cmd) {
  return (cmd&TunnelExtensionFlag) != 0;
}

#endif
