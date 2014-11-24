#ifndef SANDBOX_TEST_MOCKS_H
#define SANDBOX_TEST_MOCKS_H

#include <cstdint>
#include <exception>
#include <map>
#include <string>

struct MockTimeManager final {
 public:
  typedef uint64_t T_TIMESTAMP;
  MockTimeManager(T_TIMESTAMP ms = 0) : ms(ms) {
  }
  T_TIMESTAMP wall_time() const {
    return ms;
  }
  T_TIMESTAMP ms;
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

template <typename TIMESTAMP>
struct GenericMockExporter final {
  typedef TIMESTAMP T_TIMESTAMP;
  void OnFileCommitted(const std::string& filename,
                       const uint64_t length,
                       const T_TIMESTAMP first_ms,
                       const T_TIMESTAMP last_ms) {
  }
};
typedef struct GenericMockExporter<typename MockTimeManager::T_TIMESTAMP> MockExporter;

#endif  // SANDBOX_TEST_MOCKS_H
