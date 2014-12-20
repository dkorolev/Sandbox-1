#include <algorithm>
#include <exception>
#include <vector>
#include <string>

#include "posix_file_manager.h"

#include "../Bricks/3party/gtest/gtest.h"
#include "../Bricks/3party/gtest/gtest-main.h"

TEST(PosixFileSystem, FileOperations) {
  PosixFileManager fs;

  {
    PosixFileManager::Handle f1 = fs.CreateFile("foo");
    f1.Append("test\n");
    f1.Append("passed\n");
    ASSERT_THROW(fs.CreateFile("foo"), FileManager::FileAlreadyExistsException);
  }

  {
    EXPECT_EQ(12, fs.GetFileSize("foo"));
    PosixFileManager::Handle f1_append = fs.CreateOrAppendToFile("foo");
    f1_append.Append("indeed!\n");
  }

  fs.CreateOrAppendToFile("bar").Append("another ").Append("test ").Append("passed");

  EXPECT_EQ(20, fs.GetFileSize("foo"));
  EXPECT_EQ(19, fs.GetFileSize("bar"));
  ASSERT_THROW(fs.GetFileSize("baz"), FileManager::CanNotGetFileSizeException);

  EXPECT_EQ("test\npassed\nindeed!\n", fs.ReadFileToString("foo"));
  EXPECT_EQ("another test passed", fs.ReadFileToString("bar"));

  ASSERT_THROW(fs.ReadFileToString("baz"), FileManager::CanNotReadFileException);

  fs.RemoveFile("foo");
  ASSERT_THROW(fs.RemoveFile("foo"), FileManager::CanNotRemoveFileException);

  fs.RenameFile("bar", "baz");
  ASSERT_THROW(fs.RenameFile("bar", "meh"), FileManager::CanNotRenameFileException);
  ASSERT_THROW(fs.ReadFileToString("bar"), FileManager::CanNotReadFileException);
  ASSERT_THROW(fs.RemoveFile("bar"), FileManager::CanNotRemoveFileException);

  EXPECT_EQ("another test passed", fs.ReadFileToString("baz"));
  fs.RemoveFile("baz");
  ASSERT_THROW(fs.ReadFileToString("baz"), FileManager::CanNotReadFileException);
}

TEST(PosixFileSystem, BinaryDataFileOperations) {
  PosixFileManager fs;

  {
    std::string binary_string(7, '\0');
    binary_string[0] = 'f';
    binary_string[1] = 'o';
    binary_string[2] = 'o';
    binary_string[4] = 'b';
    binary_string[5] = 'a';
    binary_string[6] = 'r';
    fs.CreateFile("1.bin").Append(binary_string);
  }

  fs.CreateFile("2.bin").Append(std::string(100, '\0'));

  fs.CreateFile("3.bin").Append("\n");
  fs.CreateFile("4.bin").Append("\r\n");

  const std::string result = fs.ReadFileToString("1.bin");
  EXPECT_EQ(7, result.length());
  EXPECT_EQ("foo", std::string(result.c_str()));
  EXPECT_EQ("bar", std::string(result.c_str() + 4));

  EXPECT_EQ(100, fs.ReadFileToString("2.bin").length());
  EXPECT_EQ("\n", fs.ReadFileToString("3.bin"));
  EXPECT_EQ("\r\n", fs.ReadFileToString("4.bin"));

  fs.RemoveFile("1.bin");
  fs.RemoveFile("2.bin");
  fs.RemoveFile("3.bin");
  fs.RemoveFile("4.bin");
}

TEST(PosixFileSystem, DirectoryOperations) {
  PosixFileManager fs;

  fs.CreateFile("test-001").Append("this\n");
  fs.CreateFile("test-002").Append("too\n");
  fs.CreateFile("test-007").Append("shall\n");
  fs.CreateFile("test-042").Append("pass\n");

  fs.CreateFile("this").Append("blah");
  fs.CreateFile("will").Append("blah");
  fs.CreateFile("not").Append("blah");
  fs.CreateFile("match").Append("blah");

  PosixFileManager::DirectoryIterator dit = fs.ScanDirectory("test-???");
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

TEST(PosixFileSystem, Exceptions) {
  {
    std::unique_ptr<PosixFileManager> p;
    ASSERT_THROW(p.reset(new PosixFileManager("")), FileManager::NeedTrailingSlashInWorkingDirectoryException);
    ASSERT_THROW(p.reset(new PosixFileManager("/foo/bar")),
                 FileManager::NeedTrailingSlashInWorkingDirectoryException);
  }
  {
    PosixFileManager fs("/foo/bar/baz/does/not/exist/");
    ASSERT_THROW(fs.ScanDirectory(""), FileManager::CanNotScanDirectoryException);
  }
  {
    PosixFileManager fs;
    {
      PosixFileManager::Handle f1 = fs.CreateFile("foo");
      f1.Append("test\n");
      PosixFileManager::Handle f2 = std::move(f1);
      ASSERT_THROW(f1.Append("failed\n"), FileManager::NullFileHandleException);
    }
    fs.RemoveFile("foo");
  }
  {
    PosixFileManager fs;
    PosixFileManager::DirectoryIterator dit1 = fs.ScanDirectory("meh");
    ASSERT_EQ("", dit1.Next());
    PosixFileManager::DirectoryIterator dit2 = std::move(dit1);
    ASSERT_THROW(dit1.Next(), FileManager::NullDirectoryIteratorException);
  }
}
