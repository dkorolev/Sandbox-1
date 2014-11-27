#include <algorithm>
#include <exception>
#include <vector>
#include <string>

#include "linux_file_manager.h"

// TODO(dkorolev): Migrate to a header-only gtest.
#include "gtest/gtest.h"

TEST(LinuxFileSystem, FileOperations) {
  LinuxFileManager fs;

  {
    LinuxFileManager::Handle f1 = fs.CreateAppendOnlyFile("foo");
    f1.Append("test\n");
    f1.Append("passed\n");
  }

  fs.CreateAppendOnlyFile("bar").Append("another ").Append("test ").Append("passed");

  EXPECT_EQ("test\npassed\n", fs.ReadFile("foo"));
  EXPECT_EQ("another test passed", fs.ReadFile("bar"));

  ASSERT_THROW(fs.ReadFile("baz"), FileManager::CanNotReadFileException);

  fs.RemoveFile("foo");
  ASSERT_THROW(fs.RemoveFile("foo"), FileManager::CanNotRemoveFileException);

  fs.RenameFile("bar", "baz");
  ASSERT_THROW(fs.RenameFile("bar", "meh"), FileManager::CanNotRenameFileException);
  ASSERT_THROW(fs.ReadFile("bar"), FileManager::CanNotReadFileException);
  ASSERT_THROW(fs.RemoveFile("bar"), FileManager::CanNotRemoveFileException);

  EXPECT_EQ("another test passed", fs.ReadFile("baz"));
  fs.RemoveFile("baz");
}

TEST(LinuxFileSystem, BinaryDataFileOperations) {
  LinuxFileManager fs;

  {
    std::string binary_string(7, '\0');
    binary_string[0] = 'f';
    binary_string[1] = 'o';
    binary_string[2] = 'o';
    binary_string[4] = 'b';
    binary_string[5] = 'a';
    binary_string[6] = 'r';
    fs.CreateAppendOnlyFile("1.bin").Append(binary_string);
  }

  fs.CreateAppendOnlyFile("2.bin").Append(std::string(100, '\0'));

  fs.CreateAppendOnlyFile("3.bin").Append("\n");
  fs.CreateAppendOnlyFile("4.bin").Append("\r\n");

  const std::string result = fs.ReadFile("1.bin");
  EXPECT_EQ(7, result.length());
  EXPECT_EQ("foo", std::string(result.c_str()));
  EXPECT_EQ("bar", std::string(result.c_str() + 4));

  EXPECT_EQ(100, fs.ReadFile("2.bin").length());
  EXPECT_EQ("\n", fs.ReadFile("3.bin"));
  EXPECT_EQ("\r\n", fs.ReadFile("4.bin"));

  fs.RemoveFile("1.bin");
  fs.RemoveFile("2.bin");
  fs.RemoveFile("3.bin");
  fs.RemoveFile("4.bin");
}

TEST(LinuxFileSystem, DirectoryOperations) {
  LinuxFileManager fs;

  fs.CreateAppendOnlyFile("test-001").Append("this\n");
  fs.CreateAppendOnlyFile("test-002").Append("too\n");
  fs.CreateAppendOnlyFile("test-007").Append("shall\n");
  fs.CreateAppendOnlyFile("test-042").Append("pass\n");

  fs.CreateAppendOnlyFile("this").Append("blah");
  fs.CreateAppendOnlyFile("will").Append("blah");
  fs.CreateAppendOnlyFile("not").Append("blah");
  fs.CreateAppendOnlyFile("match").Append("blah");

  LinuxFileManager::DirectoryIterator dit = fs.ScanDirectory("test-???");
  std::vector<std::string> files;
  std::string current;
  while (current = dit.Next(), !current.empty()) {
    files.push_back(current);
  }

  std::sort(files.begin(), files.end());
  ASSERT_EQ(4, files.size());
  EXPECT_EQ("test-001", files[0]);
  EXPECT_EQ("test-002", files[1]);
  EXPECT_EQ("test-007", files[2]);
  EXPECT_EQ("test-042", files[3]);

  fs.RemoveFile("test-001");
  fs.RemoveFile("test-002");
  fs.RemoveFile("test-007");
  fs.RemoveFile("test-042");
  fs.RemoveFile("this");
  fs.RemoveFile("will");
  fs.RemoveFile("not");
  fs.RemoveFile("match");
}

// TODO(dkorolev): /usr/src/gtest/libgtest_main.a is not header_only, fix it.
int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
