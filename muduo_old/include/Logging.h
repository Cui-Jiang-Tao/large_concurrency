#ifndef MUDUO_BASE_LOGGING_H
#define MUDUO_BASE_LOGGING_H

#include "LogStream.h"
#include "Timestamp.h"

namespace muduo {

class Logger {
public:
  /* 日志级别 */
  enum LogLevel {
    TRACE,          /* 细粒度最高的信息 */
    DEBUG,          /* 对调试有帮助的事件信息 */
    INFO,           /* 粗粒度级别上强调程序的运行信息 */
    WARN,           /* 程序能正常运行，但是有潜在危险的信息 */
    ERROR,          /* 程序出错，但是不影响系统运行的信息 */
    FATAL,          /* 将导致程序停止运行的严重信息 */
    NUM_LOG_LEVELS, /* 日志级别个数 */
  };

  // compile time calculation of basename of source file
  /* 内部类，保存调用LOG_WARN<<之类的那条语句所在文件名 */
  class SourceFile {
  public:
    template <int N>
    inline SourceFile(const char (&arr)[N]) : data_(arr), size_(N - 1) {
      const char *slash = strrchr(data_, '/'); // builtin function
      if (slash) {
        data_ = slash + 1;
        size_ -= static_cast<int>(data_ - arr);
      }
    }

    explicit SourceFile(const char *filename) : data_(filename) {
      const char *slash = strrchr(filename, '/');
      if (slash) {
        data_ = slash + 1;
      }
      size_ = static_cast<int>(strlen(data_));
    }

    const char *data_;
    int size_;
  };

  Logger(SourceFile file, int line);
  Logger(SourceFile file, int line, LogLevel level);
  Logger(SourceFile file, int line, LogLevel level, const char *func);
  Logger(SourceFile file, int line, bool toAbort);
  ~Logger();

  /* 返回Impl的LogStream对象 */
  LogStream &stream() { return impl_.stream_; }

  /* 返回日志级别及设置日志级别 */
  static LogLevel logLevel();
  static void setLogLevel(LogLevel level);

  /* 输出函数，将日志信息输出 */
  typedef void (*OutputFunc)(const char *msg, int len);
  /* 刷新缓冲区 */
  typedef void (*FlushFunc)();
  static void setOutput(OutputFunc);
  static void setFlush(FlushFunc);

private:
  /*
   * Impl技法，数据和对象分离
   * 通常应该是在类定义中声明class Impl;
   * 然后创建Impl对象，Impl impl_;
   * 最后在.cpp文件中实现Logger::Impl的定义
   *
   * Impl对象存储的就是Logger所需要的所有数据
   */
  class Impl {
  public:
    typedef Logger::LogLevel LogLevel;
    Impl(LogLevel level, int old_errno, const SourceFile &file, int line);
    void formatTime();
    void finish();

    /* UTC时间，记录写入日志的时间 */
    Timestamp time_;
    // 将日志信息存在缓冲区中，使用LOG_WARN是会返回Logger().stream()，就是返回这个LogStream
    LogStream stream_;
    /* 日志级别，TRACE, DEBUG, WARN... */
    LogLevel level_;
    int line_;

    /* 保存调用LOG_WARN<<语句的源文件名 */
    SourceFile basename_;
  };

  Impl impl_;
};

extern Logger::LogLevel g_logLevel;

inline Logger::LogLevel Logger::logLevel() { return g_logLevel; }

/*
 * __FILE__:返回所在文件名
 * __LINE__:返回所在行数
 * __func__:返回所在函数名
 *
 * 这些都是无名对象，当使用LOG_* << "***"时，
 * 1.构造Logger类型的临时对象，返回LogStream类型变量
 * 2.调用LogStream重载的operator<<操作符，将数据写入到LogStream的Buffer中
 * 3.当前语句结束，Logger临时对象析构，调用Logger析构函数，将LogStream中的数据输出
 */
#define LOG_TRACE                                                              \
  if (muduo::Logger::logLevel() <= muduo::Logger::TRACE)                       \
  muduo::Logger(__FILE__, __LINE__, muduo::Logger::TRACE, __func__).stream()
#define LOG_DEBUG                                                              \
  if (muduo::Logger::logLevel() <= muduo::Logger::DEBUG)                       \
  muduo::Logger(__FILE__, __LINE__, muduo::Logger::DEBUG, __func__).stream()
#define LOG_INFO                                                               \
  if (muduo::Logger::logLevel() <= muduo::Logger::INFO)                        \
  muduo::Logger(__FILE__, __LINE__).stream()
#define LOG_WARN muduo::Logger(__FILE__, __LINE__, muduo::Logger::WARN).stream()
#define LOG_ERROR                                                              \
  muduo::Logger(__FILE__, __LINE__, muduo::Logger::ERROR).stream()
#define LOG_FATAL                                                              \
  muduo::Logger(__FILE__, __LINE__, muduo::Logger::FATAL).stream()
#define LOG_SYSERR muduo::Logger(__FILE__, __LINE__, false).stream()
#define LOG_SYSFATAL muduo::Logger(__FILE__, __LINE__, true).stream()

const char *strerror_tl(int savedErrno);

// Taken from glog/logging.h
//
// Check that the input is non NULL.  This very useful in constructor
// initializer lists.

#define CHECK_NOTNULL(val)                                                     \
  ::muduo::CheckNotNull(__FILE__, __LINE__, "'" #val "' Must be non NULL",     \
                        (val))

// A small helper for CHECK_NOTNULL().
template <typename T>
T *CheckNotNull(Logger::SourceFile file, int line, const char *names, T *ptr) {
  if (ptr == NULL) {
    Logger(file, line, Logger::FATAL).stream() << names;
  }
  return ptr;
}

} // namespace muduo

#endif // MUDUO_BASE_LOGGING_H
