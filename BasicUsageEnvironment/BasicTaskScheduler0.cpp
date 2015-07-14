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
#include "HandlerSet.hh"

////////// A subclass of DelayQueueEntry,
//////////     used to implement BasicTaskScheduler0::scheduleDelayedTask()

class AlarmHandler : public DelayQueueEntry {
public:
	AlarmHandler(TaskFunc* proc, void* clientData, DelayInterval timeToDelay)
		: DelayQueueEntry(timeToDelay), fProc(proc), fClientData(clientData) {
	}

private: // redefined virtual functions
	virtual void handleTimeout() {
		(*fProc)(fClientData);
		DelayQueueEntry::handleTimeout();
	}

private:
	TaskFunc* fProc;
	void* fClientData;
};


////////// BasicTaskScheduler0 //////////

BasicTaskScheduler0::BasicTaskScheduler0()
: fLastHandledSocketNum(-1), fTriggersAwaitingHandling(0), fLastUsedTriggerMask(1), fLastUsedTriggerNum(MAX_NUM_EVENT_TRIGGERS - 1) {
	fHandlers = new HandlerSet;
	for (unsigned i = 0; i < MAX_NUM_EVENT_TRIGGERS; ++i) {
		fTriggeredEventHandlers[i] = NULL;
		fTriggeredEventClientDatas[i] = NULL;
	}
}

BasicTaskScheduler0::~BasicTaskScheduler0() {
	delete fHandlers;
}

TaskToken BasicTaskScheduler0::scheduleDelayedTask(int64_t microseconds,
	TaskFunc* proc,
	void* clientData) {
	if (microseconds < 0) microseconds = 0;
	DelayInterval timeToDelay((long)(microseconds / 1000000), (long)(microseconds % 1000000));
	AlarmHandler* alarmHandler = new AlarmHandler(proc, clientData, timeToDelay);
	fDelayQueue.addEntry(alarmHandler);

	return (void*)(alarmHandler->token());
}

void BasicTaskScheduler0::unscheduleDelayedTask(TaskToken& prevTask) {
	DelayQueueEntry* alarmHandler = fDelayQueue.removeEntry((intptr_t)prevTask);
	prevTask = NULL;
	delete alarmHandler;
}

void BasicTaskScheduler0::doEventLoop(char* watchVariable) {
	// Repeatedly loop, handling readble sockets and timed events:
	// 反复循环，可读取套接字和定时事件的处理：
	while (1) {
		if (watchVariable != NULL && *watchVariable != 0) break;
		SingleStep();
	}
}

EventTriggerId BasicTaskScheduler0::createEventTrigger(TaskFunc* eventHandlerProc) {
	unsigned i = fLastUsedTriggerNum;	//最后使用的触发器在数组的下标
	EventTriggerId mask = fLastUsedTriggerMask;	//bit位置

	do {
		i = (i + 1) % MAX_NUM_EVENT_TRIGGERS;	//0-31之间
		mask >>= 1;		//向左移位，与下标同步
		if (mask == 0) mask = 0x80000000;
		//找到一个未使用的位置，就将事件处理程序地址保存到此处
		if (fTriggeredEventHandlers[i] == NULL) {
			// This trigger number is free; use it:
			fTriggeredEventHandlers[i] = eventHandlerProc;
			fTriggeredEventClientDatas[i] = NULL; // sanity
			//更新这两个至
			fLastUsedTriggerMask = mask;
			fLastUsedTriggerNum = i;
			//这个返回值可以求得上面的数组位置值
			return mask;
		}
	} while (i != fLastUsedTriggerNum);

	// All available event triggers are allocated; return 0 instead:
	// 所有可用的事件触发都被分配；而返回0：
	return 0;
}


void BasicTaskScheduler0::deleteEventTrigger(EventTriggerId eventTriggerId) {
	//如果fTriggersAwaitingHandling不是0，而是等待触发处理位标识(标识数组fTriggeredEventHandlers已使用)
	//那么这儿很好理解，就是eventTriggerId中不为0的位，在对应的fTriggersAwaitingHandling中清零
	//相当于是标识fTriggeredEventHandlers相应的位置已经被释放了。
	fTriggersAwaitingHandling &= ~eventTriggerId;

	if (eventTriggerId == fLastUsedTriggerMask) { // common-case optimization:
		fTriggeredEventHandlers[fLastUsedTriggerNum] = NULL;
		fTriggeredEventClientDatas[fLastUsedTriggerNum] = NULL;
	}
	else {
		// "eventTriggerId" should have just one bit set.其可能是一个集合(要删除多个触发器)
		// However, we do the reasonable thing if the user happened to 'or' together two or more "EventTriggerId"s:
		//从将数组对应的位置清零
		EventTriggerId mask = 0x80000000;
		for (unsigned i = 0; i < MAX_NUM_EVENT_TRIGGERS; ++i) {
			if ((eventTriggerId&mask) != 0) {
				fTriggeredEventHandlers[i] = NULL;
				fTriggeredEventClientDatas[i] = NULL;
			}
			mask >>= 1;
		}
	}
}

void BasicTaskScheduler0::triggerEvent(EventTriggerId eventTriggerId, void* clientData) {
	// First, record the "clientData":首先，记录“客户端数据”：
	if (eventTriggerId == fLastUsedTriggerMask) { // common-case optimization:
		fTriggeredEventClientDatas[fLastUsedTriggerNum] = clientData;
	}
	else {
		EventTriggerId mask = 0x80000000;
		for (unsigned i = 0; i < MAX_NUM_EVENT_TRIGGERS; ++i) {
			if ((eventTriggerId&mask) != 0) {
				fTriggeredEventClientDatas[i] = clientData;

				fLastUsedTriggerMask = mask;
				fLastUsedTriggerNum = i;
			}
			mask >>= 1;
		}
	}

	// Then, note this event as being ready to be handled.
	// (Note that because this function (unlike others in the library) can be called from an external thread, we do this last, to
	//  reduce the risk of a race condition.)
	//  将fTriggersAwaitingHandling中对应的位置1。
	fTriggersAwaitingHandling |= eventTriggerId;
}


////////// HandlerSet (etc.) implementation //////////

HandlerDescriptor::HandlerDescriptor(HandlerDescriptor* nextHandler)
: conditionSet(0), handlerProc(NULL) {
	// Link this descriptor into a doubly-linked list:
	if (nextHandler == this) { // initialization
		fNextHandler = fPrevHandler = this;
	}
	else {
		fNextHandler = nextHandler;
		fPrevHandler = nextHandler->fPrevHandler;
		nextHandler->fPrevHandler = this;
		fPrevHandler->fNextHandler = this;
	}
}

HandlerDescriptor::~HandlerDescriptor() {
	// Unlink this descriptor from a doubly-linked list:
	fNextHandler->fPrevHandler = fPrevHandler;
	fPrevHandler->fNextHandler = fNextHandler;
}

HandlerSet::HandlerSet()
: fHandlers(&fHandlers) {
	fHandlers.socketNum = -1; // shouldn't ever get looked at, but in case...
}

HandlerSet::~HandlerSet() {
	// Delete each handler descriptor:
	while (fHandlers.fNextHandler != &fHandlers) {
		delete fHandlers.fNextHandler; // changes fHandlers->fNextHandler
	}
}

void HandlerSet
::assignHandler(int socketNum, int conditionSet, TaskScheduler::BackgroundHandlerProc* handlerProc, void* clientData) {
	// First, see if there's already a handler for this socket:
	HandlerDescriptor* handler = lookupHandler(socketNum);
	if (handler == NULL) { // No existing handler, so create a new descr:
		handler = new HandlerDescriptor(fHandlers.fNextHandler);
		handler->socketNum = socketNum;
	}

	handler->conditionSet = conditionSet;
	handler->handlerProc = handlerProc;
	handler->clientData = clientData;
}

void HandlerSet::clearHandler(int socketNum) {
	HandlerDescriptor* handler = lookupHandler(socketNum);
	delete handler;
}

void HandlerSet::moveHandler(int oldSocketNum, int newSocketNum) {
	HandlerDescriptor* handler = lookupHandler(oldSocketNum);
	if (handler != NULL) {
		handler->socketNum = newSocketNum;
	}
}

HandlerDescriptor* HandlerSet::lookupHandler(int socketNum) {
	HandlerDescriptor* handler;
	HandlerIterator iter(*this);
	while ((handler = iter.next()) != NULL) {
		if (handler->socketNum == socketNum) break;
	}
	return handler;
}

HandlerIterator::HandlerIterator(HandlerSet& handlerSet)
: fOurSet(handlerSet) {
	reset();
}

HandlerIterator::~HandlerIterator() {
}

void HandlerIterator::reset() {
	fNextPtr = fOurSet.fHandlers.fNextHandler;
}

HandlerDescriptor* HandlerIterator::next() {
	HandlerDescriptor* result = fNextPtr;
	//要注意的是，这里是走到了最后一个，因为这是循环链表
	if (result == &fOurSet.fHandlers) { // no more
		result = NULL;
	}
	else {
		fNextPtr = fNextPtr->fNextHandler;
	}

	return result;
}
