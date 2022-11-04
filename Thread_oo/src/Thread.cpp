#include "Thread.h"

//开启线程
void Thread::start() {
  pthread_create(&threadId_, NULL, ThreadRoutine, this);

  if (isDetach) {
    pthread_detach(threadId_);
  }
}

void Thread::join() { pthread_join(threadId_, NULL); }

void *Thread::ThreadRoutine(void *arg) {
  Thread *thread = static_cast<Thread *>(arg);
  thread->run();

  return nullptr;
}

void Thread::setDetach(bool flag) { isDetach = flag; }