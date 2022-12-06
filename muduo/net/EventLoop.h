// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//
// This is a public header file, it must only include public header files.

#ifndef MUDUO_NET_EVENTLOOP_H
#define MUDUO_NET_EVENTLOOP_H

#include <vector>

#include <boost/noncopyable.hpp>
#include <boost/scoped_ptr.hpp>

#include "CurrentThread.h"
#include "Mutex.h"
#include "Thread.h"
#include "Timestamp.h"

#include "Callbacks.h"
#include "TimerId.h"

namespace muduo {
namespace net {

//前置声明
class Channel;
class Poller;
class TimerQueue;
///
/// Reactor, at most one per thread.
///
/// This is an interface class, so don't expose too much details.
class EventLoop : boost::noncopyable {
public:
  typedef boost::function<void()> Functor;

  EventLoop();
  ~EventLoop(); // force out-line dtor, for scoped_ptr members.

  ///
  /// Loops forever.
  ///
  /// Must be called in the same thread as creation of the object.
  ///
  //开始事件循环
  void loop();

  //用于停止事件循环(不是立刻停止)
  void quit();

  ///
  /// Time when poll returns, usually means data arrivial.
  ///
  Timestamp pollReturnTime() const { return pollReturnTime_; }

  /// Runs callback immediately in the loop thread.
  /// It wakes up the loop, and run the cb.
  /// If in the same loop thread, cb is run within the function.
  /// Safe to call from other threads.
  void runInLoop(const Functor &cb);
  /// Queues callback in the loop thread.
  /// Runs after finish pooling.
  /// Safe to call from other threads.
  void queueInLoop(const Functor &cb);

  // timers

  ///
  /// Runs callback at 'time'.
  /// Safe to call from other threads.
  ///
  //直接回调
  TimerId runAt(const Timestamp &time, const TimerCallback &cb);
  ///
  /// Runs callback after @c delay seconds.
  /// Safe to call from other threads.
  ///
  //延期回调
  TimerId runAfter(double delay, const TimerCallback &cb);
  ///
  /// Runs callback every @c interval seconds.
  /// Safe to call from other threads.
  ///
  //以interval的频率回调
  TimerId runEvery(double interval, const TimerCallback &cb);
  ///
  /// Cancels the timer.
  /// Safe to call from other threads.
  ///
  void cancel(TimerId timerId);

  // internal usage
  void wakeup();
  void updateChannel(Channel *channel); // 在Poller中添加或者更新通道
  void removeChannel(Channel *channel); // 从Poller中移除通道

  //当EventLoop不在创建的IO线程中使用，对其进行处理
  void assertInLoopThread() {
    if (!isInLoopThread()) {
      abortNotInLoopThread();
    }
  }

  //判断是EventLoop是否是在创建的IO线程中调用
  bool isInLoopThread() const { return threadId_ == CurrentThread::tid(); }

  static EventLoop *getEventLoopOfCurrentThread();

private:
  void abortNotInLoopThread();
  void handleRead(); // waked up
  void doPendingFunctors();

  void printActiveChannels() const; // DEBUG

  typedef std::vector<Channel *> ChannelList;

  //说明：在linux中对bool类型的操作都是原子性的
  bool looping_;       //判断EventLoop是否在事件循环中
  bool quit_;          //判断是否停止事件循环(不是立刻停止)
  bool eventHandling_; //判断是否处于处理事件中
  bool callingPendingFunctors_; //判断是否IO线程处理一些回调任务中=>pendingFunctors_
  const pid_t threadId_;        // 当前对象所属线程ID
  Timestamp pollReturnTime_;    //执行完Poller::poll()的时间
  boost::scoped_ptr<Poller> poller_;
  boost::scoped_ptr<TimerQueue> timerQueue_;
  int wakeupFd_; // 用于eventfd
  // unlike in TimerQueue, which is an internal class,
  // we don't expose Channel to client.
  boost::scoped_ptr<Channel> wakeupChannel_; // 该通道将会纳入poller_来管理
  ChannelList activeChannels_;               // Poller返回的活动通道
  Channel *currentActiveChannel_; // 当前正在处理的活动通道
  MutexLock mutex_;
  std::vector<Functor> pendingFunctors_; // @BuardedBy mutex_   即将发生的回调，即在IO线程中执行需要执行回调函数集合
};

} // namespace net
} // namespace muduo
#endif // MUDUO_NET_EVENTLOOP_H
