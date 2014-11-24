#include <exception>
#include <map>
#include <string>

#include "client_file_storage.h"

#include "test_mocks.h"

// TODO(dkorolev): Migrate to a header-only gtest.
#include "gtest/gtest.h"

TEST(ClientFileStorage, CompilesWithoutFlagsWithExplicitParamsProvided) {
  MockExporter exporter;
  MockTimeManager clock;
  MockFileManager fs;
  typedef ClientFileStorage<MockExporter, std::string, MockTimeManager, MockFileManager> CFS;
  CFS storage(exporter,
              clock,
              fs,
              CFS::Params().set_current_filename("meh").set_max_file_age(1000).set_max_file_size(1000));
  clock.ms = 100;
  storage.OnMessage("one\n", 0);
  clock.ms = 200;
  storage.OnMessage("two\n", 0);
  EXPECT_EQ(1, fs.NumberOfFiles());
  EXPECT_EQ("one\ntwo\n", fs.FileContents("meh"));
}

// TODO(dkorolev): /usr/src/gtest/libgtest_main.a 1) does not parse flags, and 2) is not header_only.
int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
