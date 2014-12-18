// This header file is provided here for the convenience of Alex from Minsk, Belarus, of using FSQ.

#include <chrono>
#include <iostream>
#include <thread>

#include "fsq.h"
#include "../Bricks/file/file.h"

struct MinimalisticProcessor {
  template <typename T_TIMESTAMP, typename T_TIME_SPAN>
  fsq::FileProcessingResult OnFileReady(const std::string& file_name,
                                        const std::string& /*file_base_name*/,
                                        uint64_t /*size*/,
                                        T_TIMESTAMP /*created*/,
                                        T_TIME_SPAN /*age*/,
                                        T_TIMESTAMP /*now*/) {
    std::cerr << file_name << std::endl << bricks::ReadFileAsString(file_name) << std::endl;
    return fsq::FileProcessingResult::Success;
  }
};

int main() {
  MinimalisticProcessor processor;
  fsq::FSQ<fsq::Config<MinimalisticProcessor>> fsq(processor, ".");
  fsq.PushMessage("Hello, World!\n");
  fsq.ForceResumeProcessing();
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
}
