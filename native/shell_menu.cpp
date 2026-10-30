#include <windows.h>
#include <shlobj.h>
#include <shellapi.h>
#include <atomic>
#include <string>
#include <vector>
#include <algorithm>
#include <cwctype>
#include "archive_name.hpp"
#include "shell_preview.hpp"
// {5B30EB48-528D-446B-B18E-BB516C5B902F}
const CLSID CLSID_EoriniZipMenu={0x5b30eb48,0x528d,0x446b,{0xb1,0x8e,0xbb,0x51,0x6c,0x5b,0x90,0x2f}};
HMODULE module;
std::atomic<long> objects{0};
std::wstring quoted(const std::wstring& value) {
  std::wstring out=L"\"";size_t slashes=0;
  for(wchar_t c:value){if(c==L'\\'){++slashes;continue;}out.append(slashes*(c==L'"'?2:1),L'\\');slashes=0;if(c==L'"')out+=L'\\';out+=c;}
  out.append(slashes*2,L'\\');return out+L"\"";
}
HBITMAP menuIcon() {
  wchar_t path[32768];if(!GetModuleFileNameW(module,path,32768))return nullptr;
  auto executable=std::filesystem::path(path).parent_path()/L"EoriniZip.exe";
  HMODULE resources=LoadLibraryExW(executable.c_str(),nullptr,LOAD_LIBRARY_AS_DATAFILE|LOAD_LIBRARY_AS_IMAGE_RESOURCE);
  if(!resources)return nullptr;
  int width=GetSystemMetrics(SM_CXSMICON),height=GetSystemMetrics(SM_CYSMICON);
  HICON icon=static_cast<HICON>(LoadImageW(resources,MAKEINTRESOURCEW(1),IMAGE_ICON,width,height,0));
  HBITMAP bitmap=nullptr;
  if(icon){
    BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=width;info.bmiHeader.biHeight=-height;
    info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
    void* bits=nullptr;HDC dc=CreateCompatibleDC(nullptr);
    if(dc){
      bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&bits,nullptr,0);
      if(bitmap){ZeroMemory(bits,width*height*4);auto old=SelectObject(dc,bitmap);DrawIconEx(dc,0,0,icon,width,height,0,nullptr,DI_NORMAL);SelectObject(dc,old);}
      DeleteDC(dc);
    }
    DestroyIcon(icon);
  }
  FreeLibrary(resources);return bitmap;
}
class Menu final:public IShellExtInit,public IContextMenu {
  std::atomic<ULONG> refs{1};
  std::vector<std::filesystem::path> paths;
  unsigned commandCount=0;UINT previewCommand=UINT_MAX;std::vector<HBITMAP> previewBitmaps;
  HBITMAP bitmap=nullptr;
public:
  Menu(){++objects;}~Menu(){if(bitmap)DeleteObject(bitmap);for(auto icon:previewBitmaps)DeleteObject(icon);--objects;}
  IFACEMETHODIMP QueryInterface(REFIID iid,void** out) override {
    if(!out)return E_POINTER;*out=nullptr;
    if(iid==IID_IUnknown||iid==IID_IShellExtInit)*out=static_cast<IShellExtInit*>(this);
    else if(iid==IID_IContextMenu)*out=static_cast<IContextMenu*>(this);
    else return E_NOINTERFACE;
    AddRef();return S_OK;
  }
  IFACEMETHODIMP_(ULONG) AddRef() override{return ++refs;}
  IFACEMETHODIMP_(ULONG) Release() override{auto r=--refs;if(!r)delete this;return r;}
  IFACEMETHODIMP Initialize(PCIDLIST_ABSOLUTE,IDataObject* data,HKEY) override {
    paths.clear();if(!data)return E_INVALIDARG;
    FORMATETC format{CF_HDROP,nullptr,DVASPECT_CONTENT,-1,TYMED_HGLOBAL};STGMEDIUM medium{};
    if(FAILED(data->GetData(&format,&medium)))return E_INVALIDARG;
    HRESULT result=S_OK;
    try{
      auto drop=static_cast<HDROP>(medium.hGlobal);
      const UINT count=DragQueryFileW(drop,0xffffffff,nullptr,0);
      for(UINT i=0;i<count;++i){UINT n=DragQueryFileW(drop,i,nullptr,0);std::wstring path(n+1,L'\0');DragQueryFileW(drop,i,path.data(),n+1);path.resize(n);paths.emplace_back(path);}
    }catch(...){result=E_FAIL;paths.clear();}
    ReleaseStgMedium(&medium);return result;
  }
  IFACEMETHODIMP QueryContextMenu(HMENU menu,UINT index,UINT first,UINT last,UINT flags) override {
    if((flags&CMF_DEFAULTONLY)||paths.empty()||last-first<1)return MAKE_HRESULT(SEVERITY_SUCCESS,0,0);
    try {
      auto name=kz::suggestedArchive(paths).filename().wstring();
      std::wstring escaped;for(auto c:name){escaped+=c;if(c==L'&')escaped+=L'&';}
      auto label=L"\""+escaped+L"\"으로 압축하기";
      if(!InsertMenuW(menu,index,MF_BYPOSITION|MF_STRING,first,L"어린이zip으로 압축하기..."))return E_FAIL;
      if(!InsertMenuW(menu,index+1,MF_BYPOSITION|MF_STRING,first+1,label.c_str())){DeleteMenu(menu,index,MF_BYPOSITION);return E_FAIL;}
      commandCount=2;
      if(paths.size()>1&&last-first>=2){
        if(!InsertMenuW(menu,index+2,MF_BYPOSITION|MF_STRING,first+2,L"각각 개별 압축하기"))return E_FAIL;
        commandCount=3;
        const bool archives=std::all_of(paths.begin(),paths.end(),[](const auto& path){
          auto ext=path.extension().wstring();std::transform(ext.begin(),ext.end(),ext.begin(),[](wchar_t c){return static_cast<wchar_t>(towlower(c));});
          return ext==L".zip"||ext==L".7z"||ext==L".rar"||ext==L".tar"||ext==L".gz"||ext==L".bz2"||ext==L".xz"||ext==L".alz"||ext==L".egg"||ext==L".zpaq"||ext==L".001"||ext==L".cab"||ext==L".iso"||ext==L".wim";
        });
        if(archives&&last-first>=3){
          if(!InsertMenuW(menu,index+3,MF_BYPOSITION|MF_STRING,first+3,L"각각 풀기"))return E_FAIL;
          commandCount=4;
        }
      }
      previewCommand=UINT_MAX;
      if(paths.size()==1&&last-first>=commandCount){
        auto ext=paths[0].extension().wstring();std::transform(ext.begin(),ext.end(),ext.begin(),[](wchar_t c){return static_cast<wchar_t>(towlower(c));});
        if(ext==L".zip"){
          HMENU preview=CreatePopupMenu();if(!preview)return E_OUTOFMEMORY;
          auto entries=kz::shell::preview(paths[0]);previewCommand=commandCount++;
          if(entries.empty())AppendMenuW(preview,MF_STRING,first+previewCommand,L"열어서 내부 목록 보기...");
          for(const auto& entry:entries){
            std::wstring title;for(wchar_t c:entry.name){title+=c;if(c==L'&')title+=L'&';}
            AppendMenuW(preview,MF_STRING,first+previewCommand,title.c_str());
            SHFILEINFOW info{};if(SHGetFileInfoW(entry.name.c_str(),entry.directory?FILE_ATTRIBUTE_DIRECTORY:FILE_ATTRIBUTE_NORMAL,&info,sizeof(info),SHGFI_ICON|SHGFI_SMALLICON|SHGFI_USEFILEATTRIBUTES)){
              BITMAPINFO bits{};bits.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bits.bmiHeader.biWidth=16;bits.bmiHeader.biHeight=-16;bits.bmiHeader.biPlanes=1;bits.bmiHeader.biBitCount=32;bits.bmiHeader.biCompression=BI_RGB;
              HDC dc=CreateCompatibleDC(nullptr);void* data=nullptr;HBITMAP icon=CreateDIBSection(dc,&bits,DIB_RGB_COLORS,&data,nullptr,0);
              if(icon){ZeroMemory(data,16*16*4);auto old=SelectObject(dc,icon);DrawIconEx(dc,0,0,info.hIcon,16,16,0,nullptr,DI_NORMAL);SelectObject(dc,old);
                MENUITEMINFOW item{sizeof(item)};item.fMask=MIIM_BITMAP;item.hbmpItem=icon;SetMenuItemInfoW(preview,GetMenuItemCount(preview)-1,TRUE,&item);previewBitmaps.push_back(icon);}
              DeleteDC(dc);DestroyIcon(info.hIcon);
            }
          }
          if(!InsertMenuW(menu,index+commandCount-1,MF_BYPOSITION|MF_POPUP|MF_STRING,reinterpret_cast<UINT_PTR>(preview),L"압축 파일 미리 보기")){DestroyMenu(preview);return E_FAIL;}
        }
      }      if(!bitmap)bitmap=menuIcon();
      if(bitmap){MENUITEMINFOW item{sizeof(item)};item.fMask=MIIM_BITMAP;item.hbmpItem=bitmap;
        for(UINT i=0;i<commandCount;++i)SetMenuItemInfoW(menu,first+i,FALSE,&item);}
      return MAKE_HRESULT(SEVERITY_SUCCESS,0,commandCount);    }catch(...){return E_FAIL;}
  }
  IFACEMETHODIMP InvokeCommand(CMINVOKECOMMANDINFO* info) override {
    if(!info)return E_POINTER;
    UINT verb=0;
    if(HIWORD(info->lpVerb)){
      if(lstrcmpiA(info->lpVerb,"eorinizip.compress")==0)verb=0;
      else if(lstrcmpiA(info->lpVerb,"eorinizip.quick")==0)verb=1;
      else if(lstrcmpiA(info->lpVerb,"eorinizip.preview")==0)verb=previewCommand;
      else if(lstrcmpiA(info->lpVerb,"eorinizip.each")==0)verb=2;
      else if(lstrcmpiA(info->lpVerb,"eorinizip.extractEach")==0)verb=3;
      else return E_INVALIDARG;
    }else verb=LOWORD(info->lpVerb);
    if(verb>=commandCount||paths.empty())return E_INVALIDARG;
    try{
      wchar_t filename[32768];DWORD n=GetModuleFileNameW(module,filename,32768);if(!n||n>=32768)return E_FAIL;
      auto executable=std::filesystem::path(filename).parent_path()/L"EoriniZip.exe";
      std::wstring command=quoted(executable.wstring())+(verb==previewCommand?L" --open":verb==0?L" --compress":verb==1?L" --compress-quick":verb==2?L" --compress-each":L" --extract-each");
      for(const auto& path:paths)command+=L" "+quoted(path.wstring());
      if(command.size()>=32767)return HRESULT_FROM_WIN32(ERROR_FILENAME_EXCED_RANGE);
      STARTUPINFOW startup{};startup.cb=sizeof(startup);PROCESS_INFORMATION process{};
      if(!CreateProcessW(executable.c_str(),command.data(),nullptr,nullptr,FALSE,0,nullptr,nullptr,&startup,&process))return HRESULT_FROM_WIN32(GetLastError());
      CloseHandle(process.hThread);CloseHandle(process.hProcess);return S_OK;
    }catch(...){return E_FAIL;}
  }
  IFACEMETHODIMP GetCommandString(UINT_PTR id,UINT flags,UINT*,LPSTR text,UINT count) override {
    if(id>=commandCount)return E_INVALIDARG;
    if(id==previewCommand){if(flags==GCS_VERBA)return lstrcpynA(text,"eorinizip.preview",count)?S_OK:E_FAIL;if(flags==GCS_VERBW)return lstrcpynW(reinterpret_cast<LPWSTR>(text),L"eorinizip.preview",count)?S_OK:E_FAIL;}
    if(flags==GCS_VERBA)return lstrcpynA(text,id==0?"eorinizip.compress":id==1?"eorinizip.quick":id==2?"eorinizip.each":"eorinizip.extractEach",count)?S_OK:E_FAIL;
    if(flags==GCS_VERBW)return lstrcpynW(reinterpret_cast<LPWSTR>(text),id==0?L"eorinizip.compress":id==1?L"eorinizip.quick":id==2?L"eorinizip.each":L"eorinizip.extractEach",count)?S_OK:E_FAIL;
    return E_NOTIMPL;
  }
};
class Factory final:public IClassFactory {
  std::atomic<ULONG> refs{1};
public:
  Factory(){++objects;}~Factory(){--objects;}
  IFACEMETHODIMP QueryInterface(REFIID iid,void** out) override{
    if(!out)return E_POINTER;*out=nullptr;if(iid!=IID_IUnknown&&iid!=IID_IClassFactory)return E_NOINTERFACE;
    *out=static_cast<IClassFactory*>(this);AddRef();return S_OK;
  }
  IFACEMETHODIMP_(ULONG) AddRef() override{return ++refs;}
  IFACEMETHODIMP_(ULONG) Release() override{auto r=--refs;if(!r)delete this;return r;}
  IFACEMETHODIMP CreateInstance(IUnknown* outer,REFIID iid,void** out) override {
    if(!out)return E_POINTER;*out=nullptr;if(outer)return CLASS_E_NOAGGREGATION;
    try{auto menu=new Menu;auto hr=menu->QueryInterface(iid,out);menu->Release();return hr;}catch(...){return E_OUTOFMEMORY;}
  }
  IFACEMETHODIMP LockServer(BOOL lock) override{if(lock)++objects;else --objects;return S_OK;}
};
STDAPI DllGetClassObject(REFCLSID clsid,REFIID iid,void** out){
  if(!out)return E_POINTER;*out=nullptr;if(clsid!=CLSID_EoriniZipMenu)return CLASS_E_CLASSNOTAVAILABLE;
  try{auto factory=new Factory;auto hr=factory->QueryInterface(iid,out);factory->Release();return hr;}catch(...){return E_OUTOFMEMORY;}
}
STDAPI DllCanUnloadNow(){return objects==0?S_OK:S_FALSE;}
BOOL WINAPI DllMain(HINSTANCE instance,DWORD reason,LPVOID){if(reason==DLL_PROCESS_ATTACH){module=instance;DisableThreadLibraryCalls(instance);}return TRUE;}
