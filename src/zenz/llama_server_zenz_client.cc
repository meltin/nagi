// Copyright 2026 The Nagi Authors.
// Use of this source code is governed by the BSD 3-Clause license in LICENSE.

// Zenz client that runs llama-server as a child process and talks to it over
// HTTP on the loopback interface. Everything stays on this machine.

#include <arpa/inet.h>
#include <fcntl.h>
#include <libproc.h>
#include <netinet/in.h>
#include <signal.h>
#include <spawn.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include "absl/log/log.h"
#include "absl/strings/str_cat.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "base/file_util.h"
#include "base/strings/unicode.h"
#include "base/system_util.h"
#include "zenz/zenz_client.h"

extern char** environ;

namespace mozc {
namespace zenz {
namespace {

constexpr char kHost[] = "127.0.0.1";
// A request that takes longer than this is useless for typing.
constexpr int kRequestTimeoutSec = 3;
// Loading the model takes a moment on the first request.
constexpr absl::Duration kStartupTimeout = absl::Seconds(15);
// Do not restart a server that keeps dying more often than this.
constexpr absl::Duration kRespawnInterval = absl::Seconds(5);
constexpr size_t kMaxResponseBytes = 1 << 20;

void SetTimeout(int fd, int seconds) {
  const timeval timeout = {seconds, 0};
  ::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
  ::setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
}

// Asks the OS for a port nobody listens on.
int PickFreePort() {
  const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) {
    return 0;
  }
  sockaddr_in address = {};
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  address.sin_port = 0;
  socklen_t length = sizeof(address);
  int port = 0;
  if (::bind(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0 &&
      ::getsockname(fd, reinterpret_cast<sockaddr*>(&address), &length) == 0) {
    port = ntohs(address.sin_port);
  }
  ::close(fd);
  return port;
}

// Decodes a body sent with "Transfer-Encoding: chunked".
std::optional<std::string> DecodeChunked(std::string_view body) {
  std::string decoded;
  while (true) {
    const size_t line_end = body.find("\r\n");
    if (line_end == std::string_view::npos) {
      return std::nullopt;
    }
    const size_t size =
        std::strtoul(std::string(body.substr(0, line_end)).c_str(), nullptr, 16);
    body.remove_prefix(line_end + 2);
    if (size == 0) {
      return decoded;
    }
    if (body.size() < size + 2) {
      return std::nullopt;
    }
    decoded.append(body.substr(0, size));
    body.remove_prefix(size + 2);
  }
}

class LlamaServerZenzClient : public ZenzClientInterface {
 public:
  LlamaServerZenzClient(std::string server_path, std::string model_path,
                        std::string pid_path)
      : server_path_(std::move(server_path)),
        model_path_(std::move(model_path)),
        pid_path_(std::move(pid_path)) {}

  ~LlamaServerZenzClient() override { Shutdown(); }

  void Request(uint32_t id, std::string_view reading,
               std::string_view left_context) override {
    std::lock_guard<std::mutex> lock(mutex_);
    latest_id_ = id;
    has_request_ = true;
    pending_ = Job{id, std::string(reading), std::string(left_context)};
    done_.reset();
    AbortActiveRequestLocked();
    if (!worker_.joinable()) {
      worker_ = std::thread([this] { WorkerLoop(); });
    }
    condition_.notify_one();
  }

  ZenzResult Take(uint32_t id) override {
    std::lock_guard<std::mutex> lock(mutex_);
    if (done_ && done_->first == id) {
      return done_->second;
    }
    ZenzResult result;
    result.state = (has_request_ && latest_id_ == id) ? ZenzResult::PENDING
                                                      : ZenzResult::FAILED;
    return result;
  }

  void Cancel() override {
    std::lock_guard<std::mutex> lock(mutex_);
    has_request_ = false;
    pending_.reset();
    done_.reset();
    AbortActiveRequestLocked();
  }

  void WarmUp() override {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      if (warmed_up_) {
        return;
      }
      warmed_up_ = true;
    }
    // Any request starts the server and loads the model.
    Request(0, "あ", "");
  }

  // Stops the worker and the server. Safe to call more than once.
  void Shutdown() {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      shutdown_ = true;
      AbortActiveRequestLocked();
      condition_.notify_one();
    }
    if (worker_.joinable()) {
      worker_.join();
    }
    StopServer();
  }

 private:
  struct Job {
    uint32_t id;
    std::string reading;
    std::string left_context;
  };

  // Makes a blocked read of the worker return, so that it can take the newer
  // request. llama-server stops generating when the connection closes.
  void AbortActiveRequestLocked() {
    if (active_fd_ >= 0) {
      ::shutdown(active_fd_, SHUT_RDWR);
    }
  }

  void WorkerLoop() {
    while (true) {
      Job job;
      {
        std::unique_lock<std::mutex> lock(mutex_);
        condition_.wait(lock, [this] { return shutdown_ || pending_; });
        if (shutdown_) {
          return;
        }
        job = std::move(*pending_);
        pending_.reset();
      }

      ZenzResult result;
      result.state = ZenzResult::FAILED;
      if (EnsureServer()) {
        if (std::optional<std::string> text = Complete(job)) {
          result.state = ZenzResult::READY;
          result.text = std::move(*text);
        }
      }

      std::lock_guard<std::mutex> lock(mutex_);
      // A newer request may have replaced this one while it was running.
      if (has_request_ && latest_id_ == job.id && !pending_) {
        done_ = std::make_pair(job.id, std::move(result));
      }
    }
  }

  // Sends one HTTP request and returns the response body.
  std::optional<std::string> Http(std::string_view method,
                                  std::string_view path,
                                  std::string_view body) {
    const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
      return std::nullopt;
    }
    SetTimeout(fd, kRequestTimeoutSec);
    {
      std::lock_guard<std::mutex> lock(mutex_);
      active_fd_ = fd;
    }
    std::optional<std::string> response = HttpOnSocket(fd, method, path, body);
    {
      std::lock_guard<std::mutex> lock(mutex_);
      active_fd_ = -1;
    }
    ::close(fd);
    return response;
  }

  std::optional<std::string> HttpOnSocket(int fd, std::string_view method,
                                          std::string_view path,
                                          std::string_view body) {
    sockaddr_in address = {};
    address.sin_family = AF_INET;
    address.sin_port = htons(port_);
    ::inet_pton(AF_INET, kHost, &address.sin_addr);
    if (::connect(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) !=
        0) {
      return std::nullopt;
    }
    const std::string request = absl::StrCat(
        method, " ", path, " HTTP/1.1\r\nHost: ", kHost,
        "\r\nConnection: close\r\nContent-Type: application/json\r\n"
        "Content-Length: ",
        body.size(), "\r\n\r\n", body);
    size_t sent = 0;
    while (sent < request.size()) {
      const ssize_t n = ::send(fd, request.data() + sent, request.size() - sent, 0);
      if (n <= 0) {
        return std::nullopt;
      }
      sent += n;
    }
    std::string response;
    char buffer[4096];
    while (true) {
      const ssize_t n = ::recv(fd, buffer, sizeof(buffer), 0);
      if (n < 0) {
        return std::nullopt;  // Timed out or aborted.
      }
      if (n == 0) {
        break;
      }
      response.append(buffer, n);
      if (response.size() > kMaxResponseBytes) {
        return std::nullopt;
      }
    }
    const size_t header_end = response.find("\r\n\r\n");
    if (header_end == std::string::npos ||
        !std::string_view(response).starts_with("HTTP/1.1 200")) {
      return std::nullopt;
    }
    std::string headers = response.substr(0, header_end);
    std::transform(headers.begin(), headers.end(), headers.begin(), ::tolower);
    const std::string_view payload =
        std::string_view(response).substr(header_end + 4);
    if (headers.find("transfer-encoding: chunked") != std::string::npos) {
      return DecodeChunked(payload);
    }
    return std::string(payload);
  }

  std::optional<std::string> Complete(const Job& job) {
    const std::string prompt = BuildZenzPrompt(job.reading, job.left_context);
    // The output is never longer than the reading; leave room for the end.
    const size_t n_predict =
        std::min<size_t>(96, strings::CharsLen(job.reading) + 8);
    const std::string body = absl::StrCat(
        "{\"prompt\":\"", EscapeJsonString(prompt), "\",\"n_predict\":",
        n_predict,
        ",\"temperature\":0.0,\"top_k\":1,\"cache_prompt\":true,"
        "\"stop\":[\"</s>\"]}");
    const std::optional<std::string> response = Http("POST", "/completion", body);
    if (!response) {
      return std::nullopt;
    }
    return ExtractJsonStringField(*response, "content");
  }

  bool IsHealthy() {
    const std::optional<std::string> response = Http("GET", "/health", "");
    return response && response->find("\"ok\"") != std::string::npos;
  }

  // Kills a server left behind by a converter process that crashed.
  void KillStaleServer() {
    std::ifstream file(pid_path_);
    pid_t pid = 0;
    if (!(file >> pid) || pid <= 1) {
      return;
    }
    char path[PROC_PIDPATHINFO_MAXSIZE] = {};
    // Only kill it when the pid still belongs to our llama-server.
    if (::proc_pidpath(pid, path, sizeof(path)) > 0 && server_path_ == path) {
      ::kill(pid, SIGTERM);
    }
    ::unlink(pid_path_.c_str());
  }

  // Worker thread only.
  bool EnsureServer() {
    if (pid_ > 0) {
      int status = 0;
      if (::waitpid(pid_, &status, WNOHANG) == 0) {
        return true;  // Still running.
      }
      pid_ = -1;  // Exited and reaped.
    }
    const absl::Time now = absl::Now();
    if (now - last_spawn_ < kRespawnInterval) {
      return false;
    }
    last_spawn_ = now;
    if (::access(server_path_.c_str(), X_OK) != 0 ||
        ::access(model_path_.c_str(), R_OK) != 0) {
      return false;  // The runtime is not set up.
    }
    KillStaleServer();
    port_ = PickFreePort();
    if (port_ == 0) {
      return false;
    }

    const std::string port = absl::StrCat(port_);
    const std::vector<std::string> arguments = {
        server_path_, "-m",         model_path_, "--host",  kHost,
        "--port",     port,         "--parallel", "1",      "--ctx-size",
        "512",        "-ngl",       "99",         "--no-webui",
        "--no-slots", "--log-disable",
    };
    std::vector<char*> argv;
    for (const std::string& argument : arguments) {
      argv.push_back(const_cast<char*>(argument.c_str()));
    }
    argv.push_back(nullptr);

    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_addopen(&actions, STDIN_FILENO, "/dev/null",
                                     O_RDONLY, 0);
    posix_spawn_file_actions_addopen(&actions, STDOUT_FILENO, "/dev/null",
                                     O_WRONLY, 0);
    posix_spawn_file_actions_addopen(&actions, STDERR_FILENO, "/dev/null",
                                     O_WRONLY, 0);
    pid_t pid = -1;
    const int error = ::posix_spawn(&pid, server_path_.c_str(), &actions,
                                    nullptr, argv.data(), environ);
    posix_spawn_file_actions_destroy(&actions);
    if (error != 0) {
      LOG(ERROR) << "Cannot start llama-server: " << error;
      return false;
    }
    pid_ = pid;
    std::ofstream(pid_path_) << pid_;

    const absl::Time deadline = absl::Now() + kStartupTimeout;
    while (absl::Now() < deadline) {
      {
        std::lock_guard<std::mutex> lock(mutex_);
        if (shutdown_) {
          return false;
        }
      }
      int status = 0;
      if (::waitpid(pid_, &status, WNOHANG) == pid_) {
        pid_ = -1;  // Died while loading the model.
        ::unlink(pid_path_.c_str());
        return false;
      }
      if (IsHealthy()) {
        return true;
      }
      absl::SleepFor(absl::Milliseconds(50));
    }
    StopServer();
    return false;
  }

  void StopServer() {
    if (pid_ <= 0) {
      return;
    }
    ::kill(pid_, SIGTERM);
    int status = 0;
    ::waitpid(pid_, &status, 0);
    pid_ = -1;
    ::unlink(pid_path_.c_str());
  }

  const std::string server_path_;
  const std::string model_path_;
  const std::string pid_path_;

  std::mutex mutex_;
  std::condition_variable condition_;
  bool shutdown_ = false;
  bool warmed_up_ = false;
  bool has_request_ = false;
  uint32_t latest_id_ = 0;
  std::optional<Job> pending_;
  std::optional<std::pair<uint32_t, ZenzResult>> done_;
  int active_fd_ = -1;
  std::thread worker_;

  // Worker thread only, except StopServer() after the worker has joined.
  pid_t pid_ = -1;
  int port_ = 0;
  absl::Time last_spawn_ = absl::InfinitePast();
};

LlamaServerZenzClient* g_default_client = nullptr;

}  // namespace

ZenzClientInterface* CreateDefaultZenzClient() {
  const std::string directory =
      FileUtil::JoinPath(SystemUtil::GetUserProfileDirectory(), "zenz");
  g_default_client = new LlamaServerZenzClient(
      FileUtil::JoinPath(directory, "llama-server"),
      FileUtil::JoinPath(directory, "model.gguf"),
      FileUtil::JoinPath(directory, "llama-server.pid"));
  // Do not leave llama-server behind when the converter exits.
  std::atexit([] { g_default_client->Shutdown(); });
  return g_default_client;
}

}  // namespace zenz
}  // namespace mozc
