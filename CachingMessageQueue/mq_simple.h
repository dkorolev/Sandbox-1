#ifndef SANDBOX_MQ_SIMPLE_H
#define SANDBOX_MQ_SIMPLE_H

// SimpleMQ blocks message sending thread until the message is added to the queue.
// Used by the benchmark as an example of the queue that blocks the thread for the entire data copy operation,
// but does not drop any messages.

#include <condition_variable>
#include <deque>
#include <mutex>
#include <string>
#include <thread>

template <typename CONSUMER, typename MESSAGE = std::string>
class SimpleMQ final {
 public:
  typedef MESSAGE T_MESSAGE;
  typedef CONSUMER T_CONSUMER;

  explicit SimpleMQ(T_CONSUMER& consumer)
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

  void PushMessage(const T_MESSAGE& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    deque_.push_back(message);
    condition_variable_.notify_all();
  }

  void PushMessage(T_MESSAGE&& message) {
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

  T_CONSUMER& consumer_;
  std::thread consumer_thread_;
  std::deque<T_MESSAGE> deque_;
  bool destructing_ = false;
  std::mutex mutex_;
  std::condition_variable condition_variable_;
};

#endif  // SANDBOX_MQ_SIMPLE_H
