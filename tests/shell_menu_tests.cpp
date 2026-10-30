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
    CLSID clsid;CLSIDFromString(L"{5B30EB48-528D-446B-B18E-BB516C5B902E}",&clsid);
    IClassFactory* factory=nullptr;check(SUCCEEDED(get(clsid,IID_PPV_ARGS(&factory))),"Class factory");
    for(int count:{1,2}){
      IShellExtInit* init=nullptr;check(SUCCEEDED(factory->CreateInstance(nullptr,IID_PPV_ARGS(&init))),"Create handler");
      PIDLIST_ABSOLUTE parent=nullptr;SHParseDisplayName(root.c_str(),nullptr,&parent,0,nullptr);
      std::vector<PIDLIST_ABSOLUTE> absolute;std::vector<PCUITEMID_CHILD> children;
      for(auto path:{one,two}){PIDLIST_ABSOLUTE item=nullptr;SHParseDisplayName(path.c_str(),nullptr,&item,0,nullptr);absolute.push_back(item);children.push_back(ILFindLastID(item));}
      IDataObject* data=nullptr;check(SUCCEEDED(SHCreateDataObject(parent,count,children.data(),nullptr,IID_PPV_ARGS(&data))),"Selection data");
      check(SUCCEEDED(init->Initialize(nullptr,data,nullptr)),"Initialize selection");
      IContextMenu* context=nullptr;init->QueryInterface(IID_PPV_ARGS(&context));
      HMENU menu=CreatePopupMenu();auto result=context->QueryContextMenu(menu,0,100,200,0);check(SUCCEEDED(result)&&HRESULT_CODE(result)==2,"Two menu commands");
      for(UINT id:{100u,101u}){MENUITEMINFOW item{sizeof(item)};item.fMask=MIIM_BITMAP;check(GetMenuItemInfoW(menu,id,FALSE,&item)&&item.hbmpItem,"Menu bitmap attached");}
      wchar_t label[1024];GetMenuStringW(menu,1,label,1024,MF_BYPOSITION);
      auto expected=count==1?L"\"한글 && file.zip\"으로 압축하기":L"\""+root.filename().wstring()+L".zip\"으로 압축하기";
      check(std::wstring(label)==expected,"Dynamic archive name");
      DestroyMenu(menu);context->Release();init->Release();data->Release();CoTaskMemFree(parent);for(auto item:absolute)CoTaskMemFree(item);
    }
    factory->Release();FreeLibrary(dll);std::filesystem::remove_all(root);CoUninitialize();
    std::cout<<"Native shell single/multi selection labels passed\n";return 0;
  }catch(const std::exception& e){std::cerr<<e.what()<<"\n";std::filesystem::remove_all(root);CoUninitialize();return 1;}
}
