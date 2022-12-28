#include "discard.h"

#include "EventLoop.h"
#include "Logging.h"

#include <unistd.h>

using namespace muduo;
using namespace muduo::net;

int main() {
  LOG_INFO << "pid = " << getpid();
  EventLoop loop;
  InetAddress listenAddr(2009);
  DiscardServer server(&loop, listenAddr);
  server.start();
  loop.loop();
}
