#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <stdexcept>
namespace kz {
struct SplitPreset { const char* label; uint64_t bytes; };
inline constexpr std::array<SplitPreset, 21> splitPresets{{
  {"분할하지 않음", 0},
  {"5MB", 5ULL * 1024 * 1024},
  {"네이버 블로그(10MB)", 10ULL * 1024 * 1024},
  {"다음 블로그(10MB)", 10ULL * 1024 * 1024},
  {"다음 메일(25MB)", 25ULL * 1024 * 1024},
  {"티스토리(20MB)", 20ULL * 1024 * 1024},
  {"공직자통합메일(20MB)", 20ULL * 1024 * 1024},
  {"네이버 카페(50MB)", 50ULL * 1024 * 1024},
  {"25MB (Gmail)", 25ULL * 1024 * 1024},
  {"100MB", 100ULL * 1024 * 1024},
  {"300MB", 300ULL * 1024 * 1024},
  {"650MB (CD)", 650ULL * 1024 * 1024},
  {"700MB (CD)", 700ULL * 1024 * 1024},
  {"네이버 메일(1.95GB)", 2093796556ULL},
  {"다음 메일(4GB)", 4ULL * 1024 * 1024 * 1024},
  {"카카오톡(300MB)", 300ULL * 1024 * 1024},
  {"1GB", 1ULL * 1024 * 1024 * 1024},
  {"2GB", 2ULL * 1024 * 1024 * 1024},
  {"4GB (FAT32)", 4294967295ULL},
  {"4.7GB (DVD)", 4700000000ULL},
  {"직접 입력", 0}
}};
inline constexpr size_t customSplitIndex = splitPresets.size() - 1;
inline std::string splitArgument(size_t index, const std::string& custom = "") {
  if (index >= splitPresets.size()) throw std::runtime_error("분할 크기를 선택하세요.");
  if (index == customSplitIndex) {
    if (custom.empty()) throw std::runtime_error("직접 입력할 분할 크기가 필요합니다. 예: 100m, 2g");
    return custom;
  }
  return index == 0 ? "" : std::to_string(splitPresets[index].bytes) + "b";
}
}
