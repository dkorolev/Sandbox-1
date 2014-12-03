#ifndef SANDBOX_CEREAL_MESSAGE_H
#define SANDBOX_CEREAL_MESSAGE_H

#include "../3party/cereal/include/archives/binary.hpp"
#include "../3party/cereal/include/archives/portable_binary.hpp"
#include "../3party/cereal/include/archives/json.hpp"

#include "../3party/cereal/include/types/string.hpp"
#include "../3party/cereal/include/types/polymorphic.hpp"

// Objects of derived types should be wrapped into a smart pointer to an instance of their base class
// for cereal to serialize them correcly with the type information encoded.
//
// Use this construct:
//
//   DerivedObject object;
//   cereal_archive(SerializeAsPolymorphic<Base>(object));

template <typename T>
struct PolymorphicSerializerNullDeleter {
  inline void operator()(T*) {
  }
};

template <typename T>
std::unique_ptr<T, PolymorphicSerializerNullDeleter<T>> SerializeAsPolymorphic(T& object) {
  return std::unique_ptr<T, PolymorphicSerializerNullDeleter<T>>(&object);
}

enum class NonStandardInt : int;

struct BaseClass {
  virtual std::string AsString() const = 0;
};

struct DerivedClassInt : BaseClass {
  NonStandardInt x;
  virtual std::string AsString() const {
    std::ostringstream os;
    os << "Int: " << int(x);
    return os.str();
  }

 private:
  friend class cereal::access;
  template <class A>
  void serialize(A& ar) {
    ar(CEREAL_NVP(x));
  }
};
CEREAL_REGISTER_TYPE(DerivedClassInt);

struct DerivedClassString : BaseClass {
  std::string s;
  virtual std::string AsString() const {
    std::ostringstream os;
    os << "String: " << s;
    return os.str();
  }

 private:
  friend class cereal::access;
  template <class A>
  void serialize(A& ar) {
    ar(CEREAL_NVP(s));
  }
};
CEREAL_REGISTER_TYPE(DerivedClassString);

#endif  // SANDBOX_CEREAL_MESSAGE_H
