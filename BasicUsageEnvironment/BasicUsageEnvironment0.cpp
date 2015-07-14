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
// Copyright (c) 1996-2012 Live Networks, Inc.  All rights reserved.
// Basic Usage Environment: for a simple, non-scripted, console application
// Implementation

#include "BasicUsageEnvironment0.hh"
#include <stdio.h>

////////// BasicUsageEnvironment //////////

BasicUsageEnvironment0::BasicUsageEnvironment0(TaskScheduler& taskScheduler)
  : UsageEnvironment(taskScheduler),
    fBufferMaxSize(RESULT_MSG_BUFFER_MAX) {
  reset();
}

BasicUsageEnvironment0::~BasicUsageEnvironment0() {
}

void BasicUsageEnvironment0::reset() {
  fCurBufferSize = 0;
  fResultMsgBuffer[fCurBufferSize] = '\0';
}


// Implementation of virtual functions:

char const* BasicUsageEnvironment0::getResultMsg() const {
  return fResultMsgBuffer;
}

// ����reset����Ϣ���buffer�ؿգ��ٽ�msg������buffer
void BasicUsageEnvironment0::setResultMsg(MsgString msg) {
  reset();
  appendToResultMsg(msg);
}

void BasicUsageEnvironment0::setResultMsg(MsgString msg1, MsgString msg2) {
  setResultMsg(msg1);
  appendToResultMsg(msg2);
}

void BasicUsageEnvironment0::setResultMsg(MsgString msg1, MsgString msg2,
				       MsgString msg3) {
  setResultMsg(msg1, msg2);
  appendToResultMsg(msg3);
}

void BasicUsageEnvironment0::setResultErrMsg(MsgString msg, int err) {
  setResultMsg(msg);

#ifndef _WIN32_WCE
  appendToResultMsg(strerror(err == 0 ? getErrno() : err));
#endif
}

void BasicUsageEnvironment0::appendToResultMsg(MsgString msg) {
  char* curPtr = &fResultMsgBuffer[fCurBufferSize];
  unsigned spaceAvailable = fBufferMaxSize - fCurBufferSize;
  unsigned msgLength = strlen(msg);

  // Copy only enough of "msg" as will fit:
  // fResultMsgBufferʣ��ռ䲻���ţ�����һ����
  if (msgLength > spaceAvailable-1) {
    msgLength = spaceAvailable-1;
  }
  /* memmove���ڴ�src����count���ַ���dest�����Ŀ�������Դ�������ص��Ļ���memmove�ܹ�
     ��֤Դ���ڱ�����֮ǰ���ص�������ֽڿ�����Ŀ�������С������ƺ�src���ݻᱻ���ġ����ǵ�Ŀ��
	 ������Դ����û���ص����memcpy����������ͬ��*/
  memmove(curPtr, (char*)msg, msgLength);
  fCurBufferSize += msgLength;
  fResultMsgBuffer[fCurBufferSize] = '\0';	//���������
}
//��fResultMsgBuffer�е�����д�뵽��׼����
void BasicUsageEnvironment0::reportBackgroundError() {
  fputs(getResultMsg(), stderr);
}

