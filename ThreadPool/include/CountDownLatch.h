// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#ifndef MUDUO_BASE_COUNTDOWNLATCH_H
#define MUDUO_BASE_COUNTDOWNLATCH_H

#include "Condition.h"
#include "Mutex.h"

#include <boost/noncopyable.hpp>

namespace muduo {

class CountDownLatch : boost::noncopyable {
public:
  explicit CountDownLatch(int count);

  void wait();

  void countDown();

  int getCount() const;

private:
  mutable MutexLock mutex_; //可以改变的MutexLock对象
  Condition condition_;
  int count_;//倒数的门栓的数量
};

} // namespace muduo
#endif // MUDUO_BASE_COUNTDOWNLATCH_H
