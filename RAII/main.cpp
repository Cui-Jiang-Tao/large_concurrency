#include "raiiTMaker.h"
#include <iostream>

class Traii {
public:
    Traii(){}
    Traii(int){}
    int getRaii() { return val; }
    void setRaii(int rhs) { val = rhs; }

    //析构
    ~Traii()
    {
        std::cout << "destroyed" << std::endl;
    }
private:
    int val;
};

void doWork()
{
    raiiTMaker<Traii> maker;
    for (int i = 0; i < 5; i++)
    {
        Traii *t = static_cast<Traii*>(maker.create(new Traii()));
    }
    //无析构
    //自动内存管理
}

int main() {
  doWork();

  std::cout << "over" << std::endl;

  return 0;
}