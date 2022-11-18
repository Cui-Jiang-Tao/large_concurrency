#include "CurrentThread.h"
#include "Singleton.h"
#include "Thread.h"

#include <boost/noncopyable.hpp>
#include <stdio.h>

class Test : boost::noncopyable {
public:
  // 1
  Test() {
    printf("tid=%d, constructing %p\n", muduo::CurrentThread::tid(), this);
  }

  // 4
  ~Test() {
    printf("tid=%d, destructing %p %s\n", muduo::CurrentThread::tid(), this,
           name_.c_str());
  }

  const std::string &name() const { return name_; }
  void setName(const std::string &n) { name_ = n; }

private:
  std::string name_;
};

// 2
void threadFunc() {
  printf("tid=%d, %p name=%s\n", muduo::CurrentThread::tid(),
         &muduo::Singleton<Test>::instance(),
         muduo::Singleton<Test>::instance().name().c_str());
  muduo::Singleton<Test>::instance().setName("only one, changed");
}

int main() {
  muduo::Singleton<Test>::instance().setName("only one");
  muduo::Thread t1(threadFunc);
  t1.start();
  t1.join();

  // 3
  printf("tid=%d, %p name=%s\n", muduo::CurrentThread::tid(),
         &muduo::Singleton<Test>::instance(),
         muduo::Singleton<Test>::instance().name().c_str());
}
