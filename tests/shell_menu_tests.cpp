#include <windows.h>
#include <shlobj.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>
#include <string>
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
int wmain(int argc,wchar_t** argv){
  if(argc!=2)return 2;CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
  auto root=std::filesystem::temp_directory_path()/(L"EoriniZipShellTest-"+std::to_wstring(GetCurrentProcessId()));
  std::filesystem::create_directory(root);
  try{
    auto one=root/L"한글 & file.txt",two=root/L"second.txt";std::ofstream(one)<<"one";std::ofstream(two)<<"two";
    HMODULE dll=LoadLibraryW(argv[1]);check(dll!=nullptr,"Load DLL");
    auto get=reinterpret_cast<HRESULT(STDAPICALLTYPE*)(REFCLSID,REFIID,void**)>(GetProcAddress(dll,"DllGetClassObject"));check(get!=nullptr,"DLL export");
    CLSID clsid;CLSIDFromString(L"{5B30EB48-528D-446B-B18E-BB516C5B902F}",&clsid);
    IClassFactory* factory=nullptr;check(SUCCEEDED(get(clsid,IID_PPV_ARGS(&factory))),"Class factory");
    auto zipOne=root/L"one.zip",zipTwo=root/L"two.egg";std::ofstream(zipOne)<<"fixture";std::ofstream(zipTwo)<<"fixture";
    for(int scenario:{0,1,2}){int count=scenario==0?1:2;const UINT menuCount=scenario==2?4:scenario==1?3:2;
      IShellExtInit* init=nullptr;check(SUCCEEDED(factory->CreateInstance(nullptr,IID_PPV_ARGS(&init))),"Create handler");
      PIDLIST_ABSOLUTE parent=nullptr;SHParseDisplayName(root.c_str(),nullptr,&parent,0,nullptr);
      std::vector<PIDLIST_ABSOLUTE> absolute;std::vector<PCUITEMID_CHILD> children;
      for(auto path:{scenario==2?zipOne:one,scenario==2?zipTwo:two}){PIDLIST_ABSOLUTE item=nullptr;SHParseDisplayName(path.c_str(),nullptr,&item,0,nullptr);absolute.push_back(item);children.push_back(ILFindLastID(item));}
      IDataObject* data=nullptr;check(SUCCEEDED(SHCreateDataObject(parent,count,children.data(),nullptr,IID_PPV_ARGS(&data))),"Selection data");
      check(SUCCEEDED(init->Initialize(nullptr,data,nullptr)),"Initialize selection");
      IContextMenu* context=nullptr;init->QueryInterface(IID_PPV_ARGS(&context));
      HMENU menu=CreatePopupMenu();auto result=context->QueryContextMenu(menu,0,100,200,0);check(SUCCEEDED(result)&&static_cast<UINT>(HRESULT_CODE(result))==menuCount,"Single and multi selection menu counts");
      for(UINT id=100;id<100+menuCount;++id){MENUITEMINFOW item{sizeof(item)};item.fMask=MIIM_BITMAP;check(GetMenuItemInfoW(menu,id,FALSE,&item)&&item.hbmpItem,"Menu bitmap attached");}
      wchar_t label[1024];GetMenuStringW(menu,1,label,1024,MF_BYPOSITION);
      auto expected=count==1?L"\"한글 && file.zip\"으로 압축하기":L"\""+root.filename().wstring()+L".zip\"으로 압축하기";
      check(std::wstring(label)==expected,"Dynamic archive name");
      if(count==2){GetMenuStringW(menu,2,label,1024,MF_BYPOSITION);check(std::wstring(label)==L"각각 개별 압축하기","Individual compression label");char verb[80]{};check(SUCCEEDED(context->GetCommandString(2,GCS_VERBA,nullptr,verb,80))&&std::string(verb)=="eorinizip.each","Individual compression verb");}
      if(scenario==2){GetMenuStringW(menu,3,label,1024,MF_BYPOSITION);check(std::wstring(label)==L"각각 풀기","Individual extraction label");char verb[80]{};check(SUCCEEDED(context->GetCommandString(3,GCS_VERBA,nullptr,verb,80))&&std::string(verb)=="eorinizip.extractEach","Individual extraction verb");}
      DestroyMenu(menu);context->Release();init->Release();data->Release();CoTaskMemFree(parent);for(auto item:absolute)CoTaskMemFree(item);
    }
    factory->Release();FreeLibrary(dll);std::filesystem::remove_all(root);CoUninitialize();
    std::cout<<"Native shell single/multi selection labels passed\n";return 0;
  }catch(const std::exception& e){std::cerr<<e.what()<<"\n";std::filesystem::remove_all(root);CoUninitialize();return 1;}
}
