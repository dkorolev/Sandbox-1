#ifndef SANDBOX_LINUX_FILE_MANAGER_H
#define SANDBOX_LINUX_FILE_MANAGER_H

// A wrapper for the filesystem. Features file append, rename, read and directory scan.
// Directory scan only supports question marks in patterns.
// Uses C++11 complemented with POSIX rename(), stat(), remove() and {open,read,close}dir().

#include <cstdio>  // rename().
#include <exception>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include <dirent.h>    // {open,read,close}dir().
#include <sys/stat.h>  // stat().

struct FileManager {
  struct Exception : std::exception {};
  struct CanNotCreateFileException : Exception {};
  struct FileAlreadyExistsException : Exception {};
  struct CanNotReadFileException : Exception {};
  struct CanNotRenameFileException : Exception {};
  struct CanNotGetFileSizeException : Exception {};
  struct CanNotRemoveFileException : Exception {};
  struct CanNotScanDirectoryException : Exception {};
  struct NullFileHandleException : Exception {};
  struct NullDirectoryIteratorException : Exception {};
  struct NeedTrailingSlashInWorkingDirectoryException : Exception {};
};

class LinuxFileManager final : FileManager {
 public:
  class Handle {
   public:
    explicit inline Handle(const std::string& absolute_filename, bool truncate)
        : impl_(new Impl(absolute_filename, truncate)) {
    }

    inline Handle(Handle&& rhs) : impl_(std::move(rhs.impl_)) {
    }

    inline void operator=(Handle&& rhs) {
      impl_.reset();
      impl_.swap(rhs.impl_);
    }

    Handle() = delete;

    inline Handle& Append(const std::string& s) {
      if (!impl_) {
        throw NullFileHandleException();
      } else {
        impl_->Append(s);
        return *this;
      }
    }

   private:
    class Impl {
     public:
      inline Impl(const std::string& absolute_filename, bool truncate)
          : fo_(absolute_filename, std::ios::binary | (truncate ? std::ios::trunc : std::ios::app)) {
        if (!fo_) {
          throw CanNotCreateFileException();
        }
      }
      inline void Append(const std::string& s) {
        fo_ << s << std::flush;
      }

     private:
      std::ofstream fo_;
    };

   private:
    std::unique_ptr<Impl> impl_;
  };

  class DirectoryIterator {
   public:
    inline DirectoryIterator(const std::string& path, const std::string& pattern)
        : dir_(opendir(path.c_str())), pattern_(pattern) {
      if (!dir_) {
        throw CanNotScanDirectoryException();
      }
    }

    inline DirectoryIterator(DirectoryIterator&& rhs) : dir_(rhs.dir_) {
      rhs.dir_ = 0;
    }

    inline void operator=(DirectoryIterator&& rhs) {
      dir_ = rhs.dir_;
      rhs.dir_ = 0;
    }

    DirectoryIterator() = delete;

    inline ~DirectoryIterator() {
      if (dir_) {
        closedir(dir_);
      }
    }

    inline std::string Next() {
      if (!dir_) {
        throw NullDirectoryIteratorException();
      } else {
        dirent* entry;
        while (true) {
          entry = readdir(dir_);
          if (!entry) {
            return "";
          } else {
            const std::string current(entry->d_name);
            if (current != "." && current != ".." && Match(current)) {
              return current;
            }
          }
        }
      }
    }

   private:
    inline bool Match(const std::string& s) const {
      if (s.length() == pattern_.length()) {
        for (size_t i = 0; i < s.length(); ++i) {
          if (!(s[i] == pattern_[i] || pattern_[i] == '?')) {
            return false;
          }
        }
        return true;
      } else {
        return false;
      }
    }

    DIR* dir_;
    std::string pattern_;
  };

  // Should include the trailing slash.
  explicit inline LinuxFileManager(const std::string& working_dir_with_trailing_slash = "./.tmp/")
      : dir_prefix_(working_dir_with_trailing_slash) {
    if (dir_prefix_.empty() || dir_prefix_.back() != '/') {
      throw NeedTrailingSlashInWorkingDirectoryException();
    }
  }

  inline Handle CreateFile(const std::string& filename) const {
    try {
      GetFileSize(filename);
      throw FileAlreadyExistsException();
    } catch (CanNotGetFileSizeException&) {
      return Handle(dir_prefix_ + filename, true);
    }
  }

  inline Handle CreateOrAppendToFile(const std::string& filename) const {
    return Handle(dir_prefix_ + filename, false);
  }

  inline std::vector<char> ReadFile(const std::string& filename) const {
    std::ifstream fi(dir_prefix_ + filename);
    if (!fi) {
      throw CanNotReadFileException();
    } else {
      fi.seekg(0, std::ios::end);
      const size_t size = fi.tellg();
      fi.seekg(0, std::ios::beg);
      std::vector<char> data(size);
      fi.read(&data[0], size);
      if (!fi) {
        throw CanNotReadFileException();
      }
      return data;
    }
  }

  inline std::string ReadFileToString(const std::string& filename) const {
    std::vector<char> data = ReadFile(filename);
    return std::string(data.begin(), data.end());
  }

  inline void RenameFile(const std::string& from, const std::string& to) const {
    if (rename((dir_prefix_ + from).c_str(), (dir_prefix_ + to).c_str()) == -1) {
      throw CanNotRenameFileException();
    }
  }

  inline size_t GetFileSize(const std::string& filename) const {
    struct stat result;
    if (stat((dir_prefix_ + filename).c_str(), &result)) {
      throw CanNotGetFileSizeException();
    } else {
      return result.st_size;
    }
  }

  inline void RemoveFile(const std::string& filename) const {
    if (remove((dir_prefix_ + filename).c_str()) != 0) {
      throw CanNotRemoveFileException();
    }
  }

  inline DirectoryIterator ScanDirectory(const std::string& pattern) const {
    return DirectoryIterator(dir_prefix_, pattern);
  }

 private:
  // Should include the trailing slash, potentially plarform-dependent.
  const std::string dir_prefix_;
};

#endif  // SANDBOX_LINUX_FILE_MANAGER_H
