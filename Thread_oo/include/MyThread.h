#include "Thread.h"

class MyThread : public Thread {
public:
  MyThread(int count) : count_(count){};
  ~MyThread() = default;

private:
  void run();

private:
  int count_;
};


