#include "TimeServer.h"

#include "EventLoop.h"
#include "InetAddress.h"
#include "Logging.h"

#include <unistd.h>

using namespace muduo;
using namespace muduo::net;

int main() {
  LOG_INFO << "pid = " << getpid();
  EventLoop loop;
  InetAddress listenAddr(2037);
  TimeServer server(&loop, listenAddr);
  server.start();
  loop.loop();
}
