// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include "Thread.h"
#include "CurrentThread.h"
#include "Exception.h"
#include "Timestamp.h"
//#include <muduo/base/Logging.h>

#include <boost/static_assert.hpp>
#include <boost/type_traits/is_same.hpp>

#include <errno.h>
#include <stdio.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>

namespace muduo {
namespace CurrentThread {
/* __thread修饰的变量是线程局部存储的。
 * 线程真实pid（tid）的缓存，
 * 减少::syscall(SYS_gettid)系统调用的次数，提高获取tid的效率
 */
__thread int t_cachedTid = 0;
__thread char t_tidString[32]; // 这是tid的字符串表示形式
__thread int t_tidStringLength = 6;
__thread const char *t_threadName = "unknown";

//断言pid_t的类型为int
const bool sameType = boost::is_same<int, pid_t>::value;
BOOST_STATIC_ASSERT(sameType);

void CurrentThread::sleepUsec(int64_t usec) {
  struct timespec ts = {0, 0};
  ts.tv_sec = static_cast<time_t>(usec / Timestamp::kMicroSecondsPerSecond);
  ts.tv_nsec =
      static_cast<long>(usec % Timestamp::kMicroSecondsPerSecond * 1000);
  ::nanosleep(&ts, NULL);
}
} // namespace CurrentThread

namespace detail {

//使用系统调用来获取内核线程ID
pid_t gettid() { return static_cast<pid_t>(::syscall(SYS_gettid)); }

//子线程调用的函数
void afterFork() {
  muduo::CurrentThread::t_cachedTid = 0;
  muduo::CurrentThread::t_threadName = "main";
  CurrentThread::tid();
  // no need to call pthread_atfork(NULL, NULL, &afterFork);
}

//调用fork()函数才有效果，进程调度的问题暂不考虑。
class ThreadNameInitializer {
public:
  ThreadNameInitializer() {
    muduo::CurrentThread::t_threadName = "main";
    CurrentThread::tid();
    pthread_atfork(NULL, NULL, &afterFork);
  }
};

ThreadNameInitializer init;
} // namespace detail
} // namespace muduo

using namespace muduo;

//设值t_cachedTid和字符串表示形式
void CurrentThread::cacheTid() {
  if (t_cachedTid == 0) {
    t_cachedTid = detail::gettid();
    t_tidStringLength =
        snprintf(t_tidString, sizeof t_tidString, "%5d ", t_cachedTid);
  }
}

//确定调用线程和本线程是同一线程
bool CurrentThread::isMainThread() { return tid() == ::getpid(); }

//创建线程的数量; 初始化，调用无参构造函数
AtomicInt32 Thread::numCreated_;

Thread::Thread(const ThreadFunc &func, const string &name)
    : started_(false), pthreadId_(0), tid_(0), func_(func), name_(name) {
  numCreated_.increment();
}

Thread::~Thread() {
  // no join
}

void Thread::start() {
  assert(!started_);
  started_ = true;
  errno = pthread_create(&pthreadId_, NULL, &startThread, this);

  if (errno != 0) {
    // LOG_SYSFATAL << "Failed in pthread_create";
  }
}

int Thread::join() {
  assert(started_);
  return pthread_join(pthreadId_, NULL);
}

void *Thread::startThread(void *obj) {
  Thread *thread = static_cast<Thread *>(obj);
  thread->runInThread();
  return NULL;
}

void Thread::runInThread() {
  tid_ = CurrentThread::tid();
  muduo::CurrentThread::t_threadName = name_.c_str();
  try {
    func_();
    muduo::CurrentThread::t_threadName = "finished";
  } catch (const Exception &ex) {
    muduo::CurrentThread::t_threadName = "crashed";
    fprintf(stderr, "exception caught in Thread %s\n", name_.c_str());
    fprintf(stderr, "reason: %s\n", ex.what());
    fprintf(stderr, "stack trace: %s\n", ex.stackTrace());
    abort();
  } catch (const std::exception &ex) {
    muduo::CurrentThread::t_threadName = "crashed";
    fprintf(stderr, "exception caught in Thread %s\n", name_.c_str());
    fprintf(stderr, "reason: %s\n", ex.what());
    abort();
  } catch (...) {
    muduo::CurrentThread::t_threadName = "crashed";
    fprintf(stderr, "unknown exception caught in Thread %s\n", name_.c_str());
    throw; // rethrow
  }
}
