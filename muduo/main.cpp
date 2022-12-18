#include "echo.h"

#include "EventLoop.h"
#include "Logging.h"

#include <unistd.h>

// using namespace muduo;
// using namespace muduo::net;

int main() {
  LOG_INFO << "pid = " << getpid();
  muduo::net::EventLoop loop;
  muduo::net::InetAddress listenAddr(2007);
  EchoServer server(&loop, listenAddr);
  server.start();
  loop.loop();
}