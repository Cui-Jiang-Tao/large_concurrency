#include "Logging.h"

#include "CurrentThread.h"
#include "StringPiece.h"
#include "Timestamp.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <sstream>

namespace muduo {

/*
class LoggerImpl
{
 public:
  typedef Logger::LogLevel LogLevel;
  LoggerImpl(LogLevel level, int old_errno, const char* file, int line);
  void finish();

  Timestamp time_;
  LogStream stream_;
  LogLevel level_;
  int line_;
  const char* fullname_;
  const char* basename_;
};
*/

__thread char t_errnobuf[512];
__thread char t_time[32];
__thread time_t t_lastSecond;

const char *strerror_tl(int savedErrno) {
  return strerror_r(savedErrno, t_errnobuf, sizeof t_errnobuf);
}

Logger::LogLevel initLogLevel() {
  return Logger::TRACE;
  /*
if (::getenv("MUDUO_LOG_TRACE"))
return Logger::TRACE;
else if (::getenv("MUDUO_LOG_DEBUG"))
return Logger::DEBUG;
else
return Logger::INFO;
  */
}

Logger::LogLevel g_logLevel = initLogLevel();

const char *LogLevelName[Logger::NUM_LOG_LEVELS] = {
    "TRACE ", "DEBUG ", "INFO  ", "WARN  ", "ERROR ", "FATAL ",
};

// helper class for known string length at compile time
class T {
public:
  T(const char *str, unsigned len) : str_(str), len_(len) {
    assert(strlen(str) == len_);
  }

  const char *str_;
  const unsigned len_;
};

inline LogStream &operator<<(LogStream &s, T v) {
  s.append(v.str_, v.len_);
  return s;
}

inline LogStream &operator<<(LogStream &s, const Logger::SourceFile &v) {
  s.append(v.data_, v.size_);
  return s;
}

/*
 * 默认输出和刷新位置，将信息打印到屏幕上
 * 值得注意的是：Logging类的析构不是FATAL级别的，不会主动刷写flash(没必要)，也就是说日志的打印不是实时的
 */
void defaultOutput(const char *msg, int len) {
  size_t n = fwrite(msg, 1, len, stdout);
  // FIXME check n
  (void)n;
}

void defaultFlush() { fflush(stdout); }

Logger::OutputFunc g_output = defaultOutput;
Logger::FlushFunc g_flush = defaultFlush;

} // namespace muduo

using namespace muduo;

/* Impl的构造函数，Impl主要负责日志的格式化 */
Logger::Impl::Impl(LogLevel level, int savedErrno, const SourceFile &file,
                   int line)
    : time_(Timestamp::now()), /* 当前时间 */
      stream_(),               /* LogStream流 */
      level_(level),           /* 日志级别 */
      line_(line),    /* 调用LOG_* << 所在行，由__LINE__获取 */
      basename_(file) /* 调用LOG_* << 所在文件名，由__FILE__获取 */
{
  /* 格式化当前时间，写入LogStream中 */
  formatTime();
  /* 缓存线程id到成员变量中，当获取时直接返回 */
  CurrentThread::tid();
  /* 将线程id和日志级别写入LogStream */
  stream_ << T(CurrentThread::tidString(), CurrentThread::tidStringLength());
  stream_ << T(LogLevelName[level], 6);
  /* 如果有错误，写入错误信息 */
  if (savedErrno != 0) {
    stream_ << strerror_tl(savedErrno) << " (errno=" << savedErrno << ") ";
  }
}

void Logger::Impl::formatTime() {
  int64_t microSecondsSinceEpoch = time_.microSecondsSinceEpoch();
  time_t seconds = static_cast<time_t>(microSecondsSinceEpoch / 1000000);
  int microseconds = static_cast<int>(microSecondsSinceEpoch % 1000000);
  if (seconds != t_lastSecond) {
    t_lastSecond = seconds;
    struct tm tm_time;
    ::gmtime_r(&seconds, &tm_time); // FIXME TimeZone::fromUtcTime

    int len =
        snprintf(t_time, sizeof(t_time), "%4d%02d%02d %02d:%02d:%02d",
                 tm_time.tm_year + 1900, tm_time.tm_mon + 1, tm_time.tm_mday,
                 tm_time.tm_hour, tm_time.tm_min, tm_time.tm_sec);
    assert(len == 17);
    (void)len;
  }
  Fmt us(".%06dZ ", microseconds);
  assert(us.length() == 9);
  stream_ << T(t_time, 17) << T(us.data(), 9);
}

void Logger::Impl::finish() {
  stream_ << " - " << basename_ << ':' << line_ << '\n';
}

Logger::Logger(SourceFile file, int line) : impl_(INFO, 0, file, line) {}

Logger::Logger(SourceFile file, int line, LogLevel level, const char *func)
    : impl_(level, 0, file, line) {
  impl_.stream_ << func << ' ';
}

Logger::Logger(SourceFile file, int line, LogLevel level)
    : impl_(level, 0, file, line) {}

Logger::Logger(SourceFile file, int line, bool toAbort)
    : impl_(toAbort ? FATAL : ERROR, errno, file, line) {}

/*
 * 对于临时对象，所在语句结束后就被析构了，所以对于日志信息的输出，肯定都交给Logger对象的析构函数处理了
 * Logger析构函数，将LogStream中的数据打印出来
 * 根据日志级别决定要不要立刻刷新
 */
Logger::~Logger() {
  /* 当前日志输出结束，添加后缀（所在文件名和行号） */
  impl_.finish();
  /* 从LogStream的Buffer中拿出日志信息 */
  const LogStream::Buffer &buf(stream().buffer());
  /* 将信息输出到屏幕上 */
  g_output(buf.data(), buf.length());
  // 如果当前日志级别是FATAL，表示是个终止程序的严重错误，将输出缓冲区的信息刷新到屏幕上，结束程序
  if (impl_.level_ == FATAL) {
    g_flush();
    abort();
  }
}

void Logger::setLogLevel(Logger::LogLevel level) { g_logLevel = level; }

void Logger::setOutput(OutputFunc out) { g_output = out; }

void Logger::setFlush(FlushFunc flush) { g_flush = flush; }
