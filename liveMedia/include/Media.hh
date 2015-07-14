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
// "liveMedia"
// Copyright (c) 1996-2012 Live Networks, Inc.  All rights reserved.
// Medium
// C++ header

#ifndef _MEDIA_HH
#define _MEDIA_HH

#ifndef _LIVEMEDIA_VERSION_HH
#include "liveMedia_version.hh"
#endif

#ifndef _BOOLEAN_HH
#include "Boolean.hh"
#endif

#ifndef _USAGE_ENVIRONMENT_HH
#include "UsageEnvironment.hh"
#endif

// Lots of files end up needing the following, so just #include them here:
#ifndef _NET_COMMON_H
#include "NetCommon.h"
#endif
#include <stdio.h>

// The following makes the Borland compiler happy:
#ifdef __BORLANDC__
#define _strnicmp strnicmp
#define fabsf(x) fabs(x)
#endif

#define mediumNameMaxLen 30

class Medium {
public:
	// 从哈希表env.liveMedia->mediaTable中查找mediunName对应的Value，赋值给resultMedium
	static Boolean lookupByName(UsageEnvironment& env,
		char const* mediumName,
		Medium*& resultMedium);

	// 从哈希表env.liveMedia->mediaTable中查找mediumName对应的条目移除
	static void close(UsageEnvironment& env, char const* mediumName);

	// 从哈希表medium->envir().liveMedia->mediaTable中查找medium->name()对应的条目移除
	static void close(Medium* medium); // alternative close() method using ptrs
	// (has no effect if medium == NULL)

	UsageEnvironment& envir() const { return fEnviron; }

	char const* name() const { return fMediumName; }

	// Test for specific types of media对于媒体的特定类型的测试:
	// 默认都是返回false，在各个派生类中对相关的进行重定义
	virtual Boolean isSource() const;
	virtual Boolean isSink() const;
	virtual Boolean isRTCPInstance() const;
	virtual Boolean isRTSPClient() const;
	virtual Boolean isRTSPServer() const;
	virtual Boolean isMediaSession() const;
	virtual Boolean isServerMediaSession() const;
	virtual Boolean isDarwinInjector() const;

protected:
	// 绑定env，创建mediunName，添加到env.liveMedia->mediaTable
	Medium(UsageEnvironment& env); // abstract base class抽象基类
	virtual ~Medium(); // instances are deleted using close() only实例只能用close()来释放

	TaskToken& nextTask()
	{
		return fNextTask;
	}

private:
	friend class MediaLookupTable;
	UsageEnvironment& fEnviron;		//使用环境(绑定的)
	char fMediumName[mediumNameMaxLen];	//名称
	TaskToken fNextTask;	//下一个任务
};

// The structure pointed to by the "liveMediaPriv" UsageEnvironment field:
// UsageEnvironment结构的liveMediaPriv字段指向
class _Tables {
public:
	// 返回env.liveMediaPriv，如果其为NULL，且createIfNotPresent为true，则
	// return (_Tables*)(env.liveMediaPriv = new _Tables(env));
	static _Tables* getOurTables(UsageEnvironment& env, Boolean createIfNotPresent = True);
	// returns a pointer to an "ourTables" structure (creating it if necessary)
	// 返回一个指向“ourTables”结构（如果有必要创建它）

	// 自我销毁(在mediaTable和socketTable都为NULL时)
	void reclaimIfPossible();
	// used to delete ourselves when we're no longer used
	// 当我们不再使用时用于删除自己

	void* mediaTable;	//默认初始化为NULL
	void* socketTable;	//默认初始化为NULL

protected:
	_Tables(UsageEnvironment& env);
	virtual ~_Tables();

private:
	UsageEnvironment& fEnv;
};

#endif
