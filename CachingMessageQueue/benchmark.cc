// A benchmark for the FIFO message queue.
//
// Benchmarks the --queue implemention: "EfficientMQ", "SimpleMQ" or "DummyMQ".
//
// Measures:
//
//   1) Drop rate.
//      Should be zero unless the total push rate approaches or exceeds processing rate,
//      and a non-blocking queue of limited size is used.
//
//   2) Thread lock time.
//      The time for which the thread pushing events is blocked when pushing an event.
//
// Entries pushing side is:
//
//   1) Using --push_threads threads,
//   2) At --push_mbps_per_thread rate,
//   3) With messages of --average_message_length bytes on average,
//      exponentially distributed with the mean of --min_message_length.
//
// Entries receiving side emulates processing messages at --process_mbps rate, exponentialy distributed as well.
//
// The test runs for --seconds seconds.

/*

# Example messages in human-readable format.
./build/benchmark \
  --dump \
  --push_threads 5 \
  --min_message_length=4 \
  --average_message_length=10 \
  --push_mbps_per_thread=0.00001

# Load test, mid-sized messages from several threads.
for q in DummyMQ SimpleMQ EfficientMQ ; do \
  ./build/benchmark \
  --queue=$q \
  --average_message_length=1000 \
  --push_threads=5 \
  --process_mbps=10 ; \
done

# Heavy load test, large messages from many threads.
for q in DummyMQ SimpleMQ EfficientMQ ; do \
  ./build/benchmark \
  --queue=$q \
  --average_message_length=1000000 \
  --push_threads=100 \
  --process_mbps=100 ; \
done

# Consumer slow relative to producers, observe produce speed adjusted to the consumer rate and/or messages dropped.
# Need more time and smaller packets, otherwith most of them end up in the circular buffer of EfficientMQ.
for q in DummyMQ SimpleMQ EfficientMQ ; do \
  ./build/benchmark \
  --queue=$q \
  --average_message_length=100 \
  --push_threads=4 \
  --push_mbps_per_thread=1 \
  --process_mbps=5 \
  --seconds=15 ; \
done

*/

#include <atomic>
#include <cassert>
#include <cstdio>
#include <random>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include <gflags/gflags.h>

#include "mq_efficient.h"
#include "mq_simple.h"
#include "mq_dummy.h"

DEFINE_string(queue, "DummyMQ", "EfficientMQ / SimpleMQ / DummyMQ");

DEFINE_int32(push_threads, 8, "The number of threads that push in messages.");
DEFINE_double(push_mbps_per_thread,
              1.0,
              "The rate, in MBPS, at which each thread sends in the messages, on average.");

DEFINE_int32(min_message_length, 16, "The minimum size of message to send in.");
DEFINE_int32(average_message_length,
             2048,
             "The average size of the message, assuming --min_message size and exponential distribution.");

DEFINE_double(process_mbps, 50.0, "The rate, in MBPS, at which the events are processed by the (fake) consumer.");

DEFINE_double(seconds, 3.0, "The time to run the benchmark for, in seconds.");

DEFINE_bool(log, false, "When debugging, set to true to output more information on the progress of the test.");
DEFINE_bool(dump, false, "When debugging or reading the code, set to true to log all the events.");

// Use wall time in nanoseconds.
double wall_time_ns() {
  return static_cast<double>(std::chrono::duration_cast<std::chrono::nanoseconds>(
                                 std::chrono::system_clock::now().time_since_epoch()).count());
}

// The producer pushes the messages, of messages averaging --average_message_length bytes, at the rate averaging
// --push_mbps.
// The producing speed is stateful, an error is auto-corrected on sending the future events.
//
// In other words, the producer will keep trying to send more and more events, even if the rate at which the queue
// can accept those is less than the rate at which this producer should be producing events.
//
// This is the essence of the benchmark: with high push rate and low processing date,
// either some events will be dropped, or inserting events will block the thread for longer.
template <typename T_MESSAGE_QUEUE>
struct Producer {
  T_MESSAGE_QUEUE& message_queue_;

  const int thread_index_;
  std::mt19937 rng_;
  std::exponential_distribution<> d_message_length_;
  std::exponential_distribution<> d_rate_in_mbps_;
  std::uniform_int_distribution<> d_random_letter_;
  const int min_message_length_;

  int number_of_messages_pushed_ = 0;
  uint64_t total_bytes_pushed_ = 0;
  int total_pushes_above_1ms_ = 0;
  int total_pushes_above_10ms_ = 0;
  int total_pushes_above_100ms_ = 0;

  Producer(T_MESSAGE_QUEUE& message_queue,
           int thread_index,
           double push_mbps,
           int min_message_length,
           int average_message_length)
      : message_queue_(message_queue),
        thread_index_(thread_index),
        rng_(thread_index),
        d_message_length_(1.0 / (average_message_length - min_message_length)),
        d_rate_in_mbps_(1.0 / push_mbps),
        d_random_letter_('a', 'z'),
        min_message_length_(min_message_length) {
    assert(min_message_length >= 3);
    assert(average_message_length > min_message_length);
  }

  void RunProducingThread(std::atomic_bool& done) {
    double last_ns = wall_time_ns();
    double next_cutoff_ns = last_ns;
    while (!done) {
      while (wall_time_ns() < next_cutoff_ns) {
        // Spin lock.
        if (done) {
          return;
        }
      }

      const size_t message_length_in_b = static_cast<size_t>(d_message_length_(rng_) + min_message_length_ + 0.5);
      const double message_length_in_mb = 1e-6 * message_length_in_b;
      const double rate_in_mbps = d_rate_in_mbps_(rng_);
      const double send_time_in_ns = 1e9 * message_length_in_mb / rate_in_mbps;

      next_cutoff_ns += send_time_in_ns;

      std::string message(message_length_in_b, ' ');
      message[0] = '0' + ((thread_index_ / 10) % 10);
      message[1] = '0' + (thread_index_ % 10);
      for (size_t i = 3; i < message.length(); ++i) {
        message[i] = d_random_letter_(rng_);
      }

      if (FLAGS_dump) {
        printf("SEND: %s\n", message.c_str());
      }

      {
        // Send this message and measure the time it took.
        const double ns_before = wall_time_ns();
        message_queue_.PushMessage(message);
        const double ns_after = wall_time_ns();
        const double push_ns = ns_after - ns_before;
        ++number_of_messages_pushed_;
        total_bytes_pushed_ += message.length();
        if (push_ns >= 1e6) {
          ++total_pushes_above_1ms_;
          if (push_ns >= 1e7) {
            ++total_pushes_above_10ms_;
            if (push_ns >= 1e8) {
              ++total_pushes_above_100ms_;
            }
          }
        }
      }
    }
  }
};

// The consumer accepts the messages, at the rate averaging --process_mbps.
struct Consumer {
  std::atomic_bool& done_;

  int total_messages_processed_ = 0;
  uint64_t total_bytes_processed_ = 0;
  size_t total_messages_dropped_ = 0;

  std::mt19937 rng_;
  std::exponential_distribution<> process_mbps_distribution_;

  Consumer(std::atomic_bool& done, double process_mbps, int random_seed = 0)
      : done_(done), rng_(random_seed), process_mbps_distribution_(1.0 / process_mbps) {
  }

  void OnMessage(const std::string& message, size_t dropped_count) {
    if (!done_) {
      const double timestamp_ns = wall_time_ns();

      ++total_messages_processed_;
      total_bytes_processed_ += message.length();
      total_messages_dropped_ += dropped_count;

      if (FLAGS_dump) {
        printf("RECV: %s\n", message.c_str());
      }

      // Emulate event processing delay assuming --process_mbps average processing rate.
      // Note that mathematically the processing rate will be slightly lower :) -- D.K.
      const double rate_in_mbps = process_mbps_distribution_(rng_);
      const double size_in_mb = 1e-6 * message.length();
      const double processing_time_in_s = size_in_mb / rate_in_mbps;

      const double wait_end_ns = timestamp_ns + 1e9 * processing_time_in_s;
      while (wall_time_ns() < wait_end_ns) {
        if (done_) {
          return;
        }
      }
    }
  }
};

template <typename T_MESSAGE_QUEUE>
void RunBenchmark(const std::string& queue_name) {
  const int number_of_threads = FLAGS_push_threads;
  const double benchmark_seconds = FLAGS_seconds;

  printf(
      "Benchmarking on %.2lf seconds:\n"
      "  Queue %s\n"
      "  %d threads pushing events at %.2lf MBPS each\n"
      "  events being processed at %.2lf MBPS\n"
      "  messages of average size %d bytes (%.2lf MB), with the minimum of %d bytes (%.2lf MB)\n",
      benchmark_seconds,
      queue_name.c_str(),
      number_of_threads,
      FLAGS_push_mbps_per_thread,
      FLAGS_process_mbps,
      FLAGS_average_message_length,
      1e-6 * FLAGS_average_message_length,
      FLAGS_min_message_length,
      1e-6 * FLAGS_min_message_length);

  std::atomic_bool done(false);

  Consumer consumer(done, FLAGS_process_mbps);

  {
    T_MESSAGE_QUEUE queue(consumer);

    std::vector<std::unique_ptr<Producer<T_MESSAGE_QUEUE>>> producers(number_of_threads);
    for (size_t i = 0; i < number_of_threads; ++i) {
      producers[i].reset(new Producer<T_MESSAGE_QUEUE>(
          queue, i + 1, FLAGS_push_mbps_per_thread, FLAGS_min_message_length, FLAGS_average_message_length));
    }

    if (FLAGS_log) {
      printf("Running the benchmark for %.1lf seconds.\n", benchmark_seconds);
    }

    std::vector<std::thread> threads(number_of_threads);
    for (size_t i = 0; i < number_of_threads; ++i) {
      threads[i] = std::thread(&Producer<T_MESSAGE_QUEUE>::RunProducingThread, producers[i].get(), std::ref(done));
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<uint64_t>(1e3 * benchmark_seconds)));
    if (FLAGS_log) {
      printf("Finalizing the benchmark.\n");
    }
    done = true;

    for (size_t i = 0; i < number_of_threads; ++i) {
      threads[i].join();
    }

    if (FLAGS_log) {
      printf("The benchmark is complete.\n");
      printf("\n");
    }

    int N = 0;                                 // Total messages pushed.
    uint64_t B = 0;                            // Total bytes pushed.
    int N2 = 0;                                // Total messages processed.
    uint64_t B2 = 0;                           // Total bytes processed.
    int M = consumer.total_messages_dropped_;  // Messages dropped.
    int C1ms = 0, C10ms = 0, C100ms = 0;       // Total messages that took longer than { 1ms, 10ms, 100ms }.

    for (size_t i = 0; i < number_of_threads; ++i) {
      N += producers[i]->number_of_messages_pushed_;
      B += producers[i]->total_bytes_pushed_;
      C1ms += producers[i]->total_pushes_above_1ms_;
      C10ms += producers[i]->total_pushes_above_10ms_;
      C100ms += producers[i]->total_pushes_above_100ms_;
    }

    N2 = consumer.total_messages_processed_;
    B2 = consumer.total_bytes_processed_;

    printf("Total messages pushed:  %14d (%.3lf GB, %.3lf MB/s)\n", N, 1e-9 * B, 1e-6 * B / benchmark_seconds);
    printf("Total messages parsed:  %14d (%.3lf GB, %.3lf MB/s)\n", N2, 1e-9 * B2, 1e-6 * B2 / benchmark_seconds);
    printf("Total messages dropped: %14d (%.2lf%%)\n", M, 100.0 * M / N);

    printf("Push time >= 1ms:   %18d (%.2lf%%)\n", C1ms, 100.0 * C1ms / N);
    printf("Push time >= 10ms:  %18d (%.2lf%%)\n", C10ms, 100.0 * C10ms / N);
    printf("Push time >= 100ms: %18d (%.2lf%%)\n", C100ms, 100.0 * C100ms / N);

    if (FLAGS_log) {
      printf("\n");
      printf("Waiting for cached events to replay before terminating: ");
      fflush(stdout);
    }
  }
  if (FLAGS_log) {
    printf("Done.\n");
  }
}

int main(int argc, char** argv) {
  if (!google::ParseCommandLineFlags(&argc, &argv, true)) {
    return -1;
  }
  if (FLAGS_queue == "EfficientMQ") {
    RunBenchmark<EfficientMQ<Consumer>>(FLAGS_queue);
  } else if (FLAGS_queue == "SimpleMQ") {
    RunBenchmark<SimpleMQ<Consumer>>(FLAGS_queue);
  } else if (FLAGS_queue == "DummyMQ") {
    RunBenchmark<DummyMQ<Consumer>>(FLAGS_queue);
  } else {
    printf("Undefined queue implementation: '%s'.\n", FLAGS_queue.c_str());
    return -1;
  }
}
