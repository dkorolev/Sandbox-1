#ifndef MQ_DUMMY_H
#define MQ_DUMMY_H

// DummyMQ blocks message sending thread until the message is processed.
// Used by the benchmark as an example of the queue that blocks the thread for as long as possible,
// but does not drop any messages.

#include <string>
#include <mutex>

template <typename CONSUMER_TYPE, typename MESSAGE_TYPE = std::string>
class DummyMQ final {
 public:
  typedef MESSAGE_TYPE T_MESSAGE_TYPE;
  typedef CONSUMER_TYPE T_CONSUMER_TYPE;

  explicit DummyMQ(T_CONSUMER_TYPE& consumer) : consumer_(consumer) {
  }

  void PushMessage(const T_MESSAGE_TYPE& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    consumer_.OnMessage(message, 0);
  }

  void PushMessage(T_MESSAGE_TYPE&& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    consumer_.OnMessage(message, 0);
  }

 private:
  T_CONSUMER_TYPE& consumer_;
  std::mutex mutex_;
};

#endif  // MQ_DUMMY_H
