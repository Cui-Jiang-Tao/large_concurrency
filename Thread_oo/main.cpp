#include "MyThread.h"

int main(int argc, char **argv) {
  MyThread thread(10);
  thread.start();
  thread.join();

  return 0;
}
