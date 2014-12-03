#include <cassert>
#include <iostream>

#include "message.h"

// Tests one string serialization and de-serialization.
template <typename T_IN_ARCHIVE, typename T_OUT_ARCHIVE>
void SingleStringTest(bool dump = true, const std::string& test = "foo") {
  std::string serialized;
  {
    std::ostringstream os;
    DerivedClassString s;
    s.s = test;
    {
      T_OUT_ARCHIVE ar(os);
      ar(s);
    }
    serialized = os.str();
  }
  if (dump) {
    std::cout << "  Serialized:" << std::endl << serialized << std::endl;
  } else {
    std::cout << "  Serialized: " << serialized.length() << " bytes." << std::endl;
  }
  {
    std::istringstream is(serialized);
    T_IN_ARCHIVE ar(is);
    DerivedClassString s;
    ar(s);
    std::cout << "  Deserialized: " << s.s << std::endl;
  }
}

// Tests multiple strings serialization and de-serialization.
template <typename T_IN_ARCHIVE, typename T_OUT_ARCHIVE>
void MultipleStringsTest(bool dump = true, const std::string& prefix = "bar ") {
  std::string serialized;
  {
    std::ostringstream os;
    {
      T_OUT_ARCHIVE ar(os);
      DerivedClassString s;
      for (int i = 1; i <= 3; ++i) {
        s.s = prefix + std::to_string(i);
        ar(s);
      }
    }
    serialized = os.str();
  }
  if (dump) {
    std::cout << "  Serialized:" << std::endl << serialized << std::endl;
  } else {
    std::cout << "  Serialized: " << serialized.length() << " bytes." << std::endl;
  }
  {
    std::istringstream is(serialized);
    T_IN_ARCHIVE ar(is);
    while (true) {
      DerivedClassString s;
      try {
        ar(s);
      } catch (cereal::Exception&) {
        break;
      }
      std::cout << "  Deserialized: " << s.s << std::endl;
    }
    std::cout << "  Done." << std::endl;
  }
}

// Tests multiple polymorphic objects.
template <typename T_IN_ARCHIVE, typename T_OUT_ARCHIVE>
void MultiplePolymorphicsTest(bool dump = true, const std::string& prefix = "baz ") {
  std::string serialized;
  {
    std::ostringstream os;
    {
      T_OUT_ARCHIVE ar(os);
      DerivedClassInt x;
      DerivedClassString s;
      for (int i = 1; i <= 3; ++i) {
        x.x = static_cast<NonStandardInt>(i);
        ar(SerializeAsPolymorphic<BaseClass>(x));
        s.s = prefix + std::to_string(i);
        ar(SerializeAsPolymorphic<BaseClass>(s));
      }
    }
    serialized = os.str();
  }
  if (dump) {
    std::cout << "  Serialized:" << std::endl << serialized << std::endl;
  } else {
    std::cout << "  Serialized: " << serialized.length() << " bytes." << std::endl;
  }
  {
    std::istringstream is(serialized);
    T_IN_ARCHIVE ar(is);
    while (true) {
      std::unique_ptr<BaseClass> x;
      try {
        ar(x);
      } catch (cereal::Exception&) {
        break;
      }
      std::cout << "  Deserialized: " << x->AsString() << std::endl;
    }
    std::cout << "  Done." << std::endl;
  }
}

template <typename T_IN_ARCHIVE, typename T_OUT_ARCHIVE>
void RunTests(bool dump = true) {
  SingleStringTest<T_IN_ARCHIVE, T_OUT_ARCHIVE>(dump);
  MultipleStringsTest<T_IN_ARCHIVE, T_OUT_ARCHIVE>(dump);
  MultiplePolymorphicsTest<T_IN_ARCHIVE, T_OUT_ARCHIVE>(dump);
}

int main() {
  std::cout << "JSON:" << std::endl;
  RunTests<cereal::JSONInputArchive, cereal::JSONOutputArchive>(false);
  std::cout << "Binary:" << std::endl;
  RunTests<cereal::BinaryInputArchive, cereal::BinaryOutputArchive>(false);
  std::cout << "PortableBinary:" << std::endl;
  RunTests<cereal::PortableBinaryInputArchive, cereal::PortableBinaryOutputArchive>(false);
  std::cout << "Done." << std::endl;
}
