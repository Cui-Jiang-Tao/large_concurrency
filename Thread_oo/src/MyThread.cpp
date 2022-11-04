#include "MyThread.h"
#include <iostream>
#include <unistd.h>

void MyThread::run() {
  int cnt = count_;

  while (count_--) {
    std::cout << "Loop is " << cnt - count_ << std::endl;
    sleep(1);
  }
}