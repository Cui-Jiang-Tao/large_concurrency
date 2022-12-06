// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#ifndef MUDUO_BASE_THREADPOOL_H
#define MUDUO_BASE_THREADPOOL_H

#include "Condition.h"
#include "Mutex.h"
#include "Thread.h"
// #include "Types.h"

#include <boost/function.hpp>
#include <boost/noncopyable.hpp>
#include <boost/ptr_container/ptr_vector.hpp>

#include <deque>

namespace muduo {

class ThreadPool : boost::noncopyable {
public:
  typedef boost::function<void()> Task;

  explicit ThreadPool(const string &name = string());
  ~ThreadPool();

  void start(int numThreads);
  void stop();

  void run(const Task &f);

private:
  void runInThread();
  Task take();

  MutexLock mutex_;
  Condition cond_;
  string name_;
  boost::ptr_vector<muduo::Thread> threads_;
  std::deque<Task> queue_;//任务队列
  bool running_;
};

} // namespace muduo

#endif
