#include <vector>
#include <iostream>

template<class T>
class raiiTMaker
{
public:
    raiiTMaker();
    ~raiiTMaker();

    T* create(T* obj);

private:
    std::vector<T*> ptrList;
};

template <class T> raiiTMaker<T>::raiiTMaker() {}

template <class T> raiiTMaker<T>::~raiiTMaker() {
  //完全析构
  while (!ptrList.empty()) {
    T *p = ptrList.back();
    ptrList.pop_back();
    delete p;
  }
}

template <class T> T *raiiTMaker<T>::create(T *obj) {
  if (obj == NULL) {
    std::cerr << "error: Ivalid Param" << std::endl;
    return NULL;
  }

  ptrList.push_back(obj);
  return obj;
}