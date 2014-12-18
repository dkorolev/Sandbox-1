#include <atomic>

#include "fsq.h"

#include "../Bricks/file/file.h"

#include "../Bricks/3party/gtest/gtest.h"
#include "../Bricks/3party/gtest/gtest-main.h"

using std::string;
using std::atomic_bool;

const char* const kTestDir = "build/";

struct MockProcessor {
  MockProcessor() : updated(false) {
  }
  template <typename T_TIMESTAMP, typename T_TIME_SPAN>
  fsq::FileProcessingResult OnFileReady(const std::string& file_name,
                                        const std::string& file_base_name,
                                        uint64_t /*size*/,
                                        T_TIMESTAMP /*created*/,
                                        T_TIME_SPAN /*age*/,
                                        T_TIMESTAMP /*now*/) {
    filename = file_base_name;
    contents = bricks::ReadFileAsString(file_name);
    updated = true;
    return fsq::FileProcessingResult::Success;
  }

  atomic_bool updated;
  string filename = "";
  string contents = "";
};

struct MockTime {
  typedef uint64_t T_TIMESTAMP;
  typedef int64_t T_TIME_SPAN;
  uint64_t now = 0;
  T_TIMESTAMP Now() const {
    return now;
  }
};

struct MockConfig : fsq::Config<MockProcessor> {
  typedef MockTime T_TIME_MANAGER;
  typedef fsq::strategy::AppendToFileWithSeparator T_FILE_APPEND_POLICY;
};

typedef fsq::FSQ<MockConfig> FSQ;

TEST(FileSystemQueueTest, SimpleSmokeTest) {
  // A simple way to create and initialize FileSystemQueue ("FSQ").
  MockProcessor processor;
  MockTime time_manager;
  bricks::FileSystem file_system;
  FSQ fsq(processor, kTestDir, time_manager, file_system);
  fsq.SetSeparator("\n");

  // Confirm the queue is empty.
  EXPECT_EQ(0ull, fsq.GetQueueStatus().appended_file_size);
  EXPECT_EQ(0ll, fsq.GetQueueStatus().appended_file_age);
  EXPECT_EQ(0u, fsq.GetQueueStatus().number_of_queued_files);
  EXPECT_EQ(0ul, fsq.GetQueueStatus().total_queued_files_size);
  EXPECT_EQ(0ll, fsq.GetQueueStatus().oldest_queued_file_age);

  // Add a few entries.
  time_manager.now = 1001;
  fsq.PushMessage("foo");
  time_manager.now = 1002;
  fsq.PushMessage("bar");
  time_manager.now = 1003;
  fsq.PushMessage("baz");
  time_manager.now = 1010;

  // Confirm the queue is empty.
  EXPECT_EQ(12ull, fsq.GetQueueStatus().appended_file_size);  // Three messages of (3 + '\n') bytes each.
  EXPECT_EQ(2ll, fsq.GetQueueStatus().appended_file_age);     // 1003 - 1001.
  EXPECT_EQ(0u, fsq.GetQueueStatus().number_of_queued_files);
  EXPECT_EQ(0ul, fsq.GetQueueStatus().total_queued_files_size);
  EXPECT_EQ(0ll, fsq.GetQueueStatus().oldest_queued_file_age);

  // Force entries processing to have three freshly added ones reach our MockProcessor.
  fsq.ForceResumeProcessing();
  while (!processor.updated) {
    ;  // Spin lock.
  }

  EXPECT_EQ("finalized-00000000000000001001.bin", processor.filename);
  EXPECT_EQ("foo\nbar\nbaz\n", processor.contents);
}

/*

TEST(FileSystemQueueTest, KeepsSameFile);
TEST(FileSystemQueueTest, RenamedFileBecauseOfSize);
TEST(FileSystemQueueTest, RenamedFileBecauseOfAge);
TEST(FileSystemQueueTest, Scan the directory on startup.);
TEST(FileSystemQueueTest, Resume already existing append-only file.);
TEST(FileSystemQueueTest, Correctly extract timestamps from all the files, including the temporary one.);
TEST(FileSystemQueueTest, Rename the current file right away if it should be renamed, before any work.);
TEST(FileSystemQueueTest, Time skew.);
TEST(FileSystemQueueTest, Custom finalize strategy.);
TEST(FileSystemQueueTest, Custom append strategy.);

*/
