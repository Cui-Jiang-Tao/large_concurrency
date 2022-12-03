// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include "Timer.h"

using namespace muduo;
using namespace muduo::net;

//默认初始化为0
AtomicInt64 Timer::s_numCreated_;

/**
 * 如果是重复的累加的，加入累加的时间重新设置到期时间，否则设置为失效时间
 */
void Timer::restart(Timestamp now) {
  if (repeat_) {
    // 重新计算下一个超时时刻
    expiration_ = addTime(now, interval_);
  } else {
    //设置无效的时间
    expiration_ = Timestamp::invalid();
  }
}
