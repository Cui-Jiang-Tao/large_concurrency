#ifndef _THREAD_H_
#define _THREAD_H_

#include <pthread.h>
#include <functional>

class Thread {
  typedef std::function<void ()> ThreadFunc;

public:
  Thread(const ThreadFunc& func) : func_(func), isDetach(false) {};
  virtual ~Thread() = default;

  void start();
  void join();
  void setDetach(bool);

private:
  static void *ThreadRoutine(void *arg);
  void run();

  ThreadFunc func_;
  pthread_t threadId_;
  bool isDetach;
};

#endif