
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

Directory<int> dir;
Directory<float> dir2;
