#include "chargen.h"

#include "EventLoop.h"
#include "Logging.h"

#include <unistd.h>

using namespace muduo;
using namespace muduo::net;

int main() {
  LOG_INFO << "pid = " << getpid();
  EventLoop loop;
  InetAddress listenAddr(2019);
  ChargenServer server(&loop, listenAddr, true);
  server.start();
  loop.loop();
}
