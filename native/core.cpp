#include "core.hpp"
#include <algorithm>
#include <cctype>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <random>
#include <regex>
#include <codecvt>
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
  std::vector<std::string> args = {"a", "-t" + o.format, "-mx=" + std::to_string(o.level), "-bd", "-y", "-sccUTF-8", "-spd"};
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
    auto p = parent / ("EoriniZip-" + std::to_string(random()) + "-" + std::to_string(random()));
    if (fs::create_directory(p)) return p;
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
  args.insert(args.end(), {"--", fs::absolute(archive).u8string()}); return parseListing(run(args));
}
void Engine::test(const fs::path& archive, const std::string& password) {
  passwordCheck(password); std::vector<std::string> args = {"t", "-bd", "-sccUTF-8"};
  if (!password.empty()) args.push_back("-p" + password);
  args.insert(args.end(), {"--", fs::absolute(archive).u8string()}); run(args);
}
fs::path Engine::create(const Options& o, const fs::path& output, const std::vector<fs::path>& inputs) {
  createArgs(o, output, inputs);
  const auto destination = freshDirectory(fs::absolute(output).parent_path());
  try { run(createArgs(o, destination / output.filename(), inputs)); return destination; }
  catch (...) { std::error_code ec; fs::remove_all(destination, ec); throw; }
}
fs::path Engine::extract(const fs::path& archive, const fs::path& parent, const std::string& password) {
  validateEntries(list(archive, password));
  auto destination = freshDirectory(parent);
  try {
    std::vector<std::string> args = {"x", "-bd", "-y", "-sccUTF-8", "-o" + destination.u8string()};
    if (!password.empty()) args.push_back("-p" + password);
    args.insert(args.end(), {"--", fs::absolute(archive).u8string()}); run(args); return destination;
  } catch (...) { std::error_code ec; fs::remove_all(destination, ec); throw; }
}
}
