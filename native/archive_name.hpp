#pragma once
#include <filesystem>
#include <vector>
#include <string>
namespace kz {
inline std::filesystem::path suggestedArchive(const std::vector<std::filesystem::path>& inputs, const std::string& format="zip") {
  namespace fs=std::filesystem;
  if(inputs.empty())return fs::path("Archive."+format);
  auto first=fs::absolute(inputs.front()).lexically_normal();
  if(first.filename().empty())first=first.parent_path();
  auto parent=first.parent_path();
  auto name=inputs.size()==1?(fs::is_directory(first)?first.filename():first.stem()):parent.filename();
  if(name.empty())name=fs::path("Archive");
  name+=fs::path("."+format);
  return parent/name;
}
}
