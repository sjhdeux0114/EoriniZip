#include <windows.h>
#include <shobjidl.h>
#include <atomic>
#include <vector>
#include <string>
#include <filesystem>
#include <algorithm>
#include <cwctype>
#include "archive_name.hpp"
#include "shell_preview.hpp"
#include <shellapi.h>
const CLSID CLSID_Modern={0x5b30eb48,0x528d,0x446b,{0xb1,0x8e,0xbb,0x51,0x6c,0x5b,0x90,0x30}};
static HMODULE module;
static std::atomic<long> objects{0};
static HRESULT copyText(const std::wstring& s,PWSTR* out){
 if(!out)return E_POINTER;*out=nullptr;
 auto p=static_cast<PWSTR>(CoTaskMemAlloc((s.size()+1)*sizeof(wchar_t)));if(!p)return E_OUTOFMEMORY;
 memcpy(p,s.c_str(),(s.size()+1)*sizeof(wchar_t));*out=p;return S_OK;
}
static std::filesystem::path executable(){
 wchar_t buffer[32768];auto n=GetModuleFileNameW(module,buffer,32768);if(!n||n>=32768)throw std::runtime_error("Module path");
 return std::filesystem::path(buffer).parent_path()/L"EoriniZip.exe";
}
static std::vector<std::filesystem::path> selection(IShellItemArray* items){
 std::vector<std::filesystem::path> result;if(!items)return result;DWORD count=0;if(FAILED(items->GetCount(&count)))return result;
 for(DWORD i=0;i<count;++i){IShellItem* item=nullptr;if(FAILED(items->GetItemAt(i,&item)))return {};
   PWSTR path=nullptr;auto hr=item->GetDisplayName(SIGDN_FILESYSPATH,&path);item->Release();
   if(FAILED(hr))return {};try{result.emplace_back(path);}catch(...){CoTaskMemFree(path);throw;}CoTaskMemFree(path);}
 return result;
}
static bool isArchive(const std::filesystem::path& p){
 auto e=p.extension().wstring();std::transform(e.begin(),e.end(),e.begin(),[](wchar_t c){return static_cast<wchar_t>(towlower(c));});
 for(auto ext:{L".zip",L".7z",L".rar",L".tar",L".gz",L".bz2",L".xz",L".alz",L".egg",L".zpaq",L".001",L".cab",L".iso",L".wim"})if(e==ext)return true;
 return false;
}
static std::wstring quote(const std::wstring& s){
 std::wstring out=L"\"";size_t n=0;for(auto c:s){if(c==L'\\'){++n;continue;}out.append(n*(c==L'"'?2:1),L'\\');n=0;if(c==L'"')out+=L'\\';out+=c;}out.append(n*2,L'\\');return out+L"\"";
}
class Commands;
class Command final:public IExplorerCommand{
 std::atomic<ULONG> refs{1};int kind;std::filesystem::path previewPath;std::wstring previewTitle;bool previewDirectory=false;
public:
 explicit Command(int k=-1,std::filesystem::path p={},std::wstring title=L"",bool directory=false):kind(k),previewPath(std::move(p)),previewTitle(std::move(title)),previewDirectory(directory){++objects;}~Command(){--objects;}
 IFACEMETHODIMP QueryInterface(REFIID iid,void** out)override{if(!out)return E_POINTER;*out=nullptr;if(iid!=IID_IUnknown&&iid!=IID_IExplorerCommand)return E_NOINTERFACE;*out=static_cast<IExplorerCommand*>(this);AddRef();return S_OK;}
 IFACEMETHODIMP_(ULONG) AddRef()override{return ++refs;}
 IFACEMETHODIMP_(ULONG) Release()override{auto n=--refs;if(!n)delete this;return n;}
 IFACEMETHODIMP GetTitle(IShellItemArray* items,PWSTR* out)override{
  try{auto selected=selection(items);if(selected.size()==1)previewPath=selected[0];if(kind>=9)return copyText(previewTitle,out);if(kind==8)return copyText(L"압축 파일 미리 보기",out);if(kind<0)return copyText(L"어린이zip",out);
   if(kind==2){auto paths=selection(items);if(paths.empty())return copyText(L"ZIP으로 바로 압축하기",out);return copyText(L"\""+kz::suggestedArchive(paths).filename().wstring()+L"\"으로 압축하기",out);}
   const wchar_t* titles[]={L"어린이zip으로 열기",L"어린이zip으로 압축하기...",L"",L"각각 개별 압축하기",L"어린이zip으로 압축 풀기",L"알아서 풀기",L"새 폴더에 풀기",L"각각 풀기"};
   return copyText(titles[kind],out);
  }catch(...){if(out)*out=nullptr;return E_FAIL;}
 }
 IFACEMETHODIMP GetIcon(IShellItemArray*,PWSTR* out)override{try{if(kind>=9){SHFILEINFOW info{};if(SHGetFileInfoW(previewTitle.c_str(),previewDirectory?FILE_ATTRIBUTE_DIRECTORY:FILE_ATTRIBUTE_NORMAL,&info,sizeof(info),SHGFI_ICONLOCATION|SHGFI_USEFILEATTRIBUTES)&&info.szDisplayName[0])return copyText(std::wstring(info.szDisplayName)+L","+std::to_wstring(info.iIcon),out);}return copyText(executable().wstring()+L",0",out);}catch(...){if(out)*out=nullptr;return E_FAIL;}}
 IFACEMETHODIMP GetToolTip(IShellItemArray*,PWSTR* out)override{if(!out)return E_POINTER;*out=nullptr;return E_NOTIMPL;}
 IFACEMETHODIMP GetCanonicalName(GUID* out)override{if(!out)return E_POINTER;*out=CLSID_Modern;if(kind>=0)out->Data1+=kind+1;return S_OK;}
 IFACEMETHODIMP GetState(IShellItemArray* items,BOOL,EXPCMDSTATE* state)override{
  if(!state)return E_POINTER;*state=ECS_HIDDEN;
  try{auto paths=selection(items);if(paths.empty())return S_OK;bool archives=std::all_of(paths.begin(),paths.end(),isArchive);if(paths.size()==1)previewPath=paths[0];
   bool visible=(kind>=8&&paths.size()==1&&_wcsicmp(paths[0].extension().c_str(),L".zip")==0)||kind<0||kind==1||kind==2||(kind==3&&paths.size()>1)||(kind==7&&paths.size()>1&&archives)||((kind==0||kind==4||kind==5||kind==6)&&paths.size()==1&&archives);
   *state=visible?ECS_ENABLED:ECS_HIDDEN;return S_OK;
  }catch(...){return E_FAIL;}
 }
 IFACEMETHODIMP Invoke(IShellItemArray* items,IBindCtx*)override{
  if(kind<0||kind==8)return E_NOTIMPL;
  try{EXPCMDSTATE state;if(FAILED(GetState(items,TRUE,&state))||state!=ECS_ENABLED)return E_INVALIDARG;
   auto paths=selection(items);const wchar_t* modes[]={L"--open",L"--compress",L"--compress-quick",L"--compress-each",L"--extract-here",L"--extract-smart",L"--extract-folder",L"--extract-each"};
   auto app=executable();auto line=quote(app.wstring())+L" "+(kind>=9?L"--open":modes[kind]);for(const auto& path:paths)line+=L" "+quote(path.wstring());
   if(line.size()>=32767)return HRESULT_FROM_WIN32(ERROR_FILENAME_EXCED_RANGE);
   STARTUPINFOW startup{sizeof(startup)};PROCESS_INFORMATION process{};
   if(!CreateProcessW(app.c_str(),line.data(),nullptr,nullptr,FALSE,0,nullptr,nullptr,&startup,&process))return HRESULT_FROM_WIN32(GetLastError());
   CloseHandle(process.hThread);CloseHandle(process.hProcess);return S_OK;
  }catch(...){return E_FAIL;}
 }
 IFACEMETHODIMP GetFlags(EXPCMDFLAGS* flags)override{if(!flags)return E_POINTER;*flags=(kind<0||kind==8)?ECF_HASSUBCOMMANDS:ECF_DEFAULT;return S_OK;}
 IFACEMETHODIMP EnumSubCommands(IEnumExplorerCommand** out)override;
};
class Commands final:public IEnumExplorerCommand{
 std::atomic<ULONG> refs{1};ULONG index=0;std::filesystem::path path;bool isPreview=false;std::vector<kz::shell::PreviewItem> entries;
 ULONG total()const{return isPreview?static_cast<ULONG>(entries.size()):9;}
public:
 explicit Commands(ULONG i=0,std::filesystem::path p={},bool preview=false):index(i),path(std::move(p)),isPreview(preview){if(isPreview){entries=kz::shell::preview(path);if(entries.empty())entries.push_back({L"열어서 내부 목록 보기...",false});}++objects;}~Commands(){--objects;}
 IFACEMETHODIMP QueryInterface(REFIID iid,void** out)override{if(!out)return E_POINTER;*out=nullptr;if(iid!=IID_IUnknown&&iid!=IID_IEnumExplorerCommand)return E_NOINTERFACE;*out=static_cast<IEnumExplorerCommand*>(this);AddRef();return S_OK;}
 IFACEMETHODIMP_(ULONG) AddRef()override{return ++refs;}
 IFACEMETHODIMP_(ULONG) Release()override{auto n=--refs;if(!n)delete this;return n;}
 IFACEMETHODIMP Next(ULONG count,IExplorerCommand** out,ULONG* fetched)override{
  if(!out||(!fetched&&count!=1))return E_POINTER;if(fetched)*fetched=0;ULONG n=0;
  try{while(n<count&&index<total()){out[n++]=isPreview?new Command(9,path,entries[index].name,entries[index].directory):new Command(static_cast<int>(index),path);++index;}if(fetched)*fetched=n;return n==count?S_OK:S_FALSE;}
  catch(...){for(ULONG i=0;i<n;++i){out[i]->Release();out[i]=nullptr;}return E_OUTOFMEMORY;}
 }
 IFACEMETHODIMP Skip(ULONG count)override{auto n=std::min<ULONG>(count,total()-index);index+=n;return n==count?S_OK:S_FALSE;}
 IFACEMETHODIMP Reset()override{index=0;return S_OK;}
 IFACEMETHODIMP Clone(IEnumExplorerCommand** out)override{if(!out)return E_POINTER;try{*out=new Commands(index,path,isPreview);return S_OK;}catch(...){*out=nullptr;return E_OUTOFMEMORY;}}
};
HRESULT Command::EnumSubCommands(IEnumExplorerCommand** out){if(!out)return E_POINTER;*out=nullptr;if(kind>=0&&kind!=8)return E_NOTIMPL;try{*out=new Commands(0,previewPath,kind==8);return S_OK;}catch(...){return E_OUTOFMEMORY;}}
class Factory final:public IClassFactory{
 std::atomic<ULONG> refs{1};
public:
 Factory(){++objects;}~Factory(){--objects;}
 IFACEMETHODIMP QueryInterface(REFIID iid,void** out)override{if(!out)return E_POINTER;*out=nullptr;if(iid!=IID_IUnknown&&iid!=IID_IClassFactory)return E_NOINTERFACE;*out=static_cast<IClassFactory*>(this);AddRef();return S_OK;}
 IFACEMETHODIMP_(ULONG) AddRef()override{return ++refs;}
 IFACEMETHODIMP_(ULONG) Release()override{auto n=--refs;if(!n)delete this;return n;}
 IFACEMETHODIMP CreateInstance(IUnknown* outer,REFIID iid,void** out)override{if(!out)return E_POINTER;*out=nullptr;if(outer)return CLASS_E_NOAGGREGATION;try{auto p=new Command;auto hr=p->QueryInterface(iid,out);p->Release();return hr;}catch(...){return E_OUTOFMEMORY;}}
 IFACEMETHODIMP LockServer(BOOL lock)override{if(lock)++objects;else --objects;return S_OK;}
};
STDAPI DllGetClassObject(REFCLSID clsid,REFIID iid,void** out){if(!out)return E_POINTER;*out=nullptr;if(clsid!=CLSID_Modern)return CLASS_E_CLASSNOTAVAILABLE;try{auto p=new Factory;auto hr=p->QueryInterface(iid,out);p->Release();return hr;}catch(...){return E_OUTOFMEMORY;}}
STDAPI DllCanUnloadNow(){return objects==0?S_OK:S_FALSE;}
BOOL WINAPI DllMain(HINSTANCE instance,DWORD reason,LPVOID){if(reason==DLL_PROCESS_ATTACH){module=instance;DisableThreadLibraryCalls(instance);}return TRUE;}
