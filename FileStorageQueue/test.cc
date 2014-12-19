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
  template <typename T_TIMESTAMP, typename T_TIME_SPAN>
  fsq::FileProcessingResult OnFileReady(const std::string& file_name,
                                        const std::string& file_base_name,
                                        uint64_t /*size*/,
                                        T_TIMESTAMP /*created*/,
                                        T_TIME_SPAN /*age*/,
                                        T_TIMESTAMP /*now*/) {
    filename = file_base_name;
    contents = bricks::ReadFileAsString(file_name);
    finalized_count = true;
    return fsq::FileProcessingResult::Success;
  }

  atomic_size_t finalized_count;
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

struct MockConfig : fsq::Config<TestOutputFilesProcessor> {
  // Mock time.
  typedef MockTime T_TIME_MANAGER;
  // Append using newlines.
  typedef fsq::strategy::AppendToFileWithSeparator T_FILE_APPEND_POLICY;
  // No backlog: 20 bytes 10 seconds old files max, with backlog: 100 bytes 60 seconds old files max.
  typedef fsq::strategy::SimpleFinalizationPolicy<MockTime::T_TIMESTAMP,
                                                  MockTime::T_TIME_SPAN,
                                                  20,
                                                  MockTime::T_TIME_SPAN(10 * 1000),
                                                  100,
                                                  MockTime::T_TIME_SPAN(60 * 1000)> T_FINALIZE_POLICY;
  // Purge after 1000 bytes total or after 3 files.
  typedef fsq::strategy::SimplePurgePolicy<1000, 3> T_PURGE_POLICY;

  // Non-static initialization.
  template <typename T_FSQ_INSTANCE>
  static void Initialize(T_FSQ_INSTANCE& instance) {
    instance.SetSeparator("\n");
  }
};

typedef fsq::FSQ<MockConfig> FSQ;

TEST(FileSystemQueueTest, SimpleSmokeTest) {
  TestOutputFilesProcessor processor;
  MockTime mock_wall_time;
  FSQ fsq(processor, kTestDir, mock_wall_time);
  //  fsq.SetSeparator("\n");

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
  mock_wall_time.now = 1010;

  // Confirm the queue is empty.
  EXPECT_EQ(12ull, fsq.GetQueueStatus().appended_file_size);  // Three messages of (3 + '\n') bytes each.
  EXPECT_EQ(0u, fsq.GetQueueStatus().finalized.queue.size());
  EXPECT_EQ(0ul, fsq.GetQueueStatus().finalized.total_size);

  // Force entries processing to have three freshly added ones reach our TestOutputFilesProcessor.
  fsq.ForceResumeProcessing();
  while (!processor.finalized_count) {
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
