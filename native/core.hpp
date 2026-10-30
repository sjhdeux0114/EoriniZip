#pragma once
#include <filesystem>
#include <string>
#include <vector>
#include <atomic>
#include <mutex>
namespace kz {
namespace fs = std::filesystem;
struct Entry { std::string name, modified; uint64_t size = 0, packed = 0; bool directory = false, encrypted = false, link = false; };
struct Options { std::string format = "zip", password, split; int level = 5; bool hideNames = false; };
std::vector<Entry> parseListing(const std::string& text);
void validateEntries(const std::vector<Entry>& entries);
std::vector<std::string> createArgs(const Options&, const fs::path&, const std::vector<fs::path>&);
std::wstring widen(const std::string&);
std::string narrow(const std::wstring&);
std::wstring quoteWindows(const std::wstring&);
class Engine {
 public:
  explicit Engine(fs::path executable) : executable_(std::move(executable)) {}
  void cancel();
  void resetCancel() { cancelled_ = false; }
  std::vector<Entry> list(const fs::path&, const std::string& password = "");
  void test(const fs::path&, const std::string& password = "");
  fs::path create(const Options&, const fs::path&, const std::vector<fs::path>&);
  fs::path extract(const fs::path&, const fs::path&, const std::string& password = "");
 private:
  std::string run(const std::vector<std::string>&);
  fs::path executable_;
  std::atomic<bool> cancelled_{false};
  std::mutex processMutex_;
  intptr_t process_ = 0;
};
}
