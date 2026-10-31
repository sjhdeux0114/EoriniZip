#pragma once
#include <windows.h>
#include <wincred.h>
#include <filesystem>
#include <string>
#include <stdexcept>
namespace kz::win {
inline std::wstring passwordTarget(const std::filesystem::path& archive){return L"EoriniZip:"+std::filesystem::absolute(archive).lexically_normal().wstring();}
inline void saveArchivePassword(const std::filesystem::path& archive,const std::string& value){
  if(value.empty()||value.size()>CRED_MAX_CREDENTIAL_BLOB_SIZE)throw std::runtime_error("저장할 암호가 없거나 Windows 자격 증명 크기 한도를 초과했습니다.");
  auto target=passwordTarget(archive);CREDENTIALW credential{};credential.Type=CRED_TYPE_GENERIC;credential.TargetName=target.data();
  credential.CredentialBlobSize=static_cast<DWORD>(value.size());credential.CredentialBlob=reinterpret_cast<LPBYTE>(const_cast<char*>(value.data()));
  credential.Persist=CRED_PERSIST_LOCAL_MACHINE;credential.UserName=const_cast<wchar_t*>(L"EoriniZip archive password");
  if(!CredWriteW(&credential,0))throw std::runtime_error("Windows 자격 증명 관리자에 암호를 저장할 수 없습니다.");
}
inline std::string loadArchivePassword(const std::filesystem::path& archive){
  auto target=passwordTarget(archive);PCREDENTIALW credential=nullptr;
  if(!CredReadW(target.c_str(),CRED_TYPE_GENERIC,0,&credential))throw std::runtime_error("이 압축 파일에 저장한 암호가 없습니다.");
  try{std::string value(reinterpret_cast<const char*>(credential->CredentialBlob),credential->CredentialBlobSize);CredFree(credential);return value;}catch(...){CredFree(credential);throw;}
}
inline void deleteArchivePassword(const std::filesystem::path& archive){
  auto target=passwordTarget(archive);if(!CredDeleteW(target.c_str(),CRED_TYPE_GENERIC,0)&&GetLastError()!=ERROR_NOT_FOUND)throw std::runtime_error("저장한 암호를 삭제할 수 없습니다.");
}
}
