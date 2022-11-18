#include "CurrentThread.h"
#include "Singleton.h"
#include "Thread.h"
#include "ThreadLocal.h"

#include <boost/bind.hpp>
#include <boost/noncopyable.hpp>
#include <stdio.h>

class Test : boost::noncopyable {
public:
  Test() {
    printf("tid=%d, constructing %p\n", muduo::CurrentThread::tid(), this);
  }

  ~Test() {
    printf("tid=%d, destructing %p %s\n", muduo::CurrentThread::tid(), this,
           name_.c_str());
  }

  const std::string &name() const { return name_; }
  void setName(const std::string &n) { name_ = n; }

private:
  std::string name_;
};

//只构造一次，也就是说muduo::ThreadLocal对象只被构造了一次，而Test对象，有几个线程使用STL就存在几个构造
#define STL muduo::Singleton<muduo::ThreadLocal<Test>>::instance().value()

void print() {
  printf("tid=%d, %p name=%s\n", muduo::CurrentThread::tid(), &STL,
         STL.name().c_str());
}

void threadFunc(const char *changeTo) {
  print();
  STL.setName(changeTo);
  sleep(1);
  print();
}

int main() {
  STL.setName("main one");                              // main Test构造
  muduo::Thread t1(boost::bind(threadFunc, "thread1")); // Thread 1 Test构造
  muduo::Thread t2(boost::bind(threadFunc, "thread2")); // Thread 2 Test构造
  t1.start();
  t2.start();
  t1.join();
  print();
  t2.join();

  pthread_exit(0);
}