#pragma once
#include <filesystem>
#include <vector>
#include <string>
namespace kz {
inline std::filesystem::path suggestedArchive(const std::vector<std::filesystem::path>& inputs, const std::string& format="zip") {
  namespace fs=std::filesystem;
  const auto extension=format=="gzip"?std::string("gz"):format=="bzip2"?std::string("bz2"):format;
  if(inputs.empty())return fs::path("Archive."+extension);
  auto first=fs::absolute(inputs.front()).lexically_normal();
  if(first.filename().empty())first=first.parent_path();
  auto parent=first.parent_path();
  auto name=inputs.size()==1?(fs::is_directory(first)?first.filename():first.stem()):parent.filename();
  if(name.empty())name=fs::path("Archive");
  name+=fs::path("."+extension);
  return parent/name;
}
}
