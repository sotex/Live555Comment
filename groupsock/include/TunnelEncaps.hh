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
/* cookie���������û������ն��ϵ����ݣ�
Cookie����ʱҲ���临����ʽCookies��ָĳЩ��վΪ�˱���û���ݡ�����session���ٶ�
�������û������ն��ϵ����ݣ�ͨ���������ܣ���������RFC2109��2965���ѷ��������¹淶��RFC6265 ��
*/

/*tunnel������Ϊ������������(Tunnelling)�����Ǹ��ؼ������������������ָ��������һ������Э����������һ������Э�飬����Ҫ�����������Э����ʵ�����ֹ��ܡ�������������漰����������Э�飬���������Э�顢���Э������ĳ���Э������Э�������صı�����Э�顣
*/

// ����������˼�����ڲ��������ݳ�Ա���亯����Ա�ķ��ض�����thisΪ��׼����ƫ��
// ��ת�����ƫ�ƺ�ĵ�ַΪ��Ӧ��ָ�����ͣ���ȡָ��ָ���ڴ�����ݡ�
// ��������ಢ���������������󣬶�����Ϊһ��������ʹ�á������������´���
// unsigned long long t= 0x1239874560864216L;
//	cout << ((TunnelEncapsulationTrailer*)&t)->address() << endl;
//	cout << 0x12398745 << endl;

class TunnelEncapsulationTrailer {
	// The trailer is layed out as follows:
	// bytes 0-1:	source 'cookie'			ԴCookie
	// bytes 2-3:	destination 'cookie'	Ŀ��Cookie
	// bytes 4-7:	address					��ַ
	// bytes 8-9:	port					�˿�
	// byte 10:		ttl						TTL
	// byte 11:		command					����

        // Optionally, there may also be a 4-byte 'auxilliary address'
		// ���⣬Ҳ������һ��4�ֽڵ�"������ַ"
        // (e.g., for 'source-specific multicast' preceding this)
		// �����磬���ض�Դ�鲥���ڴ�֮ǰ��
        // bytes -4 through -1: auxilliary address
		// -4��-1�ֽ�(this֮ǰ4���ֽ�)��������ַ

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
	//ȡthisƫ��charIndex
	inline char* byteOffset(int charIndex)
		{ return ((char*)this) + charIndex; }
};

const unsigned TunnelEncapsulationTrailerSize = 12; // bytes�����װ�ϳ��ߴ�
const unsigned TunnelEncapsulationTrailerAuxSize = 4; // bytes�����ĳߴ�
const unsigned TunnelEncapsulationTrailerMaxSize	//���ߴ�
    = TunnelEncapsulationTrailerSize + TunnelEncapsulationTrailerAuxSize;

// Command codes:������
// 0: unused
const u_int8_t TunnelDataCmd = 1;			//�������������
const u_int8_t TunnelJoinGroupCmd = 2;		//�������������
const u_int8_t TunnelLeaveGroupCmd = 3;		//����뿪������
const u_int8_t TunnelTearDownCmd = 4;		//����������
const u_int8_t TunnelProbeCmd = 5;			//���̽������
const u_int8_t TunnelProbeAckCmd = 6;		//���̽��ACK����
const u_int8_t TunnelProbeNackCmd = 7;		//���̽��NACK����
const u_int8_t TunnelJoinRTPGroupCmd = 8;	//�������RTP������
const u_int8_t TunnelLeaveRTPGroupCmd = 9;	//����뿪RTP������

// 0x0A through 0x10: currently unused.0x0a��0x10��Ŀǰδʹ��
// a flag, not a cmd codeһ����ʶ�����������롣�����չ��ʶ
const u_int8_t TunnelExtensionFlag = 0x80;	//bits:1000 0000

const u_int8_t TunnelDataAuxCmd			//������ݸ�������
    = (TunnelExtensionFlag|TunnelDataCmd);
const u_int8_t TunnelJoinGroupAuxCmd	//��������鸨������
    = (TunnelExtensionFlag|TunnelJoinGroupCmd);
const u_int8_t TunnelLeaveGroupAuxCmd	//����뿪�鸨������
    = (TunnelExtensionFlag|TunnelLeaveGroupCmd);
// Note: the TearDown, Probe, ProbeAck, ProbeNack cmds have no Aux version
// ע�⣺TearDown(���),Probe(̽��),ProbeAck(Ack̽��),ProbeNack(NACK̽��)û�и���������
// 0x84 through 0x87: currently unused.
const u_int8_t TunnelJoinRTPGroupAuxCmd	//�������RTP�鸨������
    = (TunnelExtensionFlag|TunnelJoinRTPGroupCmd);
const u_int8_t TunnelLeaveRTPGroupAuxCmd//����뿪RTP�鸨������
    = (TunnelExtensionFlag|TunnelLeaveRTPGroupCmd);
// 0x8A through 0xFF: currently unused
	//�жϲ���cmd�Ƿ��Ǹ�������
inline Boolean TunnelIsAuxCmd(u_int8_t cmd) {
  return (cmd&TunnelExtensionFlag) != 0;
}

#endif
