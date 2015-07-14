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

#ifndef _HANDLER_SET_HH
#define _HANDLER_SET_HH

#ifndef _BOOLEAN_HH
#include "Boolean.hh"
#endif

////////// HandlerSet (etc.) definition //////////

// 处理程序描述类(作为链表的节点)
class HandlerDescriptor {

	//构造和析构都是private权限的，因为只能在其友元类HandlerSet中来调用
	// 如果nextHandler是其自身，自身就是双向链表的头结点
	// 否则将自身插入到nextHandler和nextHandler->fPrevHandler之间
	HandlerDescriptor(HandlerDescriptor* nextHandler);

	// 将自身从双向链表中移除。这个函数一般由delete操作来调用
	virtual ~HandlerDescriptor();

public:
	int socketNum;	//socket 在链表里面用来标识节点
	int conditionSet;	//条件集合
	//typedef void BackgroundHandlerProc(void* clientData, int mask);
	TaskScheduler::BackgroundHandlerProc* handlerProc;	//后台处理程序函数指针
	void* clientData;	//客户端数据

private:
	// Descriptors are linked together in a doubly-linked list:
	friend class HandlerSet;
	friend class HandlerIterator;
	HandlerDescriptor* fNextHandler;	//下一个处理程序描述
	HandlerDescriptor* fPrevHandler;	//上一个处理程序描述
};

class HandlerSet {
public:
	//设置fHandlers的下一个和上一个指向fHandler自己
	HandlerSet();
	//逐个释放链表节点
	virtual ~HandlerSet();

	// 从链表中查找socketNum代表的HandlerDescriptor,如果没有找到就创建一个并加入到链表
	void assignHandler(int socketNum, int conditionSet, TaskScheduler::BackgroundHandlerProc* handlerProc, void* clientData);

	//从链表中查找socketNum对应的HandlerDescriptor，找到了就delete
	void clearHandler(int socketNum);

	// 从链表中查找oldSocketNum代表的HandlerDescriptor,找到了就将其sockerNum成员替换为newSocketNum
	void moveHandler(int oldSocketNum, int newSocketNum);

private:
	// 从链表中查找socketNum代表的HandlerDescriptor，没找到返回NULL
	HandlerDescriptor* lookupHandler(int socketNum);

private:
	friend class HandlerIterator;
	HandlerDescriptor fHandlers;	//处理程序描述对象 链表头节点
};

// 处理程序描述链表迭代器类
class HandlerIterator {
public:
	// 必须绑定到一个处理程序描述链表对象，并调用reset()将fNextPtr赋值为handlerSet.fNextHandler
	HandlerIterator(HandlerSet& handlerSet);
	virtual ~HandlerIterator();

	// 返回fNextPtr,并将fNextPtr指向下一个处理程序描述对象
	HandlerDescriptor* next(); // returns NULL if none
	void reset();	//将 fNextPtr 指向链表的头结点的下一个

private:
	HandlerSet& fOurSet;			//指向绑定链表的引用
	HandlerDescriptor* fNextPtr;	//处理程序描述对象指针
};

#endif
