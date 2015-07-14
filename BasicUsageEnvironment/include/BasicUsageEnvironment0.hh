/**********	GPL授权协议
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

#ifndef _BASICUSAGEENVIRONMENT_VERSION_HH	//版本信息
#include "BasicUsageEnvironment_version.hh"	//版本信息的字符串宏定义在里面
#endif

#ifndef _USAGE_ENVIRONMENT_HH
#include "UsageEnvironment.hh"	//基本使用环境类继承自使用环境类
#endif

#ifndef _DELAY_QUEUE_HH
#include "DelayQueue.hh"		//基本环境使用类有一个成员是DealyQueue类对象(延时队列)
#endif

#define RESULT_MSG_BUFFER_MAX 1000

// An abstract base class, useful for subclassing
// (e.g., to redefine the implementation of "operator<<")
class BasicUsageEnvironment0 : public UsageEnvironment {
public:
	// redefined virtual functions:重定义虚函数

	//返回fResultMsgBuffer
	virtual MsgString getResultMsg() const;
	// 调用reset将消息结果buffer截空，再将msg(msg1-3)拷贝到buffer
	virtual void setResultMsg(MsgString msg);
	virtual void setResultMsg(MsgString msg1,
		MsgString msg2);
	virtual void setResultMsg(MsgString msg1,
		MsgString msg2,
		MsgString msg3);
	//将msg设置到fResultMsgBuffer，支持_WIN32_WCE的平台可能会加入strerror(errno)
	virtual void setResultErrMsg(MsgString msg, int err = 0);
	//将msg拷贝到fResultMsgBuffer可用部分，剩余空间不够时，只拷贝部分
	virtual void appendToResultMsg(MsgString msg);
	////将fResultMsgBuffer中的内容写入到标准错误
	virtual void reportBackgroundError();

protected:
	BasicUsageEnvironment0(TaskScheduler& taskScheduler);
	virtual ~BasicUsageEnvironment0();

private:
	void reset();	//截空buffer字符串(首元素置'\0')

	//消息处理结果缓冲
	char fResultMsgBuffer[RESULT_MSG_BUFFER_MAX];
	unsigned fCurBufferSize;	//当前buffer已用大小
	unsigned fBufferMaxSize;	//最大buffer大小
};


//===========================================================================


class HandlerSet; // forward

#define MAX_NUM_EVENT_TRIGGERS 32

// An abstract base class, useful for subclassing 抽象基类，用于子类化
// (e.g., to redefine the implementation of socket event handling)
// 例如，重新定义socket事件处理的实现
class BasicTaskScheduler0 : public TaskScheduler {
public:
	//析构的时候 delete fHandlers
	virtual ~BasicTaskScheduler0();

	//设置select轮询的超时时间的最大值，如果maxDelatTime不大于0，那么就设置为一百万秒
	virtual void SingleStep(unsigned maxDelayTime = 0) = 0;
	// "maxDelayTime" is in microseconds.  It allows a subclass to impose a limit
	// maxDelayTime 单位是微秒，它允许一个子类施加限制
	// on how long "select()" can delay, in case it wants to also do polling.
	// 多长时间”select()”可以延迟，如果它想做轮询。
	// 0 (the default value) means: There's no maximum; just look at the delay queue
	// 0作为默认值，意思是：没有最大；只是看看延迟队列

public:
	// Redefined virtual functions:重新定义虚函数

	/* 调度延时任务
	* 1、创建一个AlarmHandler对象（定时处理）;(new AlarmHandler(proc, clientData, timeToDelay);)
	* 2、将创建的alarmHandler对象添加到fDelayQueue中;(fDelayQueue.addEntry(alarmHandler))
	* 3、返回这个alarmHandler的token标志
	*/
	virtual TaskToken scheduleDelayedTask(int64_t microseconds, TaskFunc* proc,
		void* clientData);

	/* 取消调度延时任务
	* 1、从fDelayeQueue中removeEntry这个prevTask
	* 2、设置prevTask=NULL
	* 3、delete这个prevTask标识的alarmHandler对象
	*/
	virtual void unscheduleDelayedTask(TaskToken& prevTask);

	/* 做事件循环
	* 1、判断watchVariable !=0 && *watchVariable != 0是否成立，若成立，函数返回
	* 2、调用函数SingleStep();函数返回后继续做步骤1
	*/
	virtual void doEventLoop(char* watchVariable);

	/* 创建事件触发器ID
	*     从fTriggeredEventHandlers数组中寻找一个没有使用的位置 pos。如果没有空位，函数返回0
	*     将eventHandlerProc放置到上述数组 pos 位置
	*     将fTriggeredEventClientDatas数组 pos 位置置为NULL
	*     设置fLastUsedTriggerMask的第 pos 位为1
	*     设置fLastUsedTriggerNum为 pos
	*     返回fLastUsedTriggerMask的值
	*/
	virtual EventTriggerId createEventTrigger(TaskFunc* eventHandlerProc);

	/* 删除事件触发器 eventTriggerId可能代表多个事件触发器
	*   设置 fTriggersAwaitingHandling &=~ eventTriggerId 
	*   即将fTriggersAwaitingHandling中对应于eventTriggerId的非零位 置零
	*   从fTriggeredEventHandlers和fTriggeredEventClientDatas中将对应的位置置为NULL
	*/
	virtual void deleteEventTrigger(EventTriggerId eventTriggerId);

	/* 触发事件
	*    从fTriggeredEventClientDatas找到eventTriggerId对应的位置，设置为clientData
	*    将fTriggersAwaitingHandling中对应eventTriggerId中的非0位置为1
	*/
	virtual void triggerEvent(EventTriggerId eventTriggerId, void* clientData = NULL);

protected:
	BasicTaskScheduler0();

protected:
	// implement vt.实施，执行; 使生效，实现; 落实（政策）; 把…填满;n.工具，器械; 家具; 手段;[法]履行（契约等）;
	
	// To implement delayed operations: 实施延迟操作：
	DelayQueue fDelayQueue;

	// To implement background reads: 实施后台读
	HandlerSet* fHandlers;		//处理程序描述对象链表指针
	int fLastHandledSocketNum;	//当前最后一个调度的HandlerDescriptor对象的socketNum标识

	// To implement event triggers:	实施时间触发器
	// fTriggersAwaitingHandling触发等待处理的 fLastUsedTriggerMask 最后使用触发器的位置置1
	// implemented as 32-bit bitmaps 实现是32位的比特位图
	EventTriggerId fTriggersAwaitingHandling, fLastUsedTriggerMask;

	TaskFunc* fTriggeredEventHandlers[MAX_NUM_EVENT_TRIGGERS];	//保存事件触发器
	void* fTriggeredEventClientDatas[MAX_NUM_EVENT_TRIGGERS];	//保存触发事件客户端数据
	unsigned fLastUsedTriggerNum; // in the range(范围) [0,MAX_NUM_EVENT_TRIGGERS) 最后使用触发器的
};

#endif
