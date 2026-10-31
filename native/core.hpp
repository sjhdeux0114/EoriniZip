#pragma once
#include <filesystem>
#include <string>
#include <vector>
#include <atomic>
#include <mutex>
#include <stdexcept>
namespace kz {
namespace fs = std::filesystem;
class PasswordRequired : public std::runtime_error { public: using std::runtime_error::runtime_error; };
struct Entry { std::string name, modified; uint64_t size = 0, packed = 0; bool directory = false, encrypted = false, link = false; };
struct Options { std::string format = "zip", password, split; int level = 5; bool hideNames = false, unicodeNames=true, storeTimes=true, excludeThumbs=false, multiThread=true,selfExtracting=false; };
std::vector<Entry> parseListing(const std::string& text);
void validateEntries(const std::vector<Entry>& entries);
std::vector<std::string> createArgs(const Options&, const fs::path&, const std::vector<fs::path>&);
std::wstring widen(const std::string&);
std::string narrow(const std::wstring&);
// Canonical composition for names created on macOS (including decomposed Hangul).
std::string composeFilename(const std::string&);
std::wstring quoteWindows(const std::wstring&);
class Engine {
 public:
  explicit Engine(fs::path executable) : executable_(fs::absolute(executable)) {}
  void cancel();
  void configureRead(int codePage,bool parallel,bool zone) { codePage_=codePage;parallel_=parallel;propagateZone_=zone; }
  void configureTemporaryDirectory(const fs::path& path){if(!path.empty()&&!fs::is_directory(path))throw std::runtime_error("임시 폴더가 존재하지 않습니다.");temporaryDirectory_=path;}
  fs::path temporaryDirectory()const{return temporaryDirectory_.empty()?fs::temp_directory_path():temporaryDirectory_;}
  int progress() const { return progress_.load(); }
  int phase() const { return phase_.load(); }
  void resetCancel() { cancelled_ = false; }
  std::vector<Entry> list(const fs::path&, const std::string& password = "");
  void test(const fs::path&, const std::string& password = "");
  void edit(const fs::path&, const std::string& action, const std::vector<std::string>& names, const std::string& password = "");
  std::string comment(const fs::path&);
  void setComment(const fs::path&,const std::string&,const std::string& password="");
  std::string recoverPassword(const fs::path&,const std::vector<std::string>& candidates);
  fs::path repairZip(const fs::path&,const fs::path& parent);
  void scanFile(const fs::path& scanner,const fs::path& file){run({"-Scan","-ScanType","3","-File",fs::absolute(file).u8string(),"-DisableRemediation"},scanner,3);}
  fs::path create(const Options&, const fs::path&, const std::vector<fs::path>&);
  std::vector<fs::path> createEach(const std::vector<fs::path>&);
  std::vector<fs::path> extractEach(const std::vector<fs::path>&, const std::string& password = "");
  fs::path decryptEasyCrypt(const fs::path&, const fs::path&, const std::string&);
  fs::path createQuick(const std::vector<fs::path>&);
  fs::path extract(const fs::path&, const fs::path&, const std::string& password = "", const std::vector<std::string>& selected = {});
  fs::path extractSmart(const fs::path&, const fs::path&, const std::string& password = "", bool forceFolder = false);
 private:
  std::string run(const std::vector<std::string>&, const fs::path& program = {}, int operation = 0, const fs::path& workingDirectory = {});
  fs::path codec(const std::string&) const;
  fs::path executable_;
  fs::path temporaryDirectory_;
  int codePage_=0; bool parallel_=true,propagateZone_=true;
  std::atomic<bool> cancelled_{false};
  std::atomic<int> progress_{-1}, phase_{0};
  std::mutex processMutex_;
  intptr_t process_ = 0;
};
}
