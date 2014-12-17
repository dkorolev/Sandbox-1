#include "client_file_storage.h"

#include "test_mocks.h"

#include "../Bricks/3party/gtest/gtest.h"
#include "../Bricks/3party/gtest/gtest-main.h"

/*

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

*/
