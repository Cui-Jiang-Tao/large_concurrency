// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#ifndef MUDUO_BASE_MUTEX_H
#define MUDUO_BASE_MUTEX_H

#include "CurrentThread.h"
#include <assert.h>
#include <boost/noncopyable.hpp>
#include <pthread.h>

namespace muduo {

class MutexLock : boost::noncopyable {
public:
  MutexLock() : holder_(0) {
    int ret = pthread_mutex_init(&mutex_, NULL);//初始化互斥量(mutex_)
    assert(ret == 0);//断言初始化成功
    // (void)ret;
  }

  ~MutexLock() {
    assert(holder_ == 0);//确保互斥量销毁时，已经没人持有它
    int ret = pthread_mutex_destroy(&mutex_);
    assert(ret == 0);//断言销毁成功
    // (void)ret;
  }

  bool isLockedByThisThread() { return holder_ == CurrentThread::tid(); }

  void assertLocked() { assert(isLockedByThisThread()); }

  // internal usage

  void lock() {
    pthread_mutex_lock(&mutex_);
    holder_ = CurrentThread::tid();
  }

  void unlock() {
    holder_ = 0;
    pthread_mutex_unlock(&mutex_);
  }

  //获取当前的互斥量地址
  pthread_mutex_t *getPthreadMutex() /* non-const */
  {
    return &mutex_;
  }

private:
  pthread_mutex_t mutex_;//互斥量
  pid_t holder_;//持有者的线程标识(tid)
};

class MutexLockGuard : boost::noncopyable {
public:
  explicit MutexLockGuard(MutexLock &mutex) : mutex_(mutex) { mutex_.lock(); }

  ~MutexLockGuard() { mutex_.unlock(); }

private:
  MutexLock &mutex_;
};

} // namespace muduo

// Prevent misuse like:
// MutexLockGuard(mutex_);
// A tempory object doesn't hold the lock for long!
#define MutexLockGuard(x) error "Missing guard object name"

#endif // MUDUO_BASE_MUTEX_H
