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
	*	1、将fBuckets指向fStaticBuckets,初始化其他几个数据成员
	*	2、将FStaticBuckets数值清零(全置为NULL)
	*/
	BasicHashTable(int keyType);
	virtual ~BasicHashTable();

//======== class iteratr =====================================
	// Used to iterate through the members of the table:
	class Iterator; friend class Iterator; // to make Sun's C++ compiler happy
	class Iterator : public HashTable::Iterator {
	public:
		//绑定到table
		Iterator(BasicHashTable& table);

	private: // implementation of inherited pure virtual functions
		//设置key为下一个节点的key,返回下一个节点的value。如果下一个不存在，返回NULL
		void* next(char const*& key); // returns 0 if none

	private:
		BasicHashTable& fTable;	//绑定一个哈希表
		unsigned fNextIndex; // index of next bucket to be enumerated after this
		TableEntry* fNextEntry; // next entry in the current bucket
	};
//===========================================================

private: // implementation of inherited pure virtual functions
		 //继承的纯虚函数的实现

	//先查找key对应的条目是否存在，存在的话替换value,不存在就创建一个条目，并加入哈希表
	virtual void* Add(char const* key, void* value);
	// Returns the old value if different, otherwise 0
	// 如果不同的话返回旧值，否则为0

	virtual Boolean Remove(char const* key);
	virtual void* Lookup(char const* key) const;
	// Returns 0 if not found

	//获取当前条目数
	virtual unsigned numEntries() const;

private:
//======== class TableEntry =================================
	class TableEntry {
	public:
		TableEntry* fNext;	//下一个指针
		char const* key;	//键
		void* value;		//值
	};
//===========================================================
	
	//使用key来确定index和要查找的条目
	TableEntry* lookupKey(char const* key, unsigned& index) const;
	// returns entry matching "key", or NULL if none
	//返回“key”匹配的条目，如果没有找到返回null

	//比较两个key是否一样
	Boolean keyMatches(char const* key1, char const* key2) const;
	// used to implement "lookupKey()"
	// 用于实现 "lookupKey()"

	//创建一个条目，将其放入到桶数组的index桶
	TableEntry* insertNewEntry(unsigned index, char const* key);
	// creates a new entry, and inserts it in the table
	// 创建一个新条目，并插入到这个哈希表

	//给一个条目entry的key成员赋值(绑定一个key)
	void assignKey(TableEntry* entry, char const* key);
	// used to implement "insertNewEntry()"
	// 用于实现“insertNewEntry”

	//从哈希表中找到entry，移除后销毁
	void deleteEntry(unsigned index, TableEntry* entry);

	//将条目entry的key删除
	void deleteKey(TableEntry* entry);
	// used to implement "deleteEntry()"
	// 用于实现 "deleteEntry()"

	//重建哈希表，重建的尺寸是以前的四倍
	void rebuild(); // rebuilds the table as its size increases
	//重建表作为它的尺寸的增加而增加

	//从key散列索引,通过key来获取一个索引值
	unsigned hashIndexFromKey(char const* key) const;
	// used to implement many of the routines above
	// 用于实现许多以上的程序

	//随机索引，其实并非随机。产生一个与i有关的随机值，这是单向不可逆的
	unsigned randomIndex(uintptr_t i) const {
		//1103515245这个数很有意思，rand函数线性同余算法中用来溢出的
		//这个函数的作用就是返回一个随机值，因为默认fMask(0x3)，也就是只保留两位
		//为什么只要保留2位，也就是0 1 2 3 这四种结果咯，因为桶默认只有四个
		// fDownShift用来移位，其默认是28，每次调整哈希表大小的时候会减2
		return (unsigned)(((i * 1103515245) >> fDownShift) & fMask);
	}

private:
	TableEntry** fBuckets; // pointer to bucket array 指向 桶数组，桶中保存TableEntry对象地址
	TableEntry* fStaticBuckets[SMALL_HASH_TABLE_SIZE];// used for small tables 用于小表
	unsigned fNumBuckets/*桶数*/, fNumEntries/*节点数*/, fRebuildSize/*重建尺寸大小*/, 
		fDownShift/*降档变速*/, fMask/*掩码*/;
	int fKeyType;
};

#endif
