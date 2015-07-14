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
// C++ header

#ifndef _BASIC_USAGE_ENVIRONMENT_HH
#define _BASIC_USAGE_ENVIRONMENT_HH

#ifndef _BASIC_USAGE_ENVIRONMENT0_HH
#include "BasicUsageEnvironment0.hh"
#endif

class BasicUsageEnvironment : public BasicUsageEnvironment0 {
public:
	static BasicUsageEnvironment* createNew(TaskScheduler& taskScheduler);

	// redefined virtual functions:
	virtual int getErrno() const;

	// 向 stderr 输出内容。stderr是不带缓冲的
	virtual UsageEnvironment& operator<<(char const* str);
	virtual UsageEnvironment& operator<<(int i);
	virtual UsageEnvironment& operator<<(unsigned u);
	virtual UsageEnvironment& operator<<(double d);
	virtual UsageEnvironment& operator<<(void* p);

protected:
	// 避免直接构造对象，只能通过createNew来创建
	BasicUsageEnvironment(TaskScheduler& taskScheduler);
	// called only by "createNew()" (or subclass constructors)
	virtual ~BasicUsageEnvironment();
};

//===========================================================================

class BasicTaskScheduler : public BasicTaskScheduler0 {
public:
	static BasicTaskScheduler* createNew();
	virtual ~BasicTaskScheduler();

protected:
	BasicTaskScheduler();
	// called only by "createNew()"

protected:
	// Redefined virtual functions:

	/*
	*    设置select的超时时间为maxDelayTime(<=0 或大于一百万秒 时1百万秒)
	*    调用int selectResult = select(fMaxNumSockets, &readSet, &writeSet, &exceptionSet, &tv_timeToDelay);
	*    如果select出错返回，打印出错信息，并调用 internalError函数
	*    从处理程序描述链表中查找fLastHandledSocketNum代表的 处理程序描述对象指针，如果没找到，就在后面的while的时候从链表的头开始,否则从找到的位置开始
	*    从链表中取出处理程序描述节点对象，并调用其内部保存的后台处理程序
	*    设置fTriggersAwaitingHandling
	*    调用fDelayQueue.handleAlarm();
	*/
	virtual void SingleStep(unsigned maxDelayTime);
	// 添加到后台处理
	virtual void setBackgroundHandling(int socketNum, int conditionSet, BackgroundHandlerProc* handlerProc, void* clientData);
	// 从后台处理移出
	virtual void moveSocketHandling(int oldSocketNum, int newSocketNum);

protected:
	// To implement background operations: 实施后台操作
	int fMaxNumSockets;		//最大的socket数,select调用时提高效率
	fd_set fReadSet;		//监控读操作的集合
	fd_set fWriteSet;		//监控写操作的集合
	fd_set fExceptionSet;	//监控有异常的集合
};

#endif
