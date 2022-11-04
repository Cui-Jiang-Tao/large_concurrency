#ifndef _THREAD_H_
#define _THREAD_H_

#include <pthread.h>

class Thread {
public:
  Thread() : isDetach(false) {};
  virtual ~Thread() = default;

  void start();
  void join();
  void setDetach(bool);

private:
  static void *ThreadRoutine(void *arg);
  virtual void run() = 0;
  pthread_t threadId_;
  bool isDetach;
};

#endif