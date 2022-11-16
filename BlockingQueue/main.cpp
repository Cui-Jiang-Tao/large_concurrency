#include "BlockingQueue.h"
#include "CountDownLatch.h"
#include "Thread.h"
#include "Timestamp.h"

#include <boost/bind.hpp>
#include <boost/ptr_container/ptr_vector.hpp>
#include <map>
#include <stdio.h>
#include <string>

class Test {
public:
  Test(int numThreads) : latch_(numThreads), threads_(numThreads) {
    for (int i = 0; i < numThreads; ++i) {
      char name[32];
      snprintf(name, sizeof name, "work thread %d", i);
      threads_.push_back(new muduo::Thread(boost::bind(&Test::threadFunc, this),
                                           std::string(name)));
    }
    for_each(threads_.begin(), threads_.end(),
             boost::bind(&muduo::Thread::start, _1));
  }

  void run(int times) {
    printf("waiting for count down latch\n");

    /*值得注意的是，子线程的countDown方法可能在main线程调用run之后，最后一个子线程最后会notifyAll方法；
     *也就是说：必须等待所有子线程执行完threadFunc方法后，再执行main线程的run方法
     */
    latch_.wait();

    printf("all threads started\n");
    for (int i = 0; i < times; ++i) {
      char buf[32];
      snprintf(buf, sizeof buf, "hello %d", i);
      queue_.put(buf);
      printf("tid=%d, put data = %s, size = %zd\n", muduo::CurrentThread::tid(),
             buf, queue_.size());
    }
  }

  void joinAll() {
    for (size_t i = 0; i < threads_.size(); ++i) {
      queue_.put("stop");
    }

    for_each(threads_.begin(), threads_.end(),
             boost::bind(&muduo::Thread::join, _1));
  }

private:
  void threadFunc() {
    printf("tid=%d, %s started\n", muduo::CurrentThread::tid(),
           muduo::CurrentThread::name());

    latch_.countDown();
    bool running = true;
    while (running) {
      std::string d(queue_.take()); //这里会阻塞
      printf("tid=%d, get data = %s, size = %zd\n", muduo::CurrentThread::tid(),
             d.c_str(), queue_.size());
      running = (d != "stop");
    }

    printf("tid=%d, %s stopped\n", muduo::CurrentThread::tid(),
           muduo::CurrentThread::name());
  }

  muduo::BlockingQueue<std::string> queue_;
  muduo::CountDownLatch latch_;
  boost::ptr_vector<muduo::Thread> threads_;
};

void test() {
  printf("pid=%d, tid=%d\n", ::getpid(), muduo::CurrentThread::tid());
  Test t(5);
  t.run(100);
  t.joinAll();

  printf("number of created threads %d\n", muduo::Thread::numCreated());
}

class Bench {
public:
  Bench(int numThreads) : latch_(numThreads), threads_(numThreads) {
    for (int i = 0; i < numThreads; ++i) {
      char name[32];
      snprintf(name, sizeof name, "work thread %d", i);
      threads_.push_back(new muduo::Thread(
          boost::bind(&Bench::threadFunc, this), std::string(name)));
    }
    for_each(threads_.begin(), threads_.end(),
             boost::bind(&muduo::Thread::start, _1));
  }

  void run(int times) {
    printf("waiting for count down latch\n");
    latch_.wait();
    printf("all threads started\n");
    for (int i = 0; i < times; ++i) {
      muduo::Timestamp now(muduo::Timestamp::now());
      queue_.put(now);
      usleep(1000);
    }
  }

  void joinAll() {
    for (size_t i = 0; i < threads_.size(); ++i) {
      queue_.put(muduo::Timestamp::invalid());
    }

    for_each(threads_.begin(), threads_.end(),
             boost::bind(&muduo::Thread::join, _1));
  }

private:
  void threadFunc() {
    printf("tid=%d, %s started\n", muduo::CurrentThread::tid(),
           muduo::CurrentThread::name());

    std::map<int, int> delays;
    latch_.countDown();
    bool running = true;
    while (running) {
      muduo::Timestamp t(queue_.take());  //队列为空，会阻塞
      muduo::Timestamp now(muduo::Timestamp::now());
      if (t.valid()) {
        int delay = static_cast<int>(timeDifference(now, t) * 1000000);
        // printf("tid=%d, latency = %d us\n",
        //        muduo::CurrentThread::tid(), delay);
        ++delays[delay];
      }
      running = t.valid();
    }

    printf("tid=%d, %s stopped\n", muduo::CurrentThread::tid(),
           muduo::CurrentThread::name());
    for (std::map<int, int>::iterator it = delays.begin(); it != delays.end();
         ++it) {
      printf("tid = %d, delay = %d, count = %d\n", muduo::CurrentThread::tid(),
             it->first, it->second);
    }
  }

  muduo::BlockingQueue<muduo::Timestamp> queue_;
  muduo::CountDownLatch latch_;
  boost::ptr_vector<muduo::Thread> threads_;
};

void testBench(int argc, char *argv[]) {
  int threads = argc > 1 ? atoi(argv[1]) : 1;

  Bench t(10);
  t.run(10000);
  t.joinAll();
}

int main(int argc, char *argv[]) {
  // test();

  testBench(argc, argv);

  return 0;
}
