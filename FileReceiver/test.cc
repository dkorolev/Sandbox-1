// This code processes files uploaded to nginx. Requirements:
//
// 1) nginx configured to:
//
// 1.1) accept files uploaded via POST to a certain URL, defaults to localhost:8088/upload,
// 1.2) collecting uploaded files in certain directory, defaults to /home/www-data/uploads,
// 1.3) proxy-pass those requests another URL on cerain port, defaults to localhost:8089/file_uploaded.
//
// 2) this test invoked in a way that has access to those files.
//    running it from a `www-data` username did the trick for me -- D.K.

// Configuration for nginx:

/*
server {
  listen   8088;

  location /upload {
    limit_except POST { deny all; }
    client_body_temp_path /home/www-data/uploads;
    client_body_in_file_only on;
    proxy_pass_request_headers on;
    proxy_set_header X-FILE $request_body_file;
    proxy_set_body off;
    proxy_redirect off;
    proxy_pass http://localhost:8089/file_uploaded;
  }
}
*/

// How to test nginx upload setup without running this code:
/*
# Upload data consisting of "Hello, World." followed by a newline.
curl -s --url localhost:8088/upload --data-binary @<(echo "Hello, World.") >/dev/null
# Observe this data file appearing.
(cd /home/www-data/uploads ; ls -t | head -n 1 | xargs cat)
# Delete the most recent file.
(cd /home/www-data/uploads ; ls -t | head -n 1 | xargs rm)
*/

// TODO(dkorolev): Migrate to a simpler HTTP server implementation
//                 that is to be added to Bricks' net/api.h soon.

#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>

#include "../Bricks/dflags/dflags.h"
#include "../Bricks/file/file.h"
#include "../Bricks/net/api/api.h"
#include "../Bricks/strings/printf.h"

#include "../Bricks/3party/gtest/gtest.h"
#include "../Bricks/3party/gtest/gtest-main-with-dflags.h"

using namespace bricks::net;       // Socket, HTTPResponseCode.
using namespace bricks::net::api;  // GET, POST.

DEFINE_string(expected_arch, "", "The expected architecture to run on, `uname` on *nix systems.");
DEFINE_string(upload_url, "http://localhost:8088/upload", "Path to upload test data to.");
DEFINE_int32(local_port, 8089, "Local port to listen to uploaded file events on.");
DEFINE_string(local_http_path, "/file_uploaded", "Local HTTP path triggered as a new file is uploaded.");
DEFINE_string(uploads_directory, "/home/www-data/uploads", "Local directory where uploaded files will appear.");
DEFINE_int64(dir_poll_period_ms, 100, "Number of milliseconds between explicit directory polling.");
DEFINE_string(full_file_name_http_header, "X-FILE", "The name of HTTP header nginx uses for file name.");
DEFINE_string(content_type_http_header, "Content-Type", "The name of HTTP header content type is passed in.");

TEST(ArchitectureTest, BRICKS_ARCH_UNAME_AS_IDENTIFIER) {
  ASSERT_EQ(BRICKS_ARCH_UNAME, FLAGS_expected_arch);
}

class FileReceiveServer final {
 public:
  FileReceiveServer()
      : web_thread_(&FileReceiveServer::ThreadWeb, this, Socket(FLAGS_local_port)),
        dir_thread_(&FileReceiveServer::ThreadDir, this) {
    // FOR THE TEST ONLY: REMOVE ALL PREVIOUSLY UPLOADED FILES.
    bricks::FileSystem::ScanDir(FLAGS_uploads_directory, [](const std::string& filename) {
      bricks::RemoveFile(bricks::FileSystem::JoinPath(FLAGS_uploads_directory, filename));
    });
  }

  ~FileReceiveServer() {
    const auto response = HTTP(GET(bricks::strings::Printf("http://localhost:%d/stop", FLAGS_local_port)));
    assert(response.code == 200);
    assert(response.body == "TERMINATING\n");
    web_thread_.join();
    dir_thread_.join();
  }

  size_t NumberOfFilesScanned() const {
    return number_of_files_scanned_;
  }

  size_t NumberOfUploadRequestsReceived() const {
    return number_of_upload_requests_received_;
  }

 private:
  void ThreadWeb(Socket socket) {
    while (!terminate_) {
      HTTPServerConnection connection(socket.Accept());
      const auto& message = connection.Message();
      const std::string method = message.Method();
      const std::string url = message.URL();
      if (url == "/healthz") {
        connection.SendHTTPResponse("OK\n");
      } else if (url == "/stop") {
        connection.SendHTTPResponse("TERMINATING\n");
        terminate_ = true;
        cv_.notify_all();
      } else if (url == FLAGS_local_http_path) {
        ++number_of_upload_requests_received_;
        const std::map<std::string, std::string>& headers = message.headers;
        auto cit_full_file_name = headers.find(FLAGS_full_file_name_http_header);
        auto cit_content_type = headers.find(FLAGS_content_type_http_header);
        if (cit_full_file_name != headers.end() && cit_content_type != headers.end()) {
          std::cerr << "RECEIVED: " << cit_full_file_name->second << ' ' << cit_content_type->second
                    << std::endl;
        }
        connection.SendHTTPResponse("RECEIVED\n", HTTPResponseCode::Accepted);
        // TODO(dkorolev) + TODO(deathbaba): See whether newly uploaded file name can be extracted.
        cv_.notify_all();
      } else {
        connection.SendHTTPResponse("ERROR\n", HTTPResponseCode::NotFound);
      }
    }
  }

  void ThreadDir() {
    while (!terminate_) {
      // Wait for the next file. Scan the directory periodically regardless.
      {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait_for(lock, std::chrono::milliseconds(FLAGS_dir_poll_period_ms));
      }
      std::string filename;
      bricks::FileSystem::ScanDirUntil(FLAGS_uploads_directory, [&filename](const std::string& fn) {
        filename = fn;
        return false;
      });
      if (!filename.empty()) {
        std::cerr << "SCANNED: " << filename << std::endl;
        filename = bricks::FileSystem::JoinPath(FLAGS_uploads_directory, filename);
        std::cerr << "SCANNED FULL PATH: " << filename << std::endl;
        std::cerr << "CONENTS: " << bricks::ReadFileAsString(filename) << std::endl;
        ++number_of_files_scanned_;
        bricks::RemoveFile(filename);
      }
    }
  }

  size_t number_of_files_scanned_ = 0;
  size_t number_of_upload_requests_received_ = 0;

  std::mutex mutex_;
  std::condition_variable cv_;

  std::thread web_thread_;
  std::thread dir_thread_;
  bool terminate_ = false;
};

TEST(FileReceiverTest, UploadFileViaNginx) {
  FileReceiveServer scoped_server;

  EXPECT_EQ(0u, scoped_server.NumberOfUploadRequestsReceived());
  const auto response = HTTP(POST(FLAGS_upload_url, "UploadedViaNginx\n", "application/some-magic-type"));
  EXPECT_EQ(202, response.code);
  EXPECT_EQ(1u, scoped_server.NumberOfUploadRequestsReceived());
  while (scoped_server.NumberOfFilesScanned() != 1u) {
    ;  // Spin lock;
  }
}

TEST(FileReceiverTest, DirectoryIsAlsoScannedIndependently) {
  FileReceiveServer scoped_server;

  bricks::WriteStringToFile(bricks::FileSystem::JoinPath(FLAGS_uploads_directory, "testfile"), "MammaMia");
  EXPECT_EQ(0u, scoped_server.NumberOfUploadRequestsReceived());
  while (scoped_server.NumberOfFilesScanned() != 1u) {
    ;  // Spin lock;
  }
}

TEST(FileReceiverTest, Healthz) {
  FileReceiveServer scoped_server;

  const auto response = HTTP(GET(bricks::strings::Printf("http://localhost:%d/healthz", FLAGS_local_port)));
  EXPECT_EQ(200, response.code);
  EXPECT_EQ("OK\n", response.body);
}

TEST(FileReceiverTest, FourOhFour) {
  FileReceiveServer scoped_server;
  static_cast<void>(scoped_server);

  const auto response = HTTP(GET(bricks::strings::Printf("http://localhost:%d/foo", FLAGS_local_port)));
  EXPECT_EQ(404, response.code);
  EXPECT_EQ("ERROR\n", response.body);
}
