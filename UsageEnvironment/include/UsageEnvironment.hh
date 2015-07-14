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
// Usage Environment
// C++ header

#ifndef _USAGE_ENVIRONMENT_HH
#define _USAGE_ENVIRONMENT_HH

#ifndef _USAGEENVIRONMENT_VERSION_HH	//版本信息头文件
#include "UsageEnvironment_version.hh"
#endif

#ifndef _NETCOMMON_H
#include "NetCommon.h"	//网络通用组件，大量的类型宏定义
#endif

#ifndef _BOOLEAN_HH
#include "Boolean.hh"	//布尔值定义
#endif

#ifndef _STRDUP_HH
// "strDup()" is used often, so include this here, so everyone gets it:
//“strdup()”是经常使用的，因此包含在这里，所以大家都可以获取它：
#include "strDup.hh"
#endif

#ifndef NULL
#define NULL 0
#endif

#ifdef __BORLANDC__	//borland C++编译器
#define _setmode setmode
#define _O_BINARY O_BINARY
#endif

class TaskScheduler; // forward	前置声明

// An abstract base class, subclassed for each use of the library
// 一个抽象类，子类为每个使用库
class UsageEnvironment {
public:

	//reclaim	vt.开拓，开垦; 感化; 取回; 沙化; n.改造，感化; 教化; 回收再利用; 收回，取回;
	//自我回收，如果liveMediaPriv或groupsockPriv这两个成员变量有一个为NULL，就delete this;
	void reclaim();

	// task scheduler:任务调度
	//直接返回对象内部的fScheduler成员
	TaskScheduler& taskScheduler() const { return fScheduler; }

	// result message handling:
	//消息处理结果，注意这里是一个类型定义
	typedef char const* MsgString;
	//纯虚接口，看意思应该是获取消息处理结果
	virtual MsgString getResultMsg() const = 0;

	virtual void setResultMsg(MsgString msg) = 0;
	virtual void setResultMsg(MsgString msg1, MsgString msg2) = 0;
	virtual void setResultMsg(MsgString msg1, MsgString msg2, MsgString msg3) = 0;
	virtual void setResultErrMsg(MsgString msg, int err = 0) = 0;
	// like setResultMsg(), except that an 'errno' message is appended.  (If "err == 0", the "getErrno()" code is used instead.)
	//类似setResultMsg()，除了“errno”的消息被追加。（如果“err== 0”，“getErrno()”代码是用于替代。）

	virtual void appendToResultMsg(MsgString msg) = 0;

	virtual void reportBackgroundError() = 0;
	// used to report a (previously set) error message within
	//用于报告错误消息（预先设定）内的
	// a background event事件的背景

	virtual void internalError(); // used to 'handle' a 'should not occur'-type error condition within the library.

	// 'errno'
	virtual int getErrno() const = 0;

	// 'console' output:
	virtual UsageEnvironment& operator<<(char const* str) = 0;
	virtual UsageEnvironment& operator<<(int i) = 0;
	virtual UsageEnvironment& operator<<(unsigned u) = 0;
	virtual UsageEnvironment& operator<<(double d) = 0;
	virtual UsageEnvironment& operator<<(void* p) = 0;

	// a pointer to additional, optional, client-specific state
	// 客户端特定的状态
	void* liveMediaPriv;
	void* groupsockPriv;

protected:
	//初始化liveMediaPriv(NULL), groupsockPriv(NULL), fScheduler(scheduler)
	UsageEnvironment(TaskScheduler& scheduler); // abstract base class
	virtual ~UsageEnvironment(); // we are deleted only by reclaim()我们只有reclaim()删除

private:
	TaskScheduler& fScheduler;
};

//=======================================================================================

typedef void TaskFunc(void* clientData);
typedef void* TaskToken;	//token 标志
typedef u_int32_t EventTriggerId;	//Trigger 触发

//任务调度器
class TaskScheduler {
public:
	virtual ~TaskScheduler();

	/*
	* 这是一个纯虚接口，在BasicTaskScheduler0中有一个实现
	* 先判断microseconds(微秒)是否合法，不合法则改为0
	* 根据microseconds创建一个DelayInterval timeToDelay延时间隔
	* 创建AlarmHandler* alarmHandler使用函数的proc和clientData参数，以及上面创建的延时间隔对象
	* 调用fDelayQueue.addEntry(alarmHandler);添加到延时队列
	* 返回alarmHandler->token()调用的结果
	*/
	virtual TaskToken scheduleDelayedTask(int64_t microseconds, TaskFunc* proc,
		void* clientData) = 0;
	// Schedules a task to occur (after a delay) when we next
	// reach a scheduling point.
	// (Does not delay if "microseconds" <= 0)
	// Returns a token that can be used in a subsequent call to
	// unscheduleDelayedTask()


	/*
	* 这是一个纯虚接口，在BasicTaskScheduler0中有一个实现
	* 先从fDelayQueue中removeEntry这个prevTask,返回保存到alarmHandler
	* prevTask = NULL,注意这里的参数是引用类型
	* delete alarmHandler
	*/
	virtual void unscheduleDelayedTask(TaskToken& prevTask) = 0;
	// (Has no effect if "prevTask" == NULL)
	// 没有影响，如果 prevTask == NULL
	// Sets "prevTask" to NULL afterwards.
	// 完成之后将设置 prevTask 为NULL

	// 虚接口，重新调度延时任务
	// 先调用unscheduleDelayedTask(task);
	// 在调用task = scheduleDelayedTask(microseconds, proc, clientData);
	virtual void rescheduleDelayedTask(TaskToken& task,
		int64_t microseconds, TaskFunc* proc,
		void* clientData);
	// Combines "unscheduleDelayedTask()" with "scheduleDelayedTask()"
	// (setting "task" to the new task token).
	// 结合了”unscheduledelayedtask()”与“scheduledelayedtask()”
	// （设置的“task”到新的任务标记）。


	// For handling socket operations in the background (from the event loop):
	// 后台处理套接字操作类型（从事件循环）：注意这是一个类型定义，而不是一个函数
	typedef void BackgroundHandlerProc(void* clientData, int mask);
	// Possible bits to set in "mask".  (These are deliberately defined
	// the same as those in Tcl, to make a Tcl-based subclass easy.)
	// 设置掩码位为mask,这是特意这样定义的，为了符合Tcl接口的一致性
	// Tcl 是“工具控制语言（Tool Control Language）”的缩写。Tk 是 Tcl“图形工具箱”的扩展
	// 它提供各种标准的 GUI 接口项，以利于迅速进行高级应用程序开发。


#define SOCKET_READABLE    (1<<1)	//readable  adj.易读的;   易懂的;   
#define SOCKET_WRITABLE    (1<<2)	//writable  adj.可写下的，能写成文的; 
#define SOCKET_EXCEPTION   (1<<3)	//exception n.例外，除外; 反对，批评;[法律]异议，反对;

	//设置后台处理
	virtual void setBackgroundHandling(int socketNum, int conditionSet, BackgroundHandlerProc* handlerProc, void* clientData) = 0;
	//禁用后台处理
	void disableBackgroundHandling(int socketNum) { setBackgroundHandling(socketNum, 0, NULL, NULL); }
	virtual void moveSocketHandling(int oldSocketNum, int newSocketNum) = 0;
	// Changes any socket handling for "oldSocketNum" so that occurs with "newSocketNum" instead.
	// 改变任何套接字操作“oldsocketnum”，发生在“newsocketnum”代替。

	// BasicTaskScheduler0中
	// 如果 (watchVariable != NULL && *watchVariable != 0)成立，函数返回
	// 否则调用SingleStep();
	virtual void doEventLoop(char* watchVariable = NULL) = 0;
	// Causes further execution to take place within the event loop.
	// 造成进一步的执行到了事件循环中。
	// Delayed tasks, background I/O handling, and other events are handled, sequentially (as a single thread of control).
	// 延时任务，后台I/O操作，以及其他事件的处理，顺序（作为一个单独的线程控制）。
	// (If "watchVariable" is not NULL, then we return from this routine when *watchVariable != 0)
	// 如果 watchVariable ！= NULL,并且*watchVariable != 0,那么程序返回

	//创建一个事件触发器
	virtual EventTriggerId createEventTrigger(TaskFunc* eventHandlerProc) = 0;
	// Creates a 'trigger' for an event, which - if it occurs - will be handled (from the event loop) using "eventHandlerProc".
	// (Returns 0 iff no such trigger can be created (e.g., because of implementation limits on the number of triggers).)

	//删除一个事件触发器
	virtual void deleteEventTrigger(EventTriggerId eventTriggerId) = 0;

	//触发事件
	virtual void triggerEvent(EventTriggerId eventTriggerId, void* clientData = NULL) = 0;
	// Causes the (previously-registered) handler function for the specified event to be handled (from the event loop).
	// The handler function is called with "clientData" as parameter.
	// Note: This function (unlike other library functions) may be called from an external thread - to signal an external event.

	// The following two functions are deprecated, and are provided for backwards-compatibility only:
	//以下两个功能是过时的，并提供仅为了向后兼容
	// 打开后台处理读操作
	void turnOnBackgroundReadHandling(int socketNum, BackgroundHandlerProc* handlerProc, void* clientData) {
		setBackgroundHandling(socketNum, SOCKET_READABLE, handlerProc, clientData);
	}
	// 关闭后台处理读操作
	void turnOffBackgroundReadHandling(int socketNum) { disableBackgroundHandling(socketNum); }

	//内部错误
	virtual void internalError(); // used to 'handle' a 'should not occur'-type error condition within the library.用于“处理”“不应该发生的错误类型情况在库。

protected:
	TaskScheduler(); // abstract base class 抽象基类
};

#endif