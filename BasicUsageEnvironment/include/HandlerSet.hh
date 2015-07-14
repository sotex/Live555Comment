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

// �������������(��Ϊ����Ľڵ�)
class HandlerDescriptor {

	//�������������privateȨ�޵ģ���Ϊֻ��������Ԫ��HandlerSet��������
	// ���nextHandler���������������˫�������ͷ���
	// ����������뵽nextHandler��nextHandler->fPrevHandler֮��
	HandlerDescriptor(HandlerDescriptor* nextHandler);

	// �������˫���������Ƴ����������һ����delete����������
	virtual ~HandlerDescriptor();

public:
	int socketNum;	//socket ����������������ʶ�ڵ�
	int conditionSet;	//��������
	//typedef void BackgroundHandlerProc(void* clientData, int mask);
	TaskScheduler::BackgroundHandlerProc* handlerProc;	//��̨���������ָ��
	void* clientData;	//�ͻ�������

private:
	// Descriptors are linked together in a doubly-linked list:
	friend class HandlerSet;
	friend class HandlerIterator;
	HandlerDescriptor* fNextHandler;	//��һ�������������
	HandlerDescriptor* fPrevHandler;	//��һ�������������
};

class HandlerSet {
public:
	//����fHandlers����һ������һ��ָ��fHandler�Լ�
	HandlerSet();
	//����ͷ�����ڵ�
	virtual ~HandlerSet();

	// �������в���socketNum�����HandlerDescriptor,���û���ҵ��ʹ���һ�������뵽����
	void assignHandler(int socketNum, int conditionSet, TaskScheduler::BackgroundHandlerProc* handlerProc, void* clientData);

	//�������в���socketNum��Ӧ��HandlerDescriptor���ҵ��˾�delete
	void clearHandler(int socketNum);

	// �������в���oldSocketNum�����HandlerDescriptor,�ҵ��˾ͽ���sockerNum��Ա�滻ΪnewSocketNum
	void moveHandler(int oldSocketNum, int newSocketNum);

private:
	// �������в���socketNum�����HandlerDescriptor��û�ҵ�����NULL
	HandlerDescriptor* lookupHandler(int socketNum);

private:
	friend class HandlerIterator;
	HandlerDescriptor fHandlers;	//��������������� ����ͷ�ڵ�
};

// ����������������������
class HandlerIterator {
public:
	// ����󶨵�һ�������������������󣬲�����reset()��fNextPtr��ֵΪhandlerSet.fNextHandler
	HandlerIterator(HandlerSet& handlerSet);
	virtual ~HandlerIterator();

	// ����fNextPtr,����fNextPtrָ����һ�����������������
	HandlerDescriptor* next(); // returns NULL if none
	void reset();	//�� fNextPtr ָ�������ͷ������һ��

private:
	HandlerSet& fOurSet;			//ָ������������
	HandlerDescriptor* fNextPtr;	//���������������ָ��
};

#endif
