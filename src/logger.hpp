#pragma once

#include <_types/_uint8_t.h>
#include <chrono>
#include <deque>
#include <fstream>
#include <iostream>
#include <map>
#include <mutex>
#include <ostream>
#include <queue>
#include <stack>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace Logger {

namespace {
inline std::ostream &operator<<(std::ostream &os, uint8_t t) { return os << static_cast<int>(t); }
template <typename T> std::ostream &logIteratorable(std::ostream &os, T const &vec) {
  os << '[';
  for (auto it = vec.cbegin(); it != vec.cend(); it++) {
    os << *it;
    if (it != vec.cend() - 1) { os << ","; }
  }
  return os << ']';
}
template <typename T> std::ostream &operator<<(std::ostream &os, std::vector<T> const &vec) {
  return logIteratorable(os, vec);
}
template <typename T> std::ostream &operator<<(std::ostream &os, std::stack<T> const &vec) {
  return logIteratorable(os, vec);
}
template <typename T> std::ostream &operator<<(std::ostream &os, std::queue<T> const &vec) {
  return logIteratorable(os, vec);
}
template <typename T> std::ostream &operator<<(std::ostream &os, std::unordered_set<T> const &vec) {
  return logIteratorable(os, vec);
}
template <typename T> std::ostream &operator<<(std::ostream &os, std::deque<T> const &vec) {
  return logIteratorable(os, vec);
}

template <typename T1, typename T2> std::ostream &operator<<(std::ostream &os, std::pair<T1, T2> const &pair) {
  return os << pair.first << ':' << pair.second;
}
template <typename T> std::ostream &logKeyValues(std::ostream &os, T const &map) {
  os << '{';
  std::size_t size = map.size();
  for (auto it = map.cbegin(); it != map.cend(); it++) {
    os << (*it);
    size--;
    if (size != 0) { os << ","; }
  }
  return os << '}';
}
template <typename K, typename V> std::ostream &operator<<(std::ostream &os, std::map<K, V> const &map) {
  return logKeyValues(os, map);
}
template <typename K, typename V> std::ostream &operator<<(std::ostream &os, std::unordered_map<K, V> const &map) {
  return logKeyValues(os, map);
}

class Logger {
public:
  static Logger &getInstance() noexcept {
    static Logger stream{};
    return stream;
  }
  void setOutput(const char *path) {
    logger_ = new std::ofstream(path);
    if (logger_->bad()) { throw std::runtime_error("cannot open file"); }
  }

  template <typename... Args> void log(Args &&...args) {
    std::lock_guard<std::mutex> lo{log_mutex};
    auto timestamp = static_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch());
    (*logger_) << '[' << timestamp.count() << ']';
    _log(std::forward<Args>(args)...);
  }

private:
  Logger() noexcept { logger_ = &std::cout; }
  Logger(Logger const &) = delete;
  Logger(Logger &&) = delete;
  Logger &operator=(Logger const &) = delete;
  Logger &operator=(Logger &&) = delete;
  ~Logger() {
    if (logger_ != &std::cout) { delete logger_; }
  }

private:
  std::ostream *logger_;

  std::mutex log_mutex;

  void _log() { (*logger_) << "\n"; }
  template <typename T, typename... Args> void _log(T &&first, Args &&...args) {
    (*logger_) << std::forward<T>(first) << " ";
    _log(std::forward<Args>(args)...);
  }
};

}; // namespace

inline void setLoggerPostion(const char *path) { Logger::getInstance().setOutput(path); }

template <typename... Args> void logDebug(Args &&...args) { Logger::getInstance().log("[DEBUG]", args...); }
template <typename... Args> void logInfo(Args &&...args) { Logger::getInstance().log("[INFO]", args...); }
template <typename... Args> void logError(Args &&...args) { Logger::getInstance().log("[ERROR]", args...); }
template <typename... Args> void logWarning(Args &&...args) { Logger::getInstance().log("[WARNING]", args...); }

}; // namespace Logger
