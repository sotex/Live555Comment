/**********	GPL��ȨЭ��
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
// C++ header

#ifndef _BASIC_USAGE_ENVIRONMENT0_HH
#define _BASIC_USAGE_ENVIRONMENT0_HH

#ifndef _BASICUSAGEENVIRONMENT_VERSION_HH	//�汾��Ϣ
#include "BasicUsageEnvironment_version.hh"	//�汾��Ϣ���ַ����궨��������
#endif

#ifndef _USAGE_ENVIRONMENT_HH
#include "UsageEnvironment.hh"	//����ʹ�û�����̳���ʹ�û�����
#endif

#ifndef _DELAY_QUEUE_HH
#include "DelayQueue.hh"		//��������ʹ������һ����Ա��DealyQueue�����(��ʱ����)
#endif

#define RESULT_MSG_BUFFER_MAX 1000

// An abstract base class, useful for subclassing
// (e.g., to redefine the implementation of "operator<<")
class BasicUsageEnvironment0 : public UsageEnvironment {
public:
	// redefined virtual functions:�ض����麯��

	//����fResultMsgBuffer
	virtual MsgString getResultMsg() const;
	// ����reset����Ϣ���buffer�ؿգ��ٽ�msg(msg1-3)������buffer
	virtual void setResultMsg(MsgString msg);
	virtual void setResultMsg(MsgString msg1,
		MsgString msg2);
	virtual void setResultMsg(MsgString msg1,
		MsgString msg2,
		MsgString msg3);
	//��msg���õ�fResultMsgBuffer��֧��_WIN32_WCE��ƽ̨���ܻ����strerror(errno)
	virtual void setResultErrMsg(MsgString msg, int err = 0);
	//��msg������fResultMsgBuffer���ò��֣�ʣ��ռ䲻��ʱ��ֻ��������
	virtual void appendToResultMsg(MsgString msg);
	////��fResultMsgBuffer�е�����д�뵽��׼����
	virtual void reportBackgroundError();

protected:
	BasicUsageEnvironment0(TaskScheduler& taskScheduler);
	virtual ~BasicUsageEnvironment0();

private:
	void reset();	//�ؿ�buffer�ַ���(��Ԫ����'\0')

	//��Ϣ����������
	char fResultMsgBuffer[RESULT_MSG_BUFFER_MAX];
	unsigned fCurBufferSize;	//��ǰbuffer���ô�С
	unsigned fBufferMaxSize;	//���buffer��С
};


//===========================================================================


class HandlerSet; // forward

#define MAX_NUM_EVENT_TRIGGERS 32

// An abstract base class, useful for subclassing ������࣬�������໯
// (e.g., to redefine the implementation of socket event handling)
// ���磬���¶���socket�¼������ʵ��
class BasicTaskScheduler0 : public TaskScheduler {
public:
	//������ʱ�� delete fHandlers
	virtual ~BasicTaskScheduler0();

	//����select��ѯ�ĳ�ʱʱ������ֵ�����maxDelatTime������0����ô������Ϊһ������
	virtual void SingleStep(unsigned maxDelayTime = 0) = 0;
	// "maxDelayTime" is in microseconds.  It allows a subclass to impose a limit
	// maxDelayTime ��λ��΢�룬������һ������ʩ������
	// on how long "select()" can delay, in case it wants to also do polling.
	// �೤ʱ�䡱select()�������ӳ٣������������ѯ��
	// 0 (the default value) means: There's no maximum; just look at the delay queue
	// 0��ΪĬ��ֵ����˼�ǣ�û�����ֻ�ǿ����ӳٶ���

public:
	// Redefined virtual functions:���¶����麯��

	/* ������ʱ����
	* 1������һ��AlarmHandler���󣨶�ʱ����;(new AlarmHandler(proc, clientData, timeToDelay);)
	* 2����������alarmHandler������ӵ�fDelayQueue��;(fDelayQueue.addEntry(alarmHandler))
	* 3���������alarmHandler��token��־
	*/
	virtual TaskToken scheduleDelayedTask(int64_t microseconds, TaskFunc* proc,
		void* clientData);

	/* ȡ��������ʱ����
	* 1����fDelayeQueue��removeEntry���prevTask
	* 2������prevTask=NULL
	* 3��delete���prevTask��ʶ��alarmHandler����
	*/
	virtual void unscheduleDelayedTask(TaskToken& prevTask);

	/* ���¼�ѭ��
	* 1���ж�watchVariable !=0 && *watchVariable != 0�Ƿ����������������������
	* 2�����ú���SingleStep();�������غ����������1
	*/
	virtual void doEventLoop(char* watchVariable);

	/* �����¼�������ID
	*     ��fTriggeredEventHandlers������Ѱ��һ��û��ʹ�õ�λ�� pos�����û�п�λ����������0
	*     ��eventHandlerProc���õ��������� pos λ��
	*     ��fTriggeredEventClientDatas���� pos λ����ΪNULL
	*     ����fLastUsedTriggerMask�ĵ� pos λΪ1
	*     ����fLastUsedTriggerNumΪ pos
	*     ����fLastUsedTriggerMask��ֵ
	*/
	virtual EventTriggerId createEventTrigger(TaskFunc* eventHandlerProc);

	/* ɾ���¼������� eventTriggerId���ܴ������¼�������
	*   ���� fTriggersAwaitingHandling &=~ eventTriggerId 
	*   ����fTriggersAwaitingHandling�ж�Ӧ��eventTriggerId�ķ���λ ����
	*   ��fTriggeredEventHandlers��fTriggeredEventClientDatas�н���Ӧ��λ����ΪNULL
	*/
	virtual void deleteEventTrigger(EventTriggerId eventTriggerId);

	/* �����¼�
	*    ��fTriggeredEventClientDatas�ҵ�eventTriggerId��Ӧ��λ�ã�����ΪclientData
	*    ��fTriggersAwaitingHandling�ж�ӦeventTriggerId�еķ�0λ��Ϊ1
	*/
	virtual void triggerEvent(EventTriggerId eventTriggerId, void* clientData = NULL);

protected:
	BasicTaskScheduler0();

protected:
	// implement vt.ʵʩ��ִ��; ʹ��Ч��ʵ��; ��ʵ�����ߣ�; �ѡ�����;n.���ߣ���е; �Ҿ�; �ֶ�;[��]���У���Լ�ȣ�;
	
	// To implement delayed operations: ʵʩ�ӳٲ�����
	DelayQueue fDelayQueue;

	// To implement background reads: ʵʩ��̨��
	HandlerSet* fHandlers;		//�������������������ָ��
	int fLastHandledSocketNum;	//��ǰ���һ�����ȵ�HandlerDescriptor�����socketNum��ʶ

	// To implement event triggers:	ʵʩʱ�䴥����
	// fTriggersAwaitingHandling�����ȴ������ fLastUsedTriggerMask ���ʹ�ô�������λ����1
	// implemented as 32-bit bitmaps ʵ����32λ�ı���λͼ
	EventTriggerId fTriggersAwaitingHandling, fLastUsedTriggerMask;

	TaskFunc* fTriggeredEventHandlers[MAX_NUM_EVENT_TRIGGERS];	//�����¼�������
	void* fTriggeredEventClientDatas[MAX_NUM_EVENT_TRIGGERS];	//���津���¼��ͻ�������
	unsigned fLastUsedTriggerNum; // in the range(��Χ) [0,MAX_NUM_EVENT_TRIGGERS) ���ʹ�ô�������
};

#endif
