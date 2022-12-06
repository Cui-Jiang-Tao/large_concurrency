// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)

#define __STDC_LIMIT_MACROS
#include "TimerQueue.h"

#include "Logging.h"

#include "EventLoop.h"
#include "Timer.h"
#include "TimerId.h"

#include <boost/bind.hpp>

#include <sys/timerfd.h>

namespace muduo {
namespace net {
namespace detail {

/**
 * 创建定时器，生成一个定时器对象，返回与之关联的文件描述符。
 */
int createTimerfd() {
  int timerfd = ::timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
  if (timerfd < 0) {
    LOG_SYSFATAL << "Failed in timerfd_create";
  }
  return timerfd;
}

// 计算超时时刻与当前时间的时间差
struct timespec howMuchTimeFromNow(Timestamp when) {
  int64_t microseconds =
      when.microSecondsSinceEpoch() - Timestamp::now().microSecondsSinceEpoch();
  if (microseconds < 100) {
    microseconds = 100;
  }
  struct timespec ts;
  ts.tv_sec =
      static_cast<time_t>(microseconds / Timestamp::kMicroSecondsPerSecond);
  ts.tv_nsec = static_cast<long>(
      (microseconds % Timestamp::kMicroSecondsPerSecond) * 1000);
  return ts;
}

// 清除定时器，避免一直触发，并记录传入的时间戳
void readTimerfd(int timerfd, Timestamp now) {
  uint64_t howmany;
  ssize_t n = ::read(timerfd, &howmany, sizeof howmany);
  LOG_TRACE << "TimerQueue::handleRead() " << howmany << " at "
            << now.toString();

  if (n != sizeof howmany) {
    LOG_ERROR << "TimerQueue::handleRead() reads " << n
              << " bytes instead of 8";
  }
}

/**
 * 重置定时器的超时时间，重新设置Timer文件描述符(Timerfd)的超时时间，启动或停止定时器
 */
void resetTimerfd(int timerfd, Timestamp expiration) {
  // wake up loop by timerfd_settime()
  struct itimerspec newValue;
  struct itimerspec oldValue;
  bzero(&newValue, sizeof newValue);
  bzero(&oldValue, sizeof oldValue);
  newValue.it_value = howMuchTimeFromNow(expiration);

  //启动或停止定时器，依据newValue
  int ret = ::timerfd_settime(timerfd, 0, &newValue, &oldValue);
  if (ret) {
    LOG_SYSERR << "timerfd_settime()";
  }
}

} // namespace detail
} // namespace net
} // namespace muduo

using namespace muduo;
using namespace muduo::net;
using namespace muduo::net::detail;

/**
 * 一个TimerQueue只对应一个Eventloop和一个内部构造的Channel(构造对象时，默认绑定一个定时器对象timerfd_)
 * 同时设置Channel的回调函数TimerQueue::handleRead
 */
TimerQueue::TimerQueue(EventLoop *loop)
    : loop_(loop), timerfd_(createTimerfd()), timerfdChannel_(loop, timerfd_),
      timers_(), callingExpiredTimers_(false) {
  timerfdChannel_.setReadCallback(boost::bind(&TimerQueue::handleRead, this));
  // we are always reading the timerfd, we disarm it with timerfd_settime.
  timerfdChannel_.enableReading();
}

TimerQueue::~TimerQueue() {
  ::close(timerfd_); //关闭关联的描述符
  // do not remove channel, since we're in EventLoop::dtor();
  for (TimerList::iterator it = timers_.begin(); it != timers_.end(); ++it) {
    delete it->second;
  }
}

/**
 * 添加一个新Timer，并及时更新；
 * 如果TimerList中元素只有一个或插入的Timer最快到期，马上重新设置TimerQueue对象绑定的timerfd的超时时间
 */
TimerId TimerQueue::addTimer(const TimerCallback &cb, Timestamp when,
                             double interval) {
  /**
   * 该对象将存储到TimerList中
   * std::set<std::pair<Timestamp, std::unique_ptr<Timer>>>
   */
  Timer *timer = new Timer(cb, when, interval);

  loop_->runInLoop(boost::bind(&TimerQueue::addTimerInLoop, this, timer));

  // addTimerInLoop(timer);
  return TimerId(timer, timer->sequence());
}

void TimerQueue::cancel(TimerId timerId) {
  loop_->runInLoop(boost::bind(&TimerQueue::cancelInLoop, this, timerId));

  // cancelInLoop(timerId);
}

/**
 * I/O线程才可调用
 */
void TimerQueue::addTimerInLoop(Timer *timer) {
  loop_->assertInLoopThread();
  // 插入一个定时器，有可能会使得最早到期的定时器发生改变
  bool earliestChanged = insert(timer);

  if (earliestChanged) {
    // 重置定时器的超时时刻(timerfd_settime)
    resetTimerfd(timerfd_, timer->expiration());
  }
}

/**
 * I/O线程才可调用
 */
void TimerQueue::cancelInLoop(TimerId timerId) {
  loop_->assertInLoopThread();
  assert(timers_.size() == activeTimers_.size());

  ActiveTimer timer(timerId.timer_, timerId.sequence_);
  // 查找该定时器
  ActiveTimerSet::iterator it = activeTimers_.find(timer);
  if (it != activeTimers_.end()) {
    size_t n = timers_.erase(Entry(it->first->expiration(), it->first));
    assert(n == 1);
    (void)n;
    delete it->first; // FIXME: no delete
                      // please,如果用了unique_ptr,这里就不需要手动删除了
    activeTimers_.erase(it);
  } else if (callingExpiredTimers_) {
    // 已经到期，并且正在调用回调函数的定时器
    cancelingTimers_.insert(timer);
  }
  assert(timers_.size() == activeTimers_.size());
}

/**
 * I/O线程才可调用
 * 用来设置关联的Channel的回调函数
 */
void TimerQueue::handleRead() {
  loop_->assertInLoopThread();

  Timestamp now(Timestamp::now());
  // 清除该事件，避免一直触发，并记录触发TimerQueue::handleRead回调函数的时间
  readTimerfd(timerfd_, now);

  // 获取该时刻之前所有的定时器列表(即超时定时器列表)
  std::vector<Entry> expired = getExpired(now);

  callingExpiredTimers_ = true;
  cancelingTimers_.clear();
  // safe to callback outside critical section
  for (std::vector<Entry>::iterator it = expired.begin(); it != expired.end();
       ++it) {
    // 这里回调定时器处理函数
    it->second->run();
  }
  callingExpiredTimers_ = false;

  // 不是一次性定时器，需要重启
  reset(expired, now);
}

/**
 * rvo优化
 * 返回一个到期时间std::vector<Entry>的容器
 * =》typedef std::pair<Timestamp,Timer*> Entry;
 */
std::vector<TimerQueue::Entry> TimerQueue::getExpired(Timestamp now) {
  assert(timers_.size() == activeTimers_.size());

  std::vector<Entry> expired;
  // std::pair作为set的key时，比较pair 是先比较first, first相同再比较second
  Entry sentry(now, reinterpret_cast<Timer *>(UINTPTR_MAX));

  // 返回第一个未到期的Timer的迭代器
  // lower_bound的含义是返回第一个值>=sentry的元素的iterator
  // 即*end >= sentry，从而end->first > now
  TimerList::iterator end = timers_.lower_bound(sentry);

  // timers_的集合全是过期的时间戳，或者？？？
  assert(end == timers_.end() || now < end->first);

  // 将到期的定时器插入到expired中
  std::copy(timers_.begin(), end, back_inserter(expired));
  // 从timers_中移除到期的定时器
  timers_.erase(timers_.begin(), end);

  // 从activeTimers_中移除到期的定时器
  for (std::vector<Entry>::iterator it = expired.begin(); it != expired.end();
       ++it) {
    ActiveTimer timer(it->second, it->second->sequence());
    size_t n = activeTimers_.erase(timer);
    assert(n == 1);
    (void)n;
  }

  assert(timers_.size() == activeTimers_.size());
  return expired;
}

/**
 * 对已经过期的队列进行处理，如果是重复调用的，重新设置新的到期时间并加入Timer队列中继续监控；重新设置TimerQueue对象绑定的fd到期时间
 */
void TimerQueue::reset(const std::vector<Entry> &expired, Timestamp now) {
  Timestamp nextExpire;

  for (std::vector<Entry>::const_iterator it = expired.begin();
       it != expired.end(); ++it) {
    ActiveTimer timer(it->second, it->second->sequence());
    // 如果是重复的定时器并且是未取消定时器，则重启该定时器
    if (it->second->repeat() &&
        cancelingTimers_.find(timer) == cancelingTimers_.end()) {
      //如果是重复的累加的，加入累加的时间重新设置到期时间，否则设置为失效时间
      it->second->restart(now);
      //将累加后新的Timer插入到Timer队列中，继续监控
      insert(it->second);
    } else {
      // 一次性定时器或者已被取消的定时器是不能重置的，因此删除该定时器
      // FIXME move to a free list
      delete it->second; // FIXME: no delete please
    }
  }

  if (!timers_.empty()) {
    // //得到队列中第一个要到期的时间，获取最早到期的定时器超时时间
    nextExpire = timers_.begin()->second->expiration();
  }

  //队列中还未到期的时间是有效的
  if (nextExpire.valid()) {
    // 重置定时器的超时时刻(timerfd_settime)，重新设置TimerQueue对象绑定的fd到期时间
    resetTimerfd(timerfd_, nextExpire);
  }
}

/**
 * 返回值，插入的Timer是否是最早到期的时间;说明：插入的Timer元素是有序timers_(set集合)的第一个元素或唯一的元素为true
 * 功能：插入一个std::pair<Timestamp，Timer*>对象，将参数timer的到期时间戳作为std::pair的first，Timner对象作为sencond
 * activeTimers_ 和 timers_两个容器保存的Timer对象是一样的，仅仅内存分布不同而已
 */
bool TimerQueue::insert(Timer *timer) {
  loop_->assertInLoopThread();
  assert(timers_.size() == activeTimers_.size());

  // 最早到期时间是否改变
  bool earliestChanged = false;
  Timestamp when = timer->expiration();
  TimerList::iterator it = timers_.begin();

  // 如果timers_为空或者when小于timers_中的最早到期时间(这个插入的Timer是集合第一个到期的时间戳)
  if (it == timers_.end() || when < it->first) {
    earliestChanged = true;
  }

  {
    // 插入到timers_中
    std::pair<TimerList::iterator, bool> result =
        timers_.insert(Entry(when, timer));

    // result.first代表插入的元素的iterator，result.second代表元素是否插入成功
    assert(result.second);
    (void)result;
  }

  {
    // 插入到activeTimers_中
    std::pair<ActiveTimerSet::iterator, bool> result =
        activeTimers_.insert(ActiveTimer(timer, timer->sequence()));
    assert(result.second);
    (void)result;
  }

  assert(timers_.size() == activeTimers_.size());
  return earliestChanged;
}
