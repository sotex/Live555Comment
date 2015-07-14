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
// Basic Hash Table implementation
// C++ header

#ifndef _BASIC_HASH_TABLE_HH
#define _BASIC_HASH_TABLE_HH

#ifndef _HASH_TABLE_HH
#include "HashTable.hh"
#endif
#ifndef _NET_COMMON_H
#include <NetCommon.h> // to ensure that "uintptr_t" is defined
#endif

// A simple hash table implementation, inspired by the hash table
// implementation used in Tcl 7.6: <http://www.tcl.tk/>

#define SMALL_HASH_TABLE_SIZE 4

class BasicHashTable : public HashTable {
private:
	class TableEntry; // forward

public:
	/**
	*	1����fBucketsָ��fStaticBuckets,��ʼ�������������ݳ�Ա
	*	2����FStaticBuckets��ֵ����(ȫ��ΪNULL)
	*/
	BasicHashTable(int keyType);
	virtual ~BasicHashTable();

//======== class iteratr =====================================
	// Used to iterate through the members of the table:
	class Iterator; friend class Iterator; // to make Sun's C++ compiler happy
	class Iterator : public HashTable::Iterator {
	public:
		//�󶨵�table
		Iterator(BasicHashTable& table);

	private: // implementation of inherited pure virtual functions
		//����keyΪ��һ���ڵ��key,������һ���ڵ��value�������һ�������ڣ�����NULL
		void* next(char const*& key); // returns 0 if none

	private:
		BasicHashTable& fTable;	//��һ����ϣ��
		unsigned fNextIndex; // index of next bucket to be enumerated after this
		TableEntry* fNextEntry; // next entry in the current bucket
	};
//===========================================================

private: // implementation of inherited pure virtual functions
		 //�̳еĴ��麯����ʵ��

	//�Ȳ���key��Ӧ����Ŀ�Ƿ���ڣ����ڵĻ��滻value,�����ھʹ���һ����Ŀ���������ϣ��
	virtual void* Add(char const* key, void* value);
	// Returns the old value if different, otherwise 0
	// �����ͬ�Ļ����ؾ�ֵ������Ϊ0

	virtual Boolean Remove(char const* key);
	virtual void* Lookup(char const* key) const;
	// Returns 0 if not found

	//��ȡ��ǰ��Ŀ��
	virtual unsigned numEntries() const;

private:
//======== class TableEntry =================================
	class TableEntry {
	public:
		TableEntry* fNext;	//��һ��ָ��
		char const* key;	//��
		void* value;		//ֵ
	};
//===========================================================
	
	//ʹ��key��ȷ��index��Ҫ���ҵ���Ŀ
	TableEntry* lookupKey(char const* key, unsigned& index) const;
	// returns entry matching "key", or NULL if none
	//���ء�key��ƥ�����Ŀ�����û���ҵ�����null

	//�Ƚ�����key�Ƿ�һ��
	Boolean keyMatches(char const* key1, char const* key2) const;
	// used to implement "lookupKey()"
	// ����ʵ�� "lookupKey()"

	//����һ����Ŀ��������뵽Ͱ�����indexͰ
	TableEntry* insertNewEntry(unsigned index, char const* key);
	// creates a new entry, and inserts it in the table
	// ����һ������Ŀ�������뵽�����ϣ��

	//��һ����Ŀentry��key��Ա��ֵ(��һ��key)
	void assignKey(TableEntry* entry, char const* key);
	// used to implement "insertNewEntry()"
	// ����ʵ�֡�insertNewEntry��

	//�ӹ�ϣ�����ҵ�entry���Ƴ�������
	void deleteEntry(unsigned index, TableEntry* entry);

	//����Ŀentry��keyɾ��
	void deleteKey(TableEntry* entry);
	// used to implement "deleteEntry()"
	// ����ʵ�� "deleteEntry()"

	//�ؽ���ϣ�����ؽ��ĳߴ�����ǰ���ı�
	void rebuild(); // rebuilds the table as its size increases
	//�ؽ�����Ϊ���ĳߴ�����Ӷ�����

	//��keyɢ������,ͨ��key����ȡһ������ֵ
	unsigned hashIndexFromKey(char const* key) const;
	// used to implement many of the routines above
	// ����ʵ���������ϵĳ���

	//�����������ʵ�������������һ����i�йص����ֵ�����ǵ��򲻿����
	unsigned randomIndex(uintptr_t i) const {
		//1103515245�����������˼��rand��������ͬ���㷨�����������
		//������������þ��Ƿ���һ�����ֵ����ΪĬ��fMask(0x3)��Ҳ����ֻ������λ
		//ΪʲôֻҪ����2λ��Ҳ����0 1 2 3 �����ֽ��������ΪͰĬ��ֻ���ĸ�
		// fDownShift������λ����Ĭ����28��ÿ�ε�����ϣ����С��ʱ����2
		return (unsigned)(((i * 1103515245) >> fDownShift) & fMask);
	}

private:
	TableEntry** fBuckets; // pointer to bucket array ָ�� Ͱ���飬Ͱ�б���TableEntry�����ַ
	TableEntry* fStaticBuckets[SMALL_HASH_TABLE_SIZE];// used for small tables ����С��
	unsigned fNumBuckets/*Ͱ��*/, fNumEntries/*�ڵ���*/, fRebuildSize/*�ؽ��ߴ��С*/, 
		fDownShift/*��������*/, fMask/*����*/;
	int fKeyType;
};

#endif