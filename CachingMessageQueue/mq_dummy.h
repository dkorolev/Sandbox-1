#ifndef SANDBOX_MQ_DUMMY_H
#define SANDBOX_MQ_DUMMY_H

// DummyMQ blocks message sending thread until the message is processed.
// Used by the benchmark as an example of the queue that blocks the thread for as long as possible,
// but does not drop any messages.

#include <string>
#include <mutex>

template <typename CONSUMER, typename MESSAGE = std::string>
class DummyMQ final {
 public:
  typedef MESSAGE T_MESSAGE;
  typedef CONSUMER T_CONSUMER;

  explicit DummyMQ(T_CONSUMER& consumer) : consumer_(consumer) {
  }

  void PushMessage(const T_MESSAGE& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    consumer_.OnMessage(message, 0);
  }

  void PushMessage(T_MESSAGE&& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    consumer_.OnMessage(message, 0);
  }

 private:
  T_CONSUMER& consumer_;
  std::mutex mutex_;
};

#endif  // SANDBOX_MQ_DUMMY_H
