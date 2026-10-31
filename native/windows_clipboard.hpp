#pragma once
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <filesystem>
#include <vector>
#include <string>
#include <stdexcept>
namespace kz::win {
inline HGLOBAL makeFilesDrop(const std::vector<std::filesystem::path>& files){
  if(files.empty())throw std::runtime_error("복사할 파일이 없습니다.");
  std::wstring paths;
  for(const auto& path:files){paths+=path.wstring();paths.push_back(L'\0');}paths.push_back(L'\0');
  auto memory=GlobalAlloc(GMEM_MOVEABLE|GMEM_ZEROINIT,sizeof(DROPFILES)+paths.size()*sizeof(wchar_t));
  if(!memory)throw std::bad_alloc();
  auto drop=static_cast<DROPFILES*>(GlobalLock(memory));
  if(!drop){GlobalFree(memory);throw std::runtime_error("클립보드 메모리를 열 수 없습니다.");}
  drop->pFiles=sizeof(DROPFILES);drop->fWide=TRUE;
  memcpy(reinterpret_cast<BYTE*>(drop)+sizeof(DROPFILES),paths.data(),paths.size()*sizeof(wchar_t));GlobalUnlock(memory);
  return memory;
}
inline std::vector<std::filesystem::path> readFilesDrop(HDROP drop){
  if(!drop)throw std::runtime_error("클립보드에 탐색기에서 복사한 파일이 없습니다.");
  std::vector<std::filesystem::path> result;
  auto count=DragQueryFileW(drop,0xffffffff,nullptr,0);if(count>100000)throw std::runtime_error("클립보드 파일이 너무 많습니다.");
  for(UINT i=0;i<count;++i){auto length=DragQueryFileW(drop,i,nullptr,0);std::wstring path(length+1,L'\0');
    if(!length||DragQueryFileW(drop,i,path.data(),length+1)!=length)throw std::runtime_error("클립보드 경로를 읽을 수 없습니다.");
    path.resize(length);result.emplace_back(path);
  }
  return result;
}
inline void copyFilesClipboard(HWND owner,const std::vector<std::filesystem::path>& files){
  auto memory=makeFilesDrop(files);
  if(!OpenClipboard(owner)){GlobalFree(memory);throw std::runtime_error("클립보드를 열 수 없습니다.");}
  bool ok=EmptyClipboard()&&SetClipboardData(CF_HDROP,memory);CloseClipboard();
  if(!ok){GlobalFree(memory);throw std::runtime_error("클립보드 복사에 실패했습니다.");}
}
inline std::vector<std::filesystem::path> clipboardFiles(HWND owner){
  if(!OpenClipboard(owner))throw std::runtime_error("클립보드를 열 수 없습니다.");
  std::vector<std::filesystem::path> result;
  try{
    auto drop=static_cast<HDROP>(GetClipboardData(CF_HDROP));
    result=readFilesDrop(drop);
    CloseClipboard();return result;
  }catch(...){CloseClipboard();throw;}
}
}
