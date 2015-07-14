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


#include "BasicUsageEnvironment.hh"
#include "HandlerSet.hh"
#include <stdio.h>
#if defined(_QNX4)
#include <sys/select.h>
#include <unix.h>
#endif

////////// BasicTaskScheduler //////////

BasicTaskScheduler* BasicTaskScheduler::createNew() {
	return new BasicTaskScheduler();
}

//最大调度粒度
#define MAX_SCHEDULER_GRANULARITY 10000 // 10 microseconds: We will return to the event loop at least this often
static void schedulerTickTask(void* clientData) {
	((BasicTaskScheduler*)clientData)->scheduleDelayedTask(MAX_SCHEDULER_GRANULARITY, schedulerTickTask, clientData);
}

BasicTaskScheduler::BasicTaskScheduler()
: fMaxNumSockets(0) {
	FD_ZERO(&fReadSet);
	FD_ZERO(&fWriteSet);
	FD_ZERO(&fExceptionSet);

	schedulerTickTask(this); // ensures that we handle events frequently
}

BasicTaskScheduler::~BasicTaskScheduler() {
}

#ifndef MILLION
#define MILLION 1000000
#endif

void BasicTaskScheduler::SingleStep(unsigned maxDelayTime) {
	//拷贝三个集合去给select调用做参数
	fd_set readSet = fReadSet; // make a copy for this select() call
	fd_set writeSet = fWriteSet; // ditto
	fd_set exceptionSet = fExceptionSet; // ditto
	//获取延时队列头结点的延时剩余时间(作为select超时时间)
	DelayInterval const& timeToDelay = fDelayQueue.timeToNextAlarm();
	struct timeval tv_timeToDelay;
	tv_timeToDelay.tv_sec = timeToDelay.seconds();
	tv_timeToDelay.tv_usec = timeToDelay.useconds();
	// Very large "tv_sec" values cause select() to fail.
	// Don't make it any larger than 1 million seconds (11.5 days)
	// 控制在1百万秒以内
	const long MAX_TV_SEC = MILLION;
	if (tv_timeToDelay.tv_sec > MAX_TV_SEC) {
		tv_timeToDelay.tv_sec = MAX_TV_SEC;
	}
	// Also check our "maxDelayTime" parameter (if it's > 0):
	if (maxDelayTime > 0 &&
		(tv_timeToDelay.tv_sec > (long)maxDelayTime / MILLION ||
		(tv_timeToDelay.tv_sec == (long)maxDelayTime / MILLION &&
		tv_timeToDelay.tv_usec > (long)maxDelayTime%MILLION))) {
		tv_timeToDelay.tv_sec = maxDelayTime / MILLION;
		tv_timeToDelay.tv_usec = maxDelayTime%MILLION;
	}

	//调用select来监控集合
	int selectResult = select(fMaxNumSockets, &readSet, &writeSet, &exceptionSet, &tv_timeToDelay);
//------------------------------------------------------------------------------------------
	//select出错返回，处理错误
	if (selectResult < 0) {
#if defined(__WIN32__) || defined(_WIN32)
		int err = WSAGetLastError();
		// For some unknown reason, select() in Windoze sometimes fails with WSAEINVAL if
		// it was called with no entries set in "readSet".  If this happens, ignore it:
		if (err == WSAEINVAL && readSet.fd_count == 0) {
			err = EINTR;
			// To stop this from happening again, create a dummy socket:
			int dummySocketNum = socket(AF_INET, SOCK_DGRAM, 0);
			FD_SET((unsigned)dummySocketNum, &fReadSet);
		}
		if (err != EINTR) {
#else
		if (errno != EINTR && errno != EAGAIN) {
#endif
			// Unexpected error - treat this as fatal:
#if !defined(_WIN32_WCE)
			perror("BasicTaskScheduler::SingleStep(): select() fails");
#endif
			//内部错误，调用abort()
			internalError();
		}
	}
//-----------------------------------------------------------------------------
	//开始处理
	// Call the handler function for one readable socket:
	HandlerIterator iter(*fHandlers);
	HandlerDescriptor* handler;
	// To ensure forward progress through the handlers, begin past the last
	// socket number that we handled:
	//注意fLastHandledSocketNum如果不为-1，说明已经调度过某些任务了
	if (fLastHandledSocketNum >= 0) {
		while ((handler = iter.next()) != NULL) {
			//从链表中找上一次最后调度的处理程序描述对象
			if (handler->socketNum == fLastHandledSocketNum) break;
		}
		if (handler == NULL) {
			fLastHandledSocketNum = -1;	//没有找到
			iter.reset(); // start from the beginning instead	迭代器回到起点
		}
	}
	//轮询处理
	//如果上面最后一个Handle == NULL成立了，那么这里不会进入，iter.next()还是会返回NULL
	//也就是说上次最后被调度的对象被找到了，这里的循环才会进入
	//这是为了提高效率，因为找到了最后一个被调度的元素，那么其之前的元素就都已经被调度过了
	while ((handler = iter.next()) != NULL) {
		int sock = handler->socketNum; // alias 别名
		int resultConditionSet = 0;		// 结果条件(状态)集合
		if (FD_ISSET(sock, &readSet) && FD_ISSET(sock, &fReadSet)/*sanity理智 check*/) resultConditionSet |= SOCKET_READABLE; //添加可读属性
		if (FD_ISSET(sock, &writeSet) && FD_ISSET(sock, &fWriteSet)/*sanity check*/) resultConditionSet |= SOCKET_WRITABLE; //添加可写属性
		if (FD_ISSET(sock, &exceptionSet) && FD_ISSET(sock, &fExceptionSet)/*sanity check*/) resultConditionSet |= SOCKET_EXCEPTION;	   //添加异常属性
		if ((resultConditionSet&handler->conditionSet) != 0 && handler->handlerProc != NULL) {
			fLastHandledSocketNum = sock;
			// Note: we set "fLastHandledSocketNum" before calling the handler,
			// in case the handler calls "doEventLoop()" reentrantly.
			//调用相关处理
			(*handler->handlerProc)(handler->clientData, resultConditionSet);
			break;
		}
	}
	//如果没有找到上次最后被调度的对象，并且fLastHandledSocketNum标识存在
	if (handler == NULL && fLastHandledSocketNum >= 0) {
		// We didn't call a handler, but we didn't get to check all of them,
		// so try again from the beginning:
		// 我们没有给一个处理程序，但我们没有去检查所有这些，所以试着重新开始：
		iter.reset();	//回到链表头
		//从链表第头开始轮询处理
		while ((handler = iter.next()) != NULL) {
			int sock = handler->socketNum; // alias
			int resultConditionSet = 0;
			if (FD_ISSET(sock, &readSet) && FD_ISSET(sock, &fReadSet)/*sanity check*/) resultConditionSet |= SOCKET_READABLE;
			if (FD_ISSET(sock, &writeSet) && FD_ISSET(sock, &fWriteSet)/*sanity check*/) resultConditionSet |= SOCKET_WRITABLE;
			if (FD_ISSET(sock, &exceptionSet) && FD_ISSET(sock, &fExceptionSet)/*sanity check*/) resultConditionSet |= SOCKET_EXCEPTION;

			if ((resultConditionSet&handler->conditionSet) != 0 && handler->handlerProc != NULL) {
				//设置fLastHandledSocketNum为最后一个被调用的处理程序的标识
				fLastHandledSocketNum = sock;
				// Note: we set "fLastHandledSocketNum" before calling the handler,
				// in case the handler calls "doEventLoop()" reentrantly.
				(*handler->handlerProc)(handler->clientData, resultConditionSet);
				break;
			}
		}
		// 没有一个合适的处理程序被调用
		if (handler == NULL) fLastHandledSocketNum = -1;//because we didn't call a handler
	}

//==========================================================================================

	// Also handle any newly-triggered event (Note that we do this *after* calling a socket handler,
	// in case the triggered event handler modifies The set of readable sockets.)
	// 处理等待触发的事件，这个在fTriggersAwaitingHandling中被标识
	if (fTriggersAwaitingHandling != 0) {
		if (fTriggersAwaitingHandling == fLastUsedTriggerMask) {
		//只有一个等待触发的事件
			// Common-case optimization for a single event trigger:
			fTriggersAwaitingHandling = 0;
			if (fTriggeredEventHandlers[fLastUsedTriggerNum] != NULL) {
				//函数调用
				(*fTriggeredEventHandlers[fLastUsedTriggerNum])(fTriggeredEventClientDatas[fLastUsedTriggerNum]);
			}
		}
		else {
			// 有多个等待触发的事件
			// Look for an event trigger that needs handling (making sure that we make forward progress through all possible triggers):
			unsigned i = fLastUsedTriggerNum;
			EventTriggerId mask = fLastUsedTriggerMask;

			do {
				i = (i + 1) % MAX_NUM_EVENT_TRIGGERS;
				mask >>= 1;
				if (mask == 0) mask = 0x80000000;

				if ((fTriggersAwaitingHandling&mask) != 0) {
					fTriggersAwaitingHandling &= ~mask;
					if (fTriggeredEventHandlers[i] != NULL) {
						(*fTriggeredEventHandlers[i])(fTriggeredEventClientDatas[i]);
					}

					fLastUsedTriggerMask = mask;
					fLastUsedTriggerNum = i;
					break;
				}
			} while (i != fLastUsedTriggerNum);
		}
	}
//======================================================================================
	// Also handle any delayed event that may have come due.
	// 处理延时队列中已经到时间的延时任务
	fDelayQueue.handleAlarm();
}

void BasicTaskScheduler
::setBackgroundHandling(int socketNum, int conditionSet, BackgroundHandlerProc* handlerProc, void* clientData) {
	if (socketNum < 0) return;	//标识不合法
	FD_CLR((unsigned)socketNum, &fReadSet);		//不监控此套接口的可读状态
	FD_CLR((unsigned)socketNum, &fWriteSet);	//写
	FD_CLR((unsigned)socketNum, &fExceptionSet);//异常
	if (conditionSet == 0) {	//不监控任何可操作状态
		fHandlers->clearHandler(socketNum);	//从链表中移除
		if (socketNum + 1 == fMaxNumSockets) {	//最大socket数减1，效率提升
			--fMaxNumSockets;
		}
	}
	else {
		//更新链表，分配处理程序
		fHandlers->assignHandler(socketNum, conditionSet, handlerProc, clientData);
		if (socketNum + 1 > fMaxNumSockets) {
			fMaxNumSockets = socketNum + 1;	//更新最大socket数
		}
		//设置要监控的状态
		if (conditionSet&SOCKET_READABLE) FD_SET((unsigned)socketNum, &fReadSet);
		if (conditionSet&SOCKET_WRITABLE) FD_SET((unsigned)socketNum, &fWriteSet);
		if (conditionSet&SOCKET_EXCEPTION) FD_SET((unsigned)socketNum, &fExceptionSet);
	}
}

void BasicTaskScheduler::moveSocketHandling(int oldSocketNum, int newSocketNum) {
	if (oldSocketNum < 0 || newSocketNum < 0) return; // sanity check完整性检查
	//清理三个集合中对oldSocketNum的监控
	if (FD_ISSET(oldSocketNum, &fReadSet)) { FD_CLR((unsigned)oldSocketNum, &fReadSet); FD_SET((unsigned)newSocketNum, &fReadSet); }
	if (FD_ISSET(oldSocketNum, &fWriteSet)) { FD_CLR((unsigned)oldSocketNum, &fWriteSet); FD_SET((unsigned)newSocketNum, &fWriteSet); }
	if (FD_ISSET(oldSocketNum, &fExceptionSet)) { FD_CLR((unsigned)oldSocketNum, &fExceptionSet); FD_SET((unsigned)newSocketNum, &fExceptionSet); }
	//替换socketNum
	fHandlers->moveHandler(oldSocketNum, newSocketNum);

	if (oldSocketNum + 1 == fMaxNumSockets) {
		--fMaxNumSockets;
	}
	if (newSocketNum + 1 > fMaxNumSockets) {
		fMaxNumSockets = newSocketNum + 1;
	}
}
