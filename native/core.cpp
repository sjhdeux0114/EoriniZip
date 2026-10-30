#include "core.hpp"
#include "archive_name.hpp"
#include <algorithm>
#include <cctype>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <random>
#include <regex>
#include <fstream>
#include <codecvt>
#ifdef _WIN32
#include <windows.h>
#elif defined(__APPLE__)
#include <stdio.h>
#endif
namespace kz {
std::wstring widen(const std::string& s) { return std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>>{}.from_bytes(s); }
std::string narrow(const std::wstring& s) { return std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>>{}.to_bytes(s); }
std::wstring quoteWindows(const std::wstring& text) {
  std::wstring out = L"\""; size_t slashes = 0;
  for (wchar_t c : text) {
    if (c == L'\\') { ++slashes; continue; }
    out.append(slashes * (c == L'"' ? 2 : 1), L'\\'); slashes = 0;
    if (c == L'"') out += L'\\';
    out += c;
  }
  out.append(slashes * 2, L'\\'); return out + L'"';
}
static void passwordCheck(const std::string& p) {
  if (p.size() > 4096 || p.find_first_of(std::string("\0\r\n", 3)) != std::string::npos) throw std::runtime_error("Invalid password.");
}
std::vector<std::string> createArgs(const Options& o, const fs::path& output, const std::vector<fs::path>& inputs) {
  if (o.format != "zip" && o.format != "7z" && o.format != "tar") throw std::runtime_error("Unsupported creation format.");
  if (o.level < 0 || o.level > 9 || inputs.empty()) throw std::runtime_error("Choose input files and a valid compression level.");
  passwordCheck(o.password);
  if (!o.split.empty() && !std::regex_match(o.split, std::regex("[1-9][0-9]{0,12}[bBkKmMgG]"))) throw std::runtime_error("Split size must look like 100m or 2g.");
  if (o.format == "tar" && (!o.password.empty() || !o.split.empty() || o.hideNames)) throw std::runtime_error("TAR does not support encryption/split options.");
  if (o.hideNames && (o.format != "7z" || o.password.empty())) throw std::runtime_error("Filename encryption requires 7Z and a password.");
  std::vector<std::string> args = {"a", "-t" + o.format, "-mx=" + std::to_string(o.level), "-bsp1", "-y", "-sccUTF-8", "-spd"};
  if(o.format!="tar"){
    args.push_back(o.multiThread?"-mmt=on":"-mmt=off");
    args.push_back(o.storeTimes?"-mtc=on":"-mtc=off");args.push_back(o.storeTimes?"-mta=on":"-mta=off");
  }
  if(o.format=="zip")args.push_back(o.unicodeNames?"-mcu=on":"-mcu=off");
  if(o.excludeThumbs)args.push_back("-xr!Thumbs.db");
  if (!o.password.empty()) { args.push_back("-p" + o.password); if (o.format == "zip") args.push_back("-mem=AES256"); }
  if (o.hideNames) args.push_back("-mhe=on");
  if (!o.split.empty()) args.push_back("-v" + o.split);
  args.push_back("--"); args.push_back(output.u8string());
  for (const auto& input : inputs) args.push_back(fs::absolute(input).u8string());
  return args;
}
std::vector<Entry> parseListing(const std::string& text) {
  std::vector<Entry> entries; std::map<std::string,std::string> fields;
  auto flush = [&] {
    if (fields.count("Path")) {
      Entry e; e.name = fields["Path"]; e.modified = fields["Modified"];
      auto number = [&](const char* key) -> uint64_t {
        const auto& v = fields[key]; if (v.empty()) return 0;
        if (!std::all_of(v.begin(), v.end(), [](unsigned char c){ return std::isdigit(c); })) throw std::runtime_error("Invalid entry size.");
        return std::stoull(v);
      };
      e.size = number("Size"); e.packed = number("Packed Size");
      const auto& a = fields["Attributes"];
      e.directory = fields["Folder"] == "+" || (!a.empty() && a[0] == 'D');
      e.encrypted = fields["Encrypted"] == "+";
      e.link = !fields["Symbolic Link"].empty() || !fields["Hard Link"].empty() || std::regex_search(a + " " + fields["Mode"], std::regex("(^| )l[rwxstST-]{9}"));
      entries.push_back(e);
    } fields.clear();
  };
  std::istringstream stream(text); std::string line;
  while (std::getline(stream, line)) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line.empty()) { flush(); continue; }
    auto pos = line.find(" = "); if (pos != std::string::npos) fields[line.substr(0, pos)] = line.substr(pos + 3);
  }
  flush(); return entries;
}
void validateEntries(const std::vector<Entry>& entries) {
  if (entries.size() > 100000) throw std::runtime_error("Safety limit: more than 100,000 entries.");
  uint64_t total = 0; std::set<std::string> seen;
  for (const auto& e : entries) {
    std::string name = e.name; std::replace(name.begin(), name.end(), '\\', '/');
    if (name.empty() || name[0] == '/' || name.find(':') != std::string::npos || e.link) throw std::runtime_error("Unsafe path or link in archive.");
    for (unsigned char c : name) if (c < 32) throw std::runtime_error("Control character in path.");
    if (name.back() == '/') name.pop_back();
    std::istringstream parts(name); std::string part;
    while (std::getline(parts, part, '/')) {
      if (part.empty() || part == "." || part == ".." || part.back() == '.' || part.back() == ' ' || std::regex_match(part, std::regex("(con|prn|aux|nul|com[0-9]|lpt[0-9])(\\..*)?", std::regex::icase))) throw std::runtime_error("Unsafe archive path.");
    }
    std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
    if (!seen.insert(name).second) throw std::runtime_error("Duplicate archive path.");
    constexpr uint64_t limit = 20ULL * 1024 * 1024 * 1024;
    if (e.size > limit - total) throw std::runtime_error("Safety limit: expanded data exceeds 20 GiB.");
    total += e.size;
  }
}
static fs::path freshDirectory(const fs::path& parent) {
  std::random_device random;
  for (int i = 0; i < 32; ++i) {
    auto p = parent / (".eorinizip-tmp-" + std::to_string(random()) + "-" + std::to_string(random()));
    if (fs::create_directory(p)) {
#ifdef _WIN32
      SetFileAttributesW(p.c_str(),FILE_ATTRIBUTE_HIDDEN);
#endif
      return p;
    }
  }
  throw std::runtime_error("Cannot create output directory.");
}
std::vector<Entry> Engine::list(const fs::path& archive, const std::string& password) {
  auto ext = archive.extension().u8string(); std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c){return static_cast<char>(std::tolower(c));});
  if (ext == ".lock" || ext == ".ezc") throw std::runtime_error("AESLocker / EasyCrypt decryption is planned; compatibility is not implemented yet.");
  if (ext == ".alz" || ext == ".egg" || ext == ".zpaq") throw std::runtime_error("ALZ / EGG / ZPAQ adapters are not implemented yet.");
  passwordCheck(password);
  std::vector<std::string> args = {"l", "-slt", "-ba", "-sccUTF-8"};
  if (!password.empty()) args.push_back("-p" + password);
  if(codePage_&&ext==".zip"&&std::find(args.begin(),args.end(),"-mcp="+std::to_string(codePage_))==args.end())args.push_back("-mcp="+std::to_string(codePage_));
  args.insert(args.end(), {"--", fs::absolute(archive).u8string()}); return parseListing(run(args));
}
void Engine::test(const fs::path& archive, const std::string& password) {
  passwordCheck(password); std::vector<std::string> args = {"t", "-bsp1", "-sccUTF-8"};
  if (!password.empty()) args.push_back("-p" + password);
  args.insert(args.end(), {"--", fs::absolute(archive).u8string()}); run(args);
}
static void moveExclusive(const fs::path& from,const fs::path& to) {
#ifdef _WIN32
  if(!MoveFileExW(from.c_str(),to.c_str(),0))throw fs::filesystem_error("Move output",from,to,std::error_code(GetLastError(),std::system_category()));
#elif defined(__APPLE__)
  if(renamex_np(from.c_str(),to.c_str(),RENAME_EXCL))throw fs::filesystem_error("Move output",from,to,std::error_code(errno,std::generic_category()));
#else
  if(fs::exists(to))throw std::runtime_error("Output already exists");
  fs::rename(from,to);
#endif
}
static fs::path numbered(const fs::path& desired,unsigned number) {
  if(!number)return desired;
  return desired.parent_path()/fs::u8path(desired.stem().u8string()+" ("+std::to_string(number)+")"+desired.extension().u8string());
}
fs::path Engine::create(const Options& o, const fs::path& output, const std::vector<fs::path>& inputs) {
  createArgs(o,output,inputs);
  const auto requested=fs::absolute(output);
  for(const auto& input:inputs)if(fs::is_directory(input)){
    auto relative=fs::weakly_canonical(requested.parent_path()).lexically_relative(fs::weakly_canonical(input));
    if(relative.empty()||(*relative.begin()!=fs::path("..")&&!relative.is_absolute()))throw std::runtime_error("압축 대상 폴더 밖에 결과 파일을 저장하세요.");
  }
  // Staging stays on the same volume so publishing is a move, not another full copy.
  const auto staging=freshDirectory(requested.parent_path());
  std::vector<std::pair<fs::path,fs::path>> moved;
  try {
    run(createArgs(o,staging/requested.filename(),inputs));
    std::vector<fs::path> volumes;for(const auto& entry:fs::directory_iterator(staging))volumes.push_back(entry.path());
    std::sort(volumes.begin(),volumes.end());
    if(volumes.empty())throw std::runtime_error("No archive was produced.");
    fs::path chosen;
    for(unsigned n=0;n<10000;++n){
      auto candidate=numbered(requested,n);bool available=!fs::exists(candidate);
      for(const auto& volume:volumes){
        auto suffix=volume.filename().u8string().substr(requested.filename().u8string().size());
        auto target=fs::u8path(candidate.u8string()+suffix);
        if(fs::exists(target))available=false;
      }
      if(available){chosen=candidate;break;}
    }
    if(chosen.empty())throw std::runtime_error("Cannot find an unused archive name.");
    for(const auto& volume:volumes){
      if(cancelled_)throw std::runtime_error("Operation cancelled.");
      auto target=fs::u8path(chosen.u8string()+volume.filename().u8string().substr(requested.filename().u8string().size()));
      moveExclusive(volume,target);moved.emplace_back(target,volume);
    }
    std::error_code ec;fs::remove(staging,ec);
    return o.split.empty()?chosen:fs::u8path(chosen.u8string()+".001");
  }catch(...){
    for(auto it=moved.rbegin();it!=moved.rend();++it){try{moveExclusive(it->first,it->second);}catch(...){}}
    std::error_code ec;fs::remove_all(staging,ec);throw;
  }
}
fs::path Engine::createQuick(const std::vector<fs::path>& inputs) {
  if(inputs.empty())throw std::runtime_error("압축할 파일이 없습니다.");
  Options options;options.level=1;options.multiThread=parallel_;
  return create(options,suggestedArchive(inputs),inputs);
}
fs::path Engine::extract(const fs::path& archive,const fs::path& parent,const std::string& password) {
  auto entries=list(archive,password);validateEntries(entries);
  if(!fs::is_directory(parent))throw std::runtime_error("대상 폴더가 존재하지 않습니다.");
  // Existing top-level names are rejected before extraction; originals remain intact.
  for(const auto& entry:entries){
    auto name=entry.name;std::replace(name.begin(),name.end(),'\\','/');
    auto top=name.substr(0,name.find('/'));
    if(fs::exists(parent/fs::u8path(top)))throw std::runtime_error("대상에 같은 이름이 있습니다. 빈 폴더를 선택하세요: "+top);
  }
  auto staging=freshDirectory(parent);
  std::vector<std::pair<fs::path,fs::path>> moved;
  try {
    std::vector<std::string> args={"x","-bsp1",parallel_?"-mmt=on":"-mmt=off","-y","-sccUTF-8","-o"+staging.u8string()};
    auto ext=archive.extension().u8string();std::transform(ext.begin(),ext.end(),ext.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
    if(codePage_&&ext==".zip")args.push_back("-mcp="+std::to_string(codePage_));
    if(!password.empty())args.push_back("-p"+password);
    args.insert(args.end(),{"--",fs::absolute(archive).u8string()});run(args);
#ifdef _WIN32
    if(propagateZone_){
      std::ifstream zone(fs::path(archive.wstring()+L":Zone.Identifier"),std::ios::binary);
      if(zone){
        std::string metadata;char buffer[4096];while(zone.read(buffer,sizeof(buffer))||zone.gcount()){metadata.append(buffer,static_cast<size_t>(zone.gcount()));if(metadata.size()>65536)throw std::runtime_error("다운로드 출처 정보가 너무 큽니다.");}
        for(const auto& entry:fs::recursive_directory_iterator(staging))if(entry.is_regular_file()){
          std::ofstream output(fs::path(entry.path().wstring()+L":Zone.Identifier"),std::ios::binary);
          output.write(metadata.data(),static_cast<std::streamsize>(metadata.size()));
          if(!output)throw std::runtime_error("다운로드 출처 정보를 저장할 수 없습니다.");
        }
      }
    }
#endif
    std::vector<fs::path> children;for(const auto& entry:fs::directory_iterator(staging))children.push_back(entry.path());
    for(const auto& child:children){
      if(cancelled_)throw std::runtime_error("Operation cancelled.");
      auto target=parent/child.filename();moveExclusive(child,target);moved.emplace_back(target,child);
    }
    std::error_code ec;fs::remove(staging,ec);return parent;
  }catch(...){
    for(auto it=moved.rbegin();it!=moved.rend();++it){try{moveExclusive(it->first,it->second);}catch(...){}}
    std::error_code ec;fs::remove_all(staging,ec);throw;
  }
}
}