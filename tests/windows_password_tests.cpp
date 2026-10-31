#include <windows.h>
#include "windows_passwords.hpp"
#include <iostream>
int main(){
  auto archive=std::filesystem::temp_directory_path()/(L"EoriniZip-credential-test-"+std::to_wstring(GetCurrentProcessId())+L".zip");
  try{
    const std::string first=u8"한글-test-password-051";
    kz::win::saveArchivePassword(archive,first);
    if(kz::win::loadArchivePassword(archive)!=first)throw std::runtime_error("Credential Unicode roundtrip failed");
    kz::win::saveArchivePassword(archive,"replacement-test-password");
    if(kz::win::loadArchivePassword(archive)!="replacement-test-password")throw std::runtime_error("Credential update failed");
    kz::win::deleteArchivePassword(archive);
    bool missing=false;try{kz::win::loadArchivePassword(archive);}catch(const std::exception&){missing=true;}
    if(!missing)throw std::runtime_error("Credential deletion failed");
    std::cout<<"Current-user Windows credential save/load/update/delete passed\n";return 0;
  }catch(const std::exception& error){try{kz::win::deleteArchivePassword(archive);}catch(...){}std::cerr<<error.what()<<"\n";return 1;}
}
