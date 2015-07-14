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
 // Copyright (c) 1996-2012, Live Networks, Inc.  All rights reserved
// Delay queue
// C++ header

#ifndef _DELAY_QUEUE_HH
#define _DELAY_QUEUE_HH

#ifndef _NET_COMMON_H
#include "NetCommon.h"
#endif

#ifdef TIME_BASE
typedef TIME_BASE time_base_seconds;
#else
typedef long time_base_seconds;	//long int
#endif

///// A "Timeval" can be either an absolute time, or a time interval /////

class Timeval {
public:
  time_base_seconds seconds() const {
    return fTv.tv_sec;
  }
  time_base_seconds seconds() {
    return fTv.tv_sec;
  }
  time_base_seconds useconds() const {
    return fTv.tv_usec;
  }
  time_base_seconds useconds() {
    return fTv.tv_usec;
  }

  int operator>=(Timeval const& arg2) const;
  int operator<=(Timeval const& arg2) const {
    return arg2 >= *this;
  }
  int operator<(Timeval const& arg2) const {
    return !(*this >= arg2);
  }
  int operator>(Timeval const& arg2) const {
    return arg2 < *this;
  }
  int operator==(Timeval const& arg2) const {
    return *this >= arg2 && arg2 >= *this;
  }
  int operator!=(Timeval const& arg2) const {
    return !(*this == arg2);
  }

  void operator+=(class DelayInterval const& arg2);
  void operator-=(class DelayInterval const& arg2);
  // returns ZERO iff arg2 >= arg1

protected:
  Timeval(time_base_seconds seconds, time_base_seconds useconds) {
    fTv.tv_sec = seconds; fTv.tv_usec = useconds;
  }

private:
  time_base_seconds& secs() {
    return (time_base_seconds&)fTv.tv_sec;
  }
  time_base_seconds& usecs() {
    return (time_base_seconds&)fTv.tv_usec;
  }

  struct timeval fTv;
};

#ifndef max
inline Timeval max(Timeval const& arg1, Timeval const& arg2) {
  return arg1 >= arg2 ? arg1 : arg2;
}
#endif
#ifndef min
inline Timeval min(Timeval const& arg1, Timeval const& arg2) {
  return arg1 <= arg2 ? arg1 : arg2;
}
#endif

class DelayInterval operator-(Timeval const& arg1, Timeval const& arg2);
// returns ZERO iff arg2 >= arg1


///// DelayInterval /////

class DelayInterval: public Timeval {
public:
  DelayInterval(time_base_seconds seconds, time_base_seconds useconds)
    : Timeval(seconds, useconds) {}
};

DelayInterval operator*(short arg1, DelayInterval const& arg2);

extern DelayInterval const DELAY_ZERO;
extern DelayInterval const DELAY_SECOND;
DelayInterval const DELAY_MINUTE = 60*DELAY_SECOND;
DelayInterval const DELAY_HOUR = 60*DELAY_MINUTE;
DelayInterval const DELAY_DAY = 24*DELAY_HOUR;

///// EventTime /////
// 事件事件，对 timeval的封装
class EventTime: public Timeval {
public:
  EventTime(unsigned secondsSinceEpoch = 0,
	    unsigned usecondsSinceEpoch = 0)
    // We use the Unix standard epoch: January 1, 1970
    : Timeval(secondsSinceEpoch, usecondsSinceEpoch) {}
};

//获取当前时间
EventTime TimeNow();

extern EventTime const THE_END_OF_TIME;


///// DelayQueueEntry /////
// 延时队列记录(节点)   entry n.进入，入场; 入口处，门口; 登记，记录; 参加比赛的人;
class DelayQueueEntry {
public:
  virtual ~DelayQueueEntry();

  intptr_t token() {
    return fToken;
  }

protected: // abstract base class
  DelayQueueEntry(DelayInterval delay);

  // delete this;
  virtual void handleTimeout();

private:
  friend class DelayQueue;
  DelayQueueEntry* fNext;	//下一个节点
  DelayQueueEntry* fPrev;	//上一个节点
  DelayInterval fDeltaTimeRemaining;	//延时剩余的时间

  intptr_t fToken;	//标识，等指针宽度的int型
  static intptr_t tokenCounter;	//标识计数(注意此处是static 变量)
};

///// DelayQueue /////
// 延时队列(链表)
class DelayQueue: public DelayQueueEntry {
public:
	// 设置头结点的 延时剩余时间 为 永恒
	// 设置最后同步时间为当前时间
  DelayQueue();
  virtual ~DelayQueue();
  
  //添加记录(节点)
  void addEntry(DelayQueueEntry* newEntry); // returns a token for the entry
  void updateEntry(DelayQueueEntry* entry, DelayInterval newDelay);
  void updateEntry(intptr_t tokenToFind, DelayInterval newDelay);
  void removeEntry(DelayQueueEntry* entry); // but doesn't delete it
  DelayQueueEntry* removeEntry(intptr_t tokenToFind); // but doesn't delete it

  // 获取头结点的 延时剩余时间
  DelayInterval const& timeToNextAlarm();
  //判断头结点的 延时剩余时间 是否为 DELAY_ZERO 是的话从链表中移除
  // 并由头结点调用handleTimeout方法(delete this)
  void handleAlarm();

private:
  DelayQueueEntry* head() { return fNext; }
  DelayQueueEntry* findEntryByToken(intptr_t token);

  //把“剩余时间”域更新。
  //    设置最后同步时间为当前时间
  //    从链表头节点开始，遍历，看节点的延时时间是否到了,到了的设置为 DELAY_ZERO
  //	从这里可以看出来，链表中节点保存的 延时剩余时间 是与前一个节点有关系的
  //	 当前节点 总的延时时间，应该是当前节点的 延时剩余时间 加上前一个节点的 总的延时时间
  void synchronize(); // bring the 'time remaining' fields up-to-date

  EventTime fLastSyncTime;	//最后同步时间
};

#endif
