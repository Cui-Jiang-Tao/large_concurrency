#include "CountDownLatch.h"
#include "CurrentThread.h"
#include "ThreadPool.h"

#include <boost/bind.hpp>
#include <stdio.h>

void print() { printf("tid=%d\n", muduo::CurrentThread::tid()); }

void printString(const std::string &str) {
  printf("tid=%d, str=%s\n", muduo::CurrentThread::tid(), str.c_str());
}

int main() {
  muduo::ThreadPool pool("MainThreadPool");
  pool.start(5);

  pool.run(print);
  pool.run(print);

  for (int i = 0; i < 100; ++i) {
    char buf[32];
    snprintf(buf, sizeof buf, "task %d", i);
    pool.run(boost::bind(printString, std::string(buf)));
  }

  //这是线程池最后一个任务
  muduo::CountDownLatch latch(1);
  pool.run(boost::bind(&muduo::CountDownLatch::countDown, &latch));
 
 //可以保证执行完在最后一个任务前，main线程阻塞在这里
  latch.wait(); //非必要
  
  pool.stop();

  return 0;
}