// TODO(dkorolev): Make sure this test passes with both MockFileManager and PosixFileManager.
// The latter would require the Makefile `test` target to wipe out the tmp dir, also add it to .gitignore.

// TODO(dkorolev): How do we handle filesystem exceptions?

#include <exception>
#include <map>
#include <string>

#include "client_file_storage.h"

// TODO(dkorolev): Migrate to a header-only gtest.
#include "gtest/gtest.h"

struct MockTimeManager final {
 public:
  typedef uint64_t T_MS;
  MockTimeManager(T_MS ms = 0) : ms(ms) {
  }
  T_MS wall_time_ms() const {
    return ms;
  }
  T_MS ms;
};

struct MockFileManager final {
  struct FileNotFoundException : std::exception {};
  struct FileAlreadyExistsException : std::exception {};

  size_t NumberOfFiles() const {
    return files.size();
  }

  void CreateFile(const std::string& filename) {
    if (files.find(filename) != files.end()) {
      throw FileAlreadyExistsException();
    }
    files[filename] = "";
  }

  // TODO(dkorolev): Change from file names to file descriptors.
  void AppendToFile(const std::string& filename, const std::string& message) {
    if (files.find(filename) == files.end()) {
      throw FileNotFoundException();
    }
    files[filename].append(message);
  }

  void RenameFile(const std::string& from, const std::string& to) {
    if (files.find(from) == files.end()) {
      throw FileNotFoundException();
    }
    if (files.find(to) != files.end()) {
      throw FileAlreadyExistsException();
    }
    files[to] = files[from];
    files.erase(from);
  }

  const std::string& FileContents(const std::string& filename) const {
    const auto cit = files.find(filename);
    if (cit != files.end()) {
      return cit->second;
    } else {
      throw FileNotFoundException();
    }
  }

  std::map<std::string, std::string> files;
};

template <typename MS>
struct GenericMockExporter final {
  typedef MS T_MS;
  void OnFileCommitted(const std::string& filename,
                       const uint64_t length,
                       const T_MS first_ms,
                       const T_MS last_ms) {
  }
};
typedef struct GenericMockExporter<typename MockTimeManager::T_MS> MockExporter;

TEST(ClientFileStorageTest, KeepsSameFile) {
  FLAGS_current_filename = "KeepsSameFile";  // TODO(dkorolev): Timestamp the filename.
  FLAGS_max_file_age_ms = 1000;
  FLAGS_max_file_size = 1000;
  MockExporter exporter;
  MockTimeManager clock;
  MockFileManager fs;
  ClientFileStorage<MockExporter, std::string, MockTimeManager, MockFileManager> storage(exporter, clock, fs);
  clock.ms = 100;
  storage.OnMessage("foo one\n", 0);
  clock.ms = 200;
  storage.OnMessage("foo two\n", 0);
  EXPECT_EQ(1, fs.NumberOfFiles());
  EXPECT_EQ("foo one\nfoo two\n", fs.FileContents("KeepsSameFile"));
}

TEST(ClientFileStorageTest, RenamedFileBecauseOfSize) {
  FLAGS_current_filename = "RenamedFileBecauseOfSize";      // TODO(dkorolev): Timestamp the filename.
  FLAGS_committed_filename = "CommittedFileBecauseOfSize";  // TODO(dkorolev): Timestamp the filename.
  FLAGS_max_file_age_ms = 1000;
  FLAGS_max_file_size = 20;
  MockExporter exporter;
  MockTimeManager clock(0);
  MockFileManager fs;
  ClientFileStorage<MockExporter, std::string, MockTimeManager, MockFileManager> storage(exporter, clock, fs);
  clock.ms = 100;
  storage.OnMessage("bar one\n", 0);
  clock.ms = 200;
  storage.OnMessage("bar two\n", 0);
  clock.ms = 300;
  storage.OnMessage("bar three\n", 0);
  EXPECT_EQ(2, fs.NumberOfFiles());
  EXPECT_EQ("bar one\nbar two\n", fs.FileContents("CommittedFileBecauseOfSize"));
  EXPECT_EQ("bar three\n", fs.FileContents("RenamedFileBecauseOfSize"));
}

TEST(ClientFileStorageTest, RenamedFileBecauseOfAge) {
  FLAGS_current_filename = "RenamedFileBecauseOfAge";      // TODO(dkorolev): Timestamp the filename.
  FLAGS_committed_filename = "CommittedFileBecauseOfAge";  // TODO(dkorolev): Timestamp the filename.
  FLAGS_max_file_age_ms = 150;
  FLAGS_max_file_size = 1000;
  MockExporter exporter;
  MockTimeManager clock(0);
  MockFileManager fs;
  ClientFileStorage<MockExporter, std::string, MockTimeManager, MockFileManager> storage(exporter, clock, fs);
  clock.ms = 100;
  storage.OnMessage("baz one\n", 0);
  clock.ms = 200;
  storage.OnMessage("baz two\n", 0);
  clock.ms = 300;
  storage.OnMessage("baz three\n", 0);
  EXPECT_EQ(2, fs.NumberOfFiles());
  EXPECT_EQ("baz one\nbaz two\n", fs.FileContents("CommittedFileBecauseOfAge"));
  EXPECT_EQ("baz three\n", fs.FileContents("RenamedFileBecauseOfAge"));
}

// TODO(dkorolev): Remove files as they are sent, EXPECT contents of already removed files.
// TODO(dkorolev): Scan the directory on startup.
// TODO(dkorolev): Timestamps in file names.
// TODO(dkorolev): Rename the current file right away if it's too old.
// TODO(dkorolev): Update the timestamp to an older one in case time goes backwards to avoid large files.

// TODO(dkorolev): /usr/src/gtest/libgtest_main.a 1) does not parse flags, and 2) is not header_only.
int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  if (!google::ParseCommandLineFlags(&argc, &argv, true)) {
    return -1;
  }
  return RUN_ALL_TESTS();
}
