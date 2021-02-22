
#include "test-common.h"

#include <type_traits>

struct Footprinter {};

struct MyTest {

  template <
    typename SerializerT,
    typename enabled_ = std::enable_if_t<
      std::is_same<SerializerT, Footprinter>::value
    >
  >
  void serialize(SerializerT& s) {
    s | z;
  }

  double z = 0.;
};

struct MyTest2 {

  template <typename SerializerT>
  void serialize(SerializerT& s) {
    s | x
      | y;
  }

  int x = 0;
  float y;
};

int main() {
  return testClass<MyTest2>("test-enable-if");
}
