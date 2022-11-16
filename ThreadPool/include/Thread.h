// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#ifndef MUDUO_BASE_THREAD_H
#define MUDUO_BASE_THREAD_H

#include "Atomic.h"
// #include <muduo/base/Types.h>

#include <boost/function.hpp>
#include <boost/noncopyable.hpp>
#include <pthread.h>

#include <string>

using std::string;

namespace muduo {

class Thread : boost::noncopyable {
public:
  typedef boost::function<void()> ThreadFunc;

  explicit Thread(const ThreadFunc &, const string &name = string());
  ~Thread();

  void start();
  int join(); // return pthread_join()

  bool started() const { return started_; }
  // pthread_t pthreadId() const { return pthreadId_; }
  pid_t tid() const { return tid_; }
  const string &name() const { return name_; }

  static int numCreated() { return numCreated_.get(); }

private:
  static void *startThread(void *thread);
  void runInThread();

  bool started_;//是否是运行状态
  pthread_t pthreadId_;
  pid_t tid_;
  ThreadFunc func_;
  string name_;

  //记录创建线程的数量
  static AtomicInt32 numCreated_; //需要初始化，现在只是声明
};

} // namespace muduo
#endif
