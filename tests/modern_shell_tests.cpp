#include <windows.h>
#include <shobjidl.h>
#include <shlobj.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
int wmain(int argc,wchar_t** argv){
 if(argc!=2)return 2;CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
 auto root=std::filesystem::temp_directory_path()/(L"EoriniZipModernTest-"+std::to_wstring(GetCurrentProcessId()));std::filesystem::create_directory(root);
 try{
  auto file=root/L"one.txt",zip=root/L"one.zip",zip2=root/L"two.egg",folder=root/L"folder",zipFolder=root/L"folder.zip";std::ofstream(file)<<"x";std::ofstream(zip)<<"x";std::ofstream(zip2)<<"x";std::filesystem::create_directory(folder);std::filesystem::create_directory(zipFolder);
  auto dll=LoadLibraryW(argv[1]);check(dll!=nullptr,"Load modern DLL");
  auto get=reinterpret_cast<HRESULT(STDAPICALLTYPE*)(REFCLSID,REFIID,void**)>(GetProcAddress(dll,"DllGetClassObject"));
  CLSID clsid;CLSIDFromString(L"{5B30EB48-528D-446B-B18E-BB516C5B9030}",&clsid);
  IClassFactory* factory=nullptr;check(SUCCEEDED(get(clsid,IID_PPV_ARGS(&factory))),"Modern class factory");
  for(int scenario=0;scenario<6;++scenario){
   const bool multiple=scenario==2||scenario==4;const bool archive=scenario==1||scenario==2;
   auto first=scenario==0?file:scenario==3?folder:scenario==5?zipFolder:zip;auto second=scenario==4?file:zip2;
   PIDLIST_ABSOLUTE ids[2]{};SHParseDisplayName(first.c_str(),nullptr,&ids[0],0,nullptr);SHParseDisplayName(second.c_str(),nullptr,&ids[1],0,nullptr);
   IShellItemArray* selection=nullptr;check(SUCCEEDED(SHCreateShellItemArrayFromIDLists(multiple?2:1,const_cast<PCIDLIST_ABSOLUTE*>(ids),&selection)),"Modern selection");
   IExplorerCommand* command=nullptr;check(SUCCEEDED(factory->CreateInstance(nullptr,IID_PPV_ARGS(&command))),"Modern command");
   PWSTR title=nullptr;check(SUCCEEDED(command->GetTitle(selection,&title))&&std::wstring(title)==L"어린이zip","Modern root title");CoTaskMemFree(title);
   EXPCMDSTATE state;check(SUCCEEDED(command->GetState(selection,FALSE,&state))&&state==ECS_ENABLED,"Modern root enabled");
   EXPCMDFLAGS flags;command->GetFlags(&flags);check(flags==static_cast<EXPCMDFLAGS>(ECF_HASSUBCOMMANDS),"Modern flyout flag");
   IEnumExplorerCommand* children=nullptr;check(SUCCEEDED(command->EnumSubCommands(&children)),"Modern submenu");
   for(int i=0;i<9;++i){IExplorerCommand* child=nullptr;ULONG fetched=0;check(children->Next(1,&child,&fetched)==S_OK&&fetched==1,"Modern enumerate");
    child->GetState(selection,FALSE,&state);
    const bool expected=i==1||i==2||(i==3&&multiple)||(i==7&&multiple&&archive)||(!multiple&&archive&&(i==0||i==4||i==5||i==6||i==8));
    if(state!=(expected?ECS_ENABLED:ECS_HIDDEN))std::cerr<<"Scenario "<<scenario<<", command "<<i<<", state "<<state<<", expected visible "<<expected<<"\n";
    check(state==(expected?ECS_ENABLED:ECS_HIDDEN),"Modern command visibility");
    if(expected){check(SUCCEEDED(child->GetTitle(selection,&title))&&title!=nullptr,"Modern child title");CoTaskMemFree(title);PWSTR icon=nullptr;check(SUCCEEDED(child->GetIcon(selection,&icon))&&icon,"Modern icon");CoTaskMemFree(icon);}
    if(i==8&&scenario==1){IEnumExplorerCommand* preview=nullptr;check(SUCCEEDED(child->EnumSubCommands(&preview)),"Preview submenu");IExplorerCommand* item=nullptr;check(preview->Next(1,&item,&fetched)==S_OK,"Preview fallback");item->Release();preview->Release();}
    child->Release();
   }
   IExplorerCommand* end=nullptr;ULONG fetched=0;check(children->Next(1,&end,&fetched)==S_FALSE&&fetched==0,"Modern enumeration end");
   children->Reset();IEnumExplorerCommand* clone=nullptr;check(SUCCEEDED(children->Clone(&clone)),"Modern enumeration clone");clone->Release();
   children->Release();command->Release();selection->Release();CoTaskMemFree(ids[0]);CoTaskMemFree(ids[1]);
  }
  factory->Release();auto unload=reinterpret_cast<HRESULT(STDAPICALLTYPE*)()>(GetProcAddress(dll,"DllCanUnloadNow"));check(unload()==S_OK,"Modern objects released");FreeLibrary(dll);
  std::filesystem::remove_all(root);CoUninitialize();std::cout<<"Windows 11 command states, titles, icons and preview enumeration passed\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<"\n";std::filesystem::remove_all(root);CoUninitialize();return 1;}
}
