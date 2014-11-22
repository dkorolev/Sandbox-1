#ifndef MQ_SIMPLE_H
#define MQ_SIMPLE_H

// SimpleMQ blocks message sending thread until the message is added to the queue.
// Used by the benchmark as an example of the queue that blocks the thread for the entire data copy operation,
// but does not drop any messages.

#include <condition_variable>
#include <deque>
#include <mutex>
#include <string>
#include <thread>

template <typename CONSUMER_TYPE, typename MESSAGE_TYPE = std::string>
class SimpleMQ final {
 public:
  typedef MESSAGE_TYPE T_MESSAGE_TYPE;
  typedef CONSUMER_TYPE T_CONSUMER_TYPE;

  explicit SimpleMQ(T_CONSUMER_TYPE& consumer)
      : consumer_(consumer), consumer_thread_(&SimpleMQ::ConsumerThread, this) {
  }

  ~SimpleMQ() {
    {
      std::unique_lock<std::mutex> lock(mutex_);
      destructing_ = true;
    }
    condition_variable_.notify_all();
    consumer_thread_.join();
  }

  void PushMessage(const T_MESSAGE_TYPE& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    deque_.push_back(message);
    condition_variable_.notify_all();
  }

  void PushMessage(T_MESSAGE_TYPE&& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    deque_.emplace_back(message);
    condition_variable_.notify_all();
  }

 private:
  void ConsumerThread() {
    std::unique_lock<std::mutex> lock(mutex_);
    while (true) {
      if (deque_.empty()) {
        if (destructing_) {
          return;
        }
        condition_variable_.wait(lock, [this] { return !deque_.empty() || destructing_; });
        if (destructing_) {
          return;
        }
      }
      consumer_.OnMessage(deque_.front(), 0);
      deque_.pop_front();
    }
  }

  T_CONSUMER_TYPE& consumer_;
  std::thread consumer_thread_;
  std::deque<MESSAGE_TYPE> deque_;
  bool destructing_ = false;
  std::mutex mutex_;
  std::condition_variable condition_variable_;
};

#endif  // MQ_SIMPLE_H
