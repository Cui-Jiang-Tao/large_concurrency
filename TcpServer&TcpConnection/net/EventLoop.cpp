// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include "EventLoop.h"

#include "Channel.h"
#include "Logging.h"
#include "Poller.h"
#include "TimerQueue.h"

//#include <poll.h>

#include <boost/bind.hpp>

#include <sys/eventfd.h>

using namespace muduo;
using namespace muduo::net;

namespace {
// 当前线程EventLoop对象指针
// 线程局部存储
__thread EventLoop *t_loopInThisThread = nullptr;

const int kPollTimeMs = 10000;

int createEventfd() {
  int evtfd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
  if (evtfd < 0) {
    LOG_SYSERR << "Failed in eventfd";
    abort();
  }
  return evtfd;
}
} // namespace

EventLoop *EventLoop::getEventLoopOfCurrentThread() {
  return t_loopInThisThread;
}

EventLoop::EventLoop()
    : looping_(false), quit_(false), eventHandling_(false),
      callingPendingFunctors_(false), threadId_(CurrentThread::tid()),
      poller_(Poller::newDefaultPoller(this)),
      timerQueue_(new TimerQueue(this)), wakeupFd_(createEventfd()),
      wakeupChannel_(new Channel(this, wakeupFd_)),
      currentActiveChannel_(NULL) {
  LOG_TRACE << "EventLoop created " << this << " in thread " << threadId_;
  // 如果当前线程已经创建了EventLoop对象，终止(LOG_FATAL)
  if (t_loopInThisThread) {
    LOG_FATAL << "Another EventLoop " << t_loopInThisThread
              << " exists in this thread " << threadId_;
  } else {
    t_loopInThisThread = this;
  }

  wakeupChannel_->setReadCallback(boost::bind(&EventLoop::handleRead, this));
  // we are always reading the wakeupfd
  //这里设置回将wakeupChannel_注册到Poller对象中，实现事件循环
  wakeupChannel_->enableReading();
}

EventLoop::~EventLoop() {
  //析构，EventLoop对象一定不会是looping状态
  assert(!looping_);

  ::close(wakeupFd_);
  t_loopInThisThread = nullptr;
}

// 事件循环，该函数不能跨线程调用
// 只能在创建该对象的线程中调用
void EventLoop::loop() {
  assert(!looping_);
  // 断言当前处于创建该对象的线程中
  assertInLoopThread();
  looping_ = true;
  quit_ = false;
  LOG_TRACE << "EventLoop " << this << " start looping";

  //::poll(NULL, 0, 5*1000);
  while (!quit_) {
    activeChannels_.clear();
    //执行完Poller::poll()的时间
    pollReturnTime_ = poller_->poll(kPollTimeMs, &activeChannels_);
    //++iteration_;
    if (Logger::logLevel() <= Logger::TRACE) {
      printActiveChannels();
    }
    // TODO sort channel by priority
    eventHandling_ = true; //标识事件正在处理
    for (ChannelList::iterator it = activeChannels_.begin();
         it != activeChannels_.end(); ++it) {
      currentActiveChannel_ = *it;
      currentActiveChannel_->handleEvent(pollReturnTime_); //事件处理
    }
    currentActiveChannel_ = NULL;
    eventHandling_ = false; //标识事件已处理完成

    // 让IO线程也能执行一些计算任务，IO不忙的时候，处于阻塞状态
    doPendingFunctors(); // 执行其他线程或者本线程添加的一些回调任务
  }

  LOG_TRACE << "EventLoop " << this << " stop looping";
  looping_ = false;
}

// 该函数可以跨线程调用
void EventLoop::quit() {
  quit_ = true;
  if (!isInLoopThread()) {
    wakeup();
  }
}

// 在I/O线程中执行某个回调函数，该函数可以跨线程调用
void EventLoop::runInLoop(const Functor &cb) {
  if (isInLoopThread()) {
    // 如果是当前IO线程调用runInLoop，则同步调用cb
    cb();
  } else {
    // 如果是其它线程调用runInLoop，则异步地将cb添加到队列，以便让EventLoop所在的线程执行这个回调函数
    queueInLoop(cb);
  }
}

void EventLoop::queueInLoop(const Functor &cb) {
  {
    MutexLockGuard lock(mutex_);
    pendingFunctors_.push_back(cb);
  }

  /**
   * 1. 调用queueInLoop的线程不是IO线程需要唤醒
   * 2. 或者调用queueInLoop的线程是IO线程，并且此时正在调用任务队列(pendingFunctors_)，需要唤醒。只有I/O线程的事件回调中调用queueInLoop才不需要唤醒
   *    =》这里不明白，既然调用queueInLoop的线程是IO线程，IO线程怎么会此时正在调用pendingFunctors_？？？
   *        =》 因为I/O处于callingPendingFunctors_状态，任务队列(pendingFunctors_)中的任务执行完可能会处于阻塞状态(poll没事件发生)，新加入的任务无法执行。
   */
  if (!isInLoopThread() || callingPendingFunctors_) {
    wakeup();
  }
}

TimerId EventLoop::runAt(const Timestamp &time, const TimerCallback &cb) {
  return timerQueue_->addTimer(cb, time, 0.0);
}

TimerId EventLoop::runAfter(double delay, const TimerCallback &cb) {
  Timestamp time(addTime(Timestamp::now(), delay));
  return runAt(time, cb);
}

TimerId EventLoop::runEvery(double interval, const TimerCallback &cb) {
  Timestamp time(addTime(Timestamp::now(), interval));
  return timerQueue_->addTimer(cb, time, interval);
}

void EventLoop::cancel(TimerId timerId) { return timerQueue_->cancel(timerId); }

void EventLoop::updateChannel(Channel *channel) {
  assert(channel->ownerLoop() == this);
  assertInLoopThread();
  poller_->updateChannel(channel);
}

void EventLoop::removeChannel(Channel *channel) {
  assert(channel->ownerLoop() == this);
  assertInLoopThread();
  if (eventHandling_) {
    assert(currentActiveChannel_ == channel ||
           std::find(activeChannels_.begin(), activeChannels_.end(), channel) ==
               activeChannels_.end());
  }
  poller_->removeChannel(channel);
}

void EventLoop::abortNotInLoopThread() {
  LOG_FATAL << "EventLoop::abortNotInLoopThread - EventLoop " << this
            << " was created in threadId_ = " << threadId_
            << ", current thread id = " << CurrentThread::tid();
}

// 唤醒，写uint64_t类型的字节就可产生可读事件，达到唤醒的目的
void EventLoop::wakeup() {
  uint64_t one = 1;
  // ssize_t n = sockets::write(wakeupFd_, &one, sizeof one);
  ssize_t n = ::write(wakeupFd_, &one, sizeof one);
  if (n != sizeof one) {
    LOG_ERROR << "EventLoop::wakeup() writes " << n << " bytes instead of 8";
  }
}

// eventfd 事件处理函数
void EventLoop::handleRead() {
  uint64_t one = 1;
  // ssize_t n = sockets::read(wakeupFd_, &one, sizeof one);
  ssize_t n = ::read(wakeupFd_, &one, sizeof one);
  if (n != sizeof one) {
    LOG_ERROR << "EventLoop::handleRead() reads " << n << " bytes instead of 8";
  }
}

//在IO线程中执行一些回调任务
void EventLoop::doPendingFunctors() {
  std::vector<Functor> functors;
  callingPendingFunctors_ = true;

  {
    MutexLockGuard lock(mutex_);
    //可以减小临界区的长度(意味着不会阻塞其它线程的queueInLoop())，另一方面，也避免了死锁(因为Functor可能再次调用queueInLoop())
    functors.swap(pendingFunctors_);
  }

  for (size_t i = 0; i < functors.size(); ++i) {
    functors[i]();
  }
  callingPendingFunctors_ = false;
}

// TRACE级别下输出活跃的Channel信息的日志
void EventLoop::printActiveChannels() const {
  for (ChannelList::const_iterator it = activeChannels_.begin();
       it != activeChannels_.end(); ++it) {
    const Channel *ch = *it;
    LOG_TRACE << "{" << ch->reventsToString() << "} ";
  }
}
