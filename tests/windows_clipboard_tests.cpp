#include <windows.h>
#include "windows_clipboard.hpp"
#include <iostream>
int main(int argc,char**){
  if(argc==1){
    try{
      const std::vector<std::filesystem::path> paths={L"C:\\한글\\😀.txt",L"C:\\folder with spaces",L"C:\\"+std::wstring(300,L'x')};
      for(const auto& input:{paths,std::vector<std::filesystem::path>{paths[0]}}){
        auto memory=kz::win::makeFilesDrop(input);
        std::vector<std::filesystem::path> actual;
        try{actual=kz::win::readFilesDrop(static_cast<HDROP>(memory));}catch(...){GlobalFree(memory);throw;}
        GlobalFree(memory);
        if(actual!=input)throw std::runtime_error("CF_HDROP Unicode/long path roundtrip failed");
      }
      bool rejected=false;try{auto memory=kz::win::makeFilesDrop({});GlobalFree(memory);}catch(const std::runtime_error&){rejected=true;}
      if(!rejected)throw std::runtime_error("Empty file selection accepted");
      std::cout<<"CF_HDROP Unicode, long path, multiple files and empty selection tests passed\n";return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
  }
  // A private window station has its own clipboard. Never replace the user's clipboard.
  auto originalStation=GetProcessWindowStation();auto originalDesktop=GetThreadDesktop(GetCurrentThreadId());
  // Named stations require administrator membership. CREATE_ONLY also prevents
  // accidentally opening an existing station and replacing its clipboard.
  HWINSTA station=CreateWindowStationW(nullptr,CWF_CREATE_ONLY,WINSTA_ALL_ACCESS,nullptr);HDESK desktop=nullptr;HWND owner=nullptr;
  if(!station){
    auto error=GetLastError();
    if(error==ERROR_ACCESS_DENIED||error==ERROR_ALREADY_EXISTS){
      // Never reuse an existing station: another process could own its clipboard.
      std::cout<<"SKIP: private clipboard station unavailable (Windows error "<<error<<")\n";return 77;
    }
  }
  auto cleanup=[&]{if(owner)DestroyWindow(owner);SetThreadDesktop(originalDesktop);if(desktop)CloseDesktop(desktop);SetProcessWindowStation(originalStation);if(station)CloseWindowStation(station);};
  try{
    if(!station)throw std::runtime_error("CreateWindowStationW failed: "+std::to_string(GetLastError()));
    if(!SetProcessWindowStation(station))throw std::runtime_error("SetProcessWindowStation failed: "+std::to_string(GetLastError()));
    desktop=CreateDesktopW(L"Default",nullptr,nullptr,0,DESKTOP_CREATEWINDOW|DESKTOP_READOBJECTS|DESKTOP_WRITEOBJECTS,nullptr);
    if(!desktop||!SetThreadDesktop(desktop))throw std::runtime_error("Cannot create isolated clipboard desktop");
    owner=CreateWindowExW(0,L"STATIC",L"EoriniZip clipboard test",WS_POPUP,0,0,1,1,nullptr,nullptr,nullptr,nullptr);
    if(!owner)throw std::runtime_error("Cannot create clipboard owner");
    const std::vector<std::filesystem::path> paths={L"C:\\EoriniZip-test\\한글.txt",L"C:\\EoriniZip-test\\folder with spaces"};
    kz::win::copyFilesClipboard(owner,paths);
    if(kz::win::clipboardFiles(owner)!=paths)throw std::runtime_error("Unicode CF_HDROP roundtrip failed");
    kz::win::copyFilesClipboard(owner,{paths[0]});
    if(kz::win::clipboardFiles(owner)!=std::vector<std::filesystem::path>{paths[0]})throw std::runtime_error("Clipboard replacement failed");
    if(OpenClipboard(owner)){EmptyClipboard();CloseClipboard();}
    cleanup();std::cout<<"Unicode file/folder CF_HDROP roundtrip passed in a private clipboard station\n";return 0;
  }catch(const std::exception& error){cleanup();std::cerr<<error.what()<<" (Windows error "<<GetLastError()<<")\n";return 1;}
}
