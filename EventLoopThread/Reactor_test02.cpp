#include "EventLoop.h"

#include <stdio.h>

using namespace muduo;
using namespace muduo::net;

//现在两个线程共享，那么loop的时候就会报错
EventLoop *g_loop;

void threadFunc() { g_loop->loop(); }

int main(void) {

  EventLoop loop;
  g_loop = &loop;

  Thread t(threadFunc);
  t.start();
  t.join();

  return 0;
}
