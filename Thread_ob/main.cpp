#include "Thread.h"

#include "unistd.h"
#include <iostream>

class Foo {
public:
  Foo(int count) : count_(count) {}

  void MemberFun() {
    int cnt = count_;
    while (count_--) {
      std::cout << "this is a test " << cnt - count_ << std::endl;
      sleep(1);
    }
  }

  void MemberFun2(int count) {
    int cnt = count;
    while (count--) {
      std::cout << "this is a test2 " << cnt - count << std::endl;
      sleep(1);
    }
  }

  int count_;
};

int main(int argc, char **argv) {
  Foo foo(10);
  Thread thread1(std::bind(&Foo::MemberFun, &foo));
  Thread thread2(std::bind(&Foo::MemberFun2, &foo, 5));

  thread1.start();
  thread2.start();
  thread1.join();
  thread2.join();

  return 0;
}
