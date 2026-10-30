#pragma once
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <stdexcept>
#include <filesystem>
#include <string>
#include <vector>
namespace kz::win {
inline std::wstring executablePath(){
  wchar_t path[32768];auto n=GetModuleFileNameW(nullptr,path,32768);
  if(!n||n==32768)throw std::runtime_error("실행 파일 경로를 읽을 수 없습니다.");return path;
}
inline void registryString(const std::wstring& path,const wchar_t* name,const std::wstring& value){
  HKEY key=nullptr;
  auto status=RegCreateKeyExW(HKEY_CURRENT_USER,path.c_str(),0,nullptr,0,KEY_SET_VALUE,nullptr,&key,nullptr);
  if(status!=ERROR_SUCCESS)throw std::runtime_error("사용자 설정을 등록할 수 없습니다.");
  status=RegSetValueExW(key,name,0,REG_SZ,reinterpret_cast<const BYTE*>(value.c_str()),static_cast<DWORD>((value.size()+1)*sizeof(wchar_t)));
  RegCloseKey(key);if(status!=ERROR_SUCCESS)throw std::runtime_error("사용자 설정을 저장할 수 없습니다.");
}
inline const std::vector<std::wstring>& extensions(){
  static const std::vector<std::wstring> list={L".zip",L".7z",L".rar",L".tar",L".gz",L".bz2",L".xz",L".cab",L".iso",L".wim",L".001",L".alz",L".egg",L".zpaq",L".ezc"};return list;
}
inline bool associated(const std::wstring& ext){
  DWORD size=0;return RegGetValueW(HKEY_CURRENT_USER,(L"Software\\Classes\\"+ext+L"\\OpenWithProgids").c_str(),L"EoriniZip.Archive",RRF_RT_ANY,nullptr,nullptr,&size)==ERROR_SUCCESS;
}
inline void associations(const std::vector<bool>& selected){
  const auto exe=executablePath();
  registryString(L"Software\\Classes\\EoriniZip.Archive",nullptr,L"어린이zip 압축 파일");
  registryString(L"Software\\Classes\\EoriniZip.Archive\\DefaultIcon",nullptr,L"\""+exe+L"\",0");
  registryString(L"Software\\Classes\\EoriniZip.Archive\\shell\\open\\command",nullptr,L"\""+exe+L"\" --open \"%1\"");
  registryString(L"Software\\EoriniZip\\Capabilities",L"ApplicationName",L"어린이zip");
  registryString(L"Software\\EoriniZip\\Capabilities",L"ApplicationDescription",L"압축 파일 열기 및 해제");
  registryString(L"Software\\RegisteredApplications",L"EoriniZip",L"Software\\EoriniZip\\Capabilities");
  for(size_t i=0;i<extensions().size();++i){
    auto path=L"Software\\Classes\\"+extensions()[i]+L"\\OpenWithProgids";HKEY key;
    if(selected.at(i)){
      registryString(path,L"EoriniZip.Archive",L"");
      registryString(L"Software\\EoriniZip\\Capabilities\\FileAssociations",extensions()[i].c_str(),L"EoriniZip.Archive");
    }else{
      if(RegOpenKeyExW(HKEY_CURRENT_USER,path.c_str(),0,KEY_SET_VALUE,&key)==ERROR_SUCCESS){RegDeleteValueW(key,L"EoriniZip.Archive");RegCloseKey(key);}
      if(RegOpenKeyExW(HKEY_CURRENT_USER,L"Software\\EoriniZip\\Capabilities\\FileAssociations",0,KEY_SET_VALUE,&key)==ERROR_SUCCESS){RegDeleteValueW(key,extensions()[i].c_str());RegCloseKey(key);}
    }
  }
  SHChangeNotify(SHCNE_ASSOCCHANGED,SHCNF_IDLIST,nullptr,nullptr);
}
inline void shellRegistration(bool install){
  const std::wstring clsid=L"{5B30EB48-528D-446B-B18E-BB516C5B902F}";
  const auto exe=std::filesystem::path(executablePath());
  const auto dll=exe.parent_path()/L"EoriniZipShell_v3.dll";
  const wchar_t* handlers[]={L"Software\\Classes\\*\\shellex\\ContextMenuHandlers\\EoriniZip",L"Software\\Classes\\Directory\\shellex\\ContextMenuHandlers\\EoriniZip"};
  if(install){
    if(!std::filesystem::is_regular_file(dll))throw std::runtime_error("탐색기 확장 DLL이 실행 파일 옆에 필요합니다.");
    registryString(L"Software\\Classes\\CLSID\\"+clsid+L"\\InprocServer32",nullptr,dll.wstring());
    registryString(L"Software\\Classes\\CLSID\\"+clsid+L"\\InprocServer32",L"ThreadingModel",L"Apartment");
    for(auto path:handlers)registryString(path,nullptr,clsid);
  }else for(auto path:handlers)RegDeleteTreeW(HKEY_CURRENT_USER,path);
  for(const auto& ext:extensions()){
    for(const auto& verb:{std::wstring(L"Open"),std::wstring(L"Extract"),std::wstring(L"ExtractHere"),std::wstring(L"ExtractSmart")}){
      const auto path=L"Software\\Classes\\SystemFileAssociations\\"+ext+L"\\shell\\EoriniZip."+verb;
      if(install){
        registryString(path,nullptr,verb==L"Open"?L"어린이zip으로 압축 풀기":verb==L"ExtractHere"?L"여기에 풀기 (어린이zip)":verb==L"ExtractSmart"?L"알아서 풀기 (어린이zip)":L"어린이zip으로 새 폴더에 풀기");
        registryString(path,L"Icon",L"\""+exe.wstring()+L"\",0");
        registryString(path,L"MultiSelectModel",L"Single");
        registryString(path+L"\\command",nullptr,L"\""+exe.wstring()+L"\" "+(verb==L"Open"?L"--extract-here":verb==L"ExtractHere"?L"--extract-here":verb==L"ExtractSmart"?L"--extract-smart":L"--extract-folder")+L" \"%1\"");
      }else RegDeleteTreeW(HKEY_CURRENT_USER,path.c_str());
    }
  }  SHChangeNotify(SHCNE_ASSOCCHANGED,SHCNF_IDLIST,nullptr,nullptr);
}
}
