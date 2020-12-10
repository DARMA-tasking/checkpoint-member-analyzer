
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
    elm_.serialize(s);
  }

  Element elm_;
};

Directory<int> dir;
Directory<float> dir2;

int main() {
  int r1 = testClass<Directory<int>>("test-inner-class<int>");
  int r2 = testClass<Directory<float>>("test-inner-class<float>");
  return r1 + r2;
}

template <>
template <>
void Directory<int>::serialize<checkpoint::serializers::Sanitizer>(checkpoint::serializers::Sanitizer& s) {
  s.check(elm_, "Directory<int>::elm_");
}
template <>
template <>
void Directory<float>::serialize<checkpoint::serializers::Sanitizer>(checkpoint::serializers::Sanitizer& s) {
  s.check(elm_, "Directory<float>::elm_");
}
