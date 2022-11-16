// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#ifndef MUDUO_BASE_CONDITION_H
#define MUDUO_BASE_CONDITION_H

#include "Mutex.h"

#include <boost/noncopyable.hpp>
#include <pthread.h>

namespace muduo {

class Condition : boost::noncopyable {
public:
  explicit Condition(MutexLock &mutex) : mutex_(mutex) {
    pthread_cond_init(&pcond_, NULL); //条件变量初始化
  }

  ~Condition() { pthread_cond_destroy(&pcond_); }

  //会解锁互斥量(unlock mutex_) 
  void wait() { pthread_cond_wait(&pcond_, mutex_.getPthreadMutex()); }

  // returns true if time out, false otherwise.
  bool waitForSeconds(int seconds);

  //和java的类似，一次唤醒一个线程，具体哪个线程被唤醒是不确定的（可以认为是随机的）。
  void notify() { pthread_cond_signal(&pcond_); }

  //唤醒所有等待的线程
  void notifyAll() { pthread_cond_broadcast(&pcond_); }

private:
  MutexLock &mutex_;
  pthread_cond_t pcond_;
};

} // namespace muduo
#endif // MUDUO_BASE_CONDITION_H
