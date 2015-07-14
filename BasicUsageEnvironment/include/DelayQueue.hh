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
// �¼��¼����� timeval�ķ�װ
class EventTime: public Timeval {
public:
  EventTime(unsigned secondsSinceEpoch = 0,
	    unsigned usecondsSinceEpoch = 0)
    // We use the Unix standard epoch: January 1, 1970
    : Timeval(secondsSinceEpoch, usecondsSinceEpoch) {}
};

//��ȡ��ǰʱ��
EventTime TimeNow();

extern EventTime const THE_END_OF_TIME;


///// DelayQueueEntry /////
// ��ʱ���м�¼(�ڵ�)   entry n.���룬�볡; ��ڴ����ſ�; �Ǽǣ���¼; �μӱ�������;
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
  DelayQueueEntry* fNext;	//��һ���ڵ�
  DelayQueueEntry* fPrev;	//��һ���ڵ�
  DelayInterval fDeltaTimeRemaining;	//��ʱʣ���ʱ��

  intptr_t fToken;	//��ʶ����ָ���ȵ�int��
  static intptr_t tokenCounter;	//��ʶ����(ע��˴���static ����)
};

///// DelayQueue /////
// ��ʱ����(����)
class DelayQueue: public DelayQueueEntry {
public:
	// ����ͷ���� ��ʱʣ��ʱ�� Ϊ ����
	// �������ͬ��ʱ��Ϊ��ǰʱ��
  DelayQueue();
  virtual ~DelayQueue();
  
  //��Ӽ�¼(�ڵ�)
  void addEntry(DelayQueueEntry* newEntry); // returns a token for the entry
  void updateEntry(DelayQueueEntry* entry, DelayInterval newDelay);
  void updateEntry(intptr_t tokenToFind, DelayInterval newDelay);
  void removeEntry(DelayQueueEntry* entry); // but doesn't delete it
  DelayQueueEntry* removeEntry(intptr_t tokenToFind); // but doesn't delete it

  // ��ȡͷ���� ��ʱʣ��ʱ��
  DelayInterval const& timeToNextAlarm();
  //�ж�ͷ���� ��ʱʣ��ʱ�� �Ƿ�Ϊ DELAY_ZERO �ǵĻ����������Ƴ�
  // ����ͷ������handleTimeout����(delete this)
  void handleAlarm();

private:
  DelayQueueEntry* head() { return fNext; }
  DelayQueueEntry* findEntryByToken(intptr_t token);

  //�ѡ�ʣ��ʱ�䡱����¡�
  //    �������ͬ��ʱ��Ϊ��ǰʱ��
  //    ������ͷ�ڵ㿪ʼ�����������ڵ����ʱʱ���Ƿ���,���˵�����Ϊ DELAY_ZERO
  //	��������Կ������������нڵ㱣��� ��ʱʣ��ʱ�� ����ǰһ���ڵ��й�ϵ��
  //	 ��ǰ�ڵ� �ܵ���ʱʱ�䣬Ӧ���ǵ�ǰ�ڵ�� ��ʱʣ��ʱ�� ����ǰһ���ڵ�� �ܵ���ʱʱ��
  void synchronize(); // bring the 'time remaining' fields up-to-date

  EventTime fLastSyncTime;	//���ͬ��ʱ��
};

#endif
