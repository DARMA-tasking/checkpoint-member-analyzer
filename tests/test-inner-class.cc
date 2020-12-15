
#include "test-common.h"

#include <type_traits>

template <typename IndexT>
struct Directory {

  struct Element {
    Element() = default;

    template <typename SerializerT>
    void serialize(SerializerT& s) {
      s | idx_;
    }

    IndexT idx_;
  };

  template <typename SerializerT>
  void serialize(SerializerT& s) {
    s | elm_;
  }

  Element elm_;
};

struct MyDir {
  template <typename SerializerT>
  void serialize(SerializerT& s) {
    s | i_;
  }

  int i_;
};

Directory<int> dir;
Directory<float> dir2;

int main() {
  int r1 = testClass<Directory<int>>("test-inner-class Directory<int>");
  int r2 = testClass<Directory<float>>("test-inner-class Directory<float>");
  int r3 = testClass<Directory<int>::Element>("test-inner-class Directory<int>::Element");
  int r4 = testClass<Directory<float>::Element>("test-inner-class Directory<float>::Element");
  int r5 = testClass<MyDir>("test-inner-class MyDir");
  return r1 + r2 + r3 + r4 + r5;
}
