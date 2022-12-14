// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#ifndef MUDUO_BASE_SINGLETON_H
#define MUDUO_BASE_SINGLETON_H

#include <boost/noncopyable.hpp>
#include <pthread.h>
#include <stdlib.h> // 包含atexit方法

namespace muduo {

template <typename T> class Singleton : boost::noncopyable {
public:
  static T &instance() {
    pthread_once(&ponce_, &Singleton::init);
    return *value_;
  }

private:
  //没有实例
  Singleton();
  ~Singleton();

  //只执行一次
  static void init() {
    value_ = new T();
    ::atexit(destroy); //在正常程序退出时调用
  }

  //也只执行一次
  static void destroy() {
    typedef char T_must_be_complete_type[sizeof(T) == 0 ? -1 : 1];
    delete value_;
  }

private:
  static pthread_once_t ponce_;
  static T *value_;
};

//初始化
template <typename T> pthread_once_t Singleton<T>::ponce_ = PTHREAD_ONCE_INIT;

template <typename T> T *Singleton<T>::value_ = NULL;

} // namespace muduo
#endif
