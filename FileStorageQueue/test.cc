#include <atomic>

#include "fsq.h"

#include "../Bricks/file/file.h"

#include "../Bricks/3party/gtest/gtest.h"
#include "../Bricks/3party/gtest/gtest-main.h"

using std::string;
using std::atomic_size_t;

const char* const kTestDir = "build/";

// TestOutputFilesProcessor collects the output of finalized files.
struct TestOutputFilesProcessor {
  TestOutputFilesProcessor() : finalized_count(0) {
  }
  fsq::FileProcessingResult OnFileReady(const fsq::FileInfo<uint64_t>& file_info, uint64_t now) {
    filename = file_info.name;
    contents = bricks::ReadFileAsString(file_info.full_path_name);
    timestamp = now;
    finalized_count = true;
    return fsq::FileProcessingResult::Success;
  }

  atomic_size_t finalized_count;
  string filename = "";
  string contents = "";
  uint64_t timestamp = 0;
};

struct MockTime {
  typedef uint64_t T_TIMESTAMP;
  typedef int64_t T_TIME_SPAN;
  uint64_t now = 0;
  T_TIMESTAMP Now() const {
    return now;
  }
};

struct MockConfig : fsq::Config<TestOutputFilesProcessor> {
  // Mock time.
  typedef MockTime T_TIME_MANAGER;
  // Append using newlines.
  typedef fsq::strategy::AppendToFileWithSeparator T_FILE_APPEND_STRATEGY;
  // No backlog: 20 bytes 10 seconds old files max, with backlog: 100 bytes 60 seconds old files max.
  typedef fsq::strategy::SimpleFinalizationStrategy<MockTime::T_TIMESTAMP,
                                                    MockTime::T_TIME_SPAN,
                                                    20,
                                                    MockTime::T_TIME_SPAN(10 * 1000),
                                                    100,
                                                    MockTime::T_TIME_SPAN(60 * 1000)> T_FINALIZE_STRATEGY;
  // Purge after 1000 bytes total or after 3 files.
  typedef fsq::strategy::SimplePurgeStrategy<1000, 3> T_PURGE_STRATEGY;

  // Non-static initialization.
  template <typename T_FSQ_INSTANCE>
  static void Initialize(T_FSQ_INSTANCE& instance) {
    instance.RemoveAllFSQFiles();
    instance.SetSeparator("\n");
  }
};

typedef fsq::FSQ<MockConfig> FSQ;

// Observe messages being processed as they exceed 20 bytes of size.
TEST(FileSystemQueueTest, FinalizedBySize) {
  TestOutputFilesProcessor processor;
  MockTime mock_wall_time;
  FSQ fsq(processor, kTestDir, mock_wall_time);

  // Confirm the queue is empty.
  EXPECT_EQ(0ull, fsq.GetQueueStatus().appended_file_size);
  EXPECT_EQ(0u, fsq.GetQueueStatus().finalized.queue.size());
  EXPECT_EQ(0ul, fsq.GetQueueStatus().finalized.total_size);

  // Add a few entries.
  mock_wall_time.now = 101;
  fsq.PushMessage("this is");
  mock_wall_time.now = 102;
  fsq.PushMessage("a test");
  mock_wall_time.now = 103;

  // Confirm the queue is still empty.
  EXPECT_EQ(15ull, fsq.GetQueueStatus().appended_file_size);  // 15 == strlen("this is\na test\n").
  EXPECT_EQ(0u, fsq.GetQueueStatus().finalized.queue.size());
  EXPECT_EQ(0ul, fsq.GetQueueStatus().finalized.total_size);
  EXPECT_EQ(0u, processor.finalized_count);

  // Add another message that would make current file exceed 20 bytes.
  fsq.PushMessage("now go ahead and process this stuff");
  while (!processor.finalized_count) {
    ;  // Spin lock.
  }

  EXPECT_EQ(1u, processor.finalized_count);
  EXPECT_EQ("finalized-00000000000000000101.bin", processor.filename);
  EXPECT_EQ("this is\na test\nnow go ahead and process this stuff\n", processor.contents);
  EXPECT_EQ(103ull, processor.timestamp);
}

// Observe messages being processed as they get older than 10 seconds.
TEST(FileSystemQueueTest, FinalizedByAge) {
  TestOutputFilesProcessor processor;
  MockTime mock_wall_time;
  FSQ fsq(processor, kTestDir, mock_wall_time);

  // Confirm the queue is empty.
  EXPECT_EQ(0ull, fsq.GetQueueStatus().appended_file_size);
  EXPECT_EQ(0u, fsq.GetQueueStatus().finalized.queue.size());
  EXPECT_EQ(0ul, fsq.GetQueueStatus().finalized.total_size);

  // Add a few entries.
  mock_wall_time.now = 10000;
  fsq.PushMessage("this too");
  mock_wall_time.now = 10001;
  fsq.PushMessage("shall");

  // Confirm the queue is still empty.
  EXPECT_EQ(15ull, fsq.GetQueueStatus().appended_file_size);  // 15 == strlen("this is\na test\n").
  EXPECT_EQ(0u, fsq.GetQueueStatus().finalized.queue.size());
  EXPECT_EQ(0ul, fsq.GetQueueStatus().finalized.total_size);
  EXPECT_EQ(0u, processor.finalized_count);

  // Add another message and make the current file span an interval of more than 10 seconds.
  mock_wall_time.now = 21000;
  fsq.PushMessage("pass");

  while (!processor.finalized_count) {
    ;  // Spin lock.
  }

  EXPECT_EQ(1u, processor.finalized_count);
  EXPECT_EQ("finalized-00000000000000010000.bin", processor.filename);
  EXPECT_EQ("this too\nshall\npass\n", processor.contents);
  EXPECT_EQ(21000ull, processor.timestamp);
}

// Pushes a few messages and force their processing.
TEST(FileSystemQueueTest, ForceProcessing) {
  TestOutputFilesProcessor processor;
  MockTime mock_wall_time;
  FSQ fsq(processor, kTestDir, mock_wall_time);

  // Confirm the queue is empty.
  EXPECT_EQ(0ull, fsq.GetQueueStatus().appended_file_size);
  EXPECT_EQ(0u, fsq.GetQueueStatus().finalized.queue.size());
  EXPECT_EQ(0ul, fsq.GetQueueStatus().finalized.total_size);

  // Add a few entries.
  mock_wall_time.now = 1001;
  fsq.PushMessage("foo");
  mock_wall_time.now = 1002;
  fsq.PushMessage("bar");
  mock_wall_time.now = 1003;
  fsq.PushMessage("baz");

  // Confirm the queue is empty.
  EXPECT_EQ(12ull, fsq.GetQueueStatus().appended_file_size);  // Three messages of (3 + '\n') bytes each.
  EXPECT_EQ(0u, fsq.GetQueueStatus().finalized.queue.size());
  EXPECT_EQ(0ul, fsq.GetQueueStatus().finalized.total_size);

  // Force entries processing to have three freshly added ones reach our TestOutputFilesProcessor.
  fsq.ForceProcessing();
  while (!processor.finalized_count) {
    ;  // Spin lock.
  }

  EXPECT_EQ(1u, processor.finalized_count);
  EXPECT_EQ("finalized-00000000000000001001.bin", processor.filename);
  EXPECT_EQ("foo\nbar\nbaz\n", processor.contents);
  EXPECT_EQ(1003ull, processor.timestamp);
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
TEST(FileSystemQueueTest, ConfirmProcessingTakesPlaceBeforeNewFileIsCreated);

*/
