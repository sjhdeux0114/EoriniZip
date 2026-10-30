#include "core.hpp"
#include "split_presets.hpp"
#include "archive_name.hpp"
#include "resource.h"
#include <windows.h>
#include <commctrl.h>
#include <shobjidl.h>
#include <memory>
#include <thread>
#include <functional>
#include <stdexcept>
#include <commdlg.h>
#include "windows_integration.hpp"
#include <algorithm>
#include <chrono>
#include <map>
#include <fstream>
namespace {
HWND progressWindow=nullptr, tree=nullptr;
HWND window, list, statusText, pathText, password, passwordLabel, logo, homeOpen, homeCreate;
HFONT font;
HBRUSH footerBrush;
std::vector<HWND> workspace, controls;
std::unique_ptr<kz::Engine> engine;
std::thread worker;
kz::fs::path archive;
bool busy=false, home=true, onTop=false, statusVisible=true, treeVisible=true, buildingTree=false;
std::vector<kz::Entry> archiveEntries,visibleEntries;
std::vector<std::string> treePaths;
std::map<std::string,HTREEITEM> treeItems;
std::string currentFolder;
int dpi=96;
struct PreferencesData { bool escape=true,grid=false,fullRow=true,startHome=true,test=false; bool unicodeNames=true,storeTimes=true,excludeThumbs=false,parallel=true,zone=true; int codePageIndex=0; bool showTree=true,showStatus=true; COLORREF theme=RGB(240,179,21); int level=2; } preferences;
const int codePages[]={0,949,932,936,950,1252,65001};
COLORREF pendingTheme=RGB(240,179,21);
constexpr const wchar_t* preferencesKey=L"Software\\EoriniZip\\Preferences";
void loadPreferences() {
  auto read=[](const wchar_t* key,DWORD fallback){DWORD value=0,size=sizeof(value);return RegGetValueW(HKEY_CURRENT_USER,preferencesKey,key,RRF_RT_REG_DWORD,nullptr,&value,&size)==ERROR_SUCCESS?value:fallback;};
  preferences.unicodeNames=read(L"UnicodeNames",1)!=0;preferences.storeTimes=read(L"StoreTimes",1)!=0;preferences.excludeThumbs=read(L"ExcludeThumbs",0)!=0;preferences.parallel=read(L"Parallel",1)!=0;preferences.zone=read(L"PropagateZone",1)!=0;preferences.codePageIndex=static_cast<int>(std::min<DWORD>(read(L"CodePageIndex",0),6));
  preferences.showTree=read(L"ShowTree",1)!=0;preferences.showStatus=read(L"ShowStatus",1)!=0;preferences.theme=read(L"Theme",RGB(240,179,21));
  preferences.escape=read(L"Escape",1)!=0;preferences.grid=read(L"Grid",0)!=0;preferences.fullRow=read(L"FullRow",1)!=0;
  preferences.startHome=read(L"StartHome",1)!=0;preferences.test=read(L"TestAfter",0)!=0;preferences.level=static_cast<int>(std::min<DWORD>(read(L"Level",2),3));
}
bool savePreferences() {
  HKEY key;if(RegCreateKeyExW(HKEY_CURRENT_USER,preferencesKey,0,nullptr,0,KEY_SET_VALUE,nullptr,&key,nullptr)!=ERROR_SUCCESS)return false;
  bool ok=true;auto write=[&](const wchar_t* name,DWORD value){if(RegSetValueExW(key,name,0,REG_DWORD,reinterpret_cast<const BYTE*>(&value),sizeof(value))!=ERROR_SUCCESS)ok=false;};
  write(L"UnicodeNames",preferences.unicodeNames);write(L"StoreTimes",preferences.storeTimes);write(L"ExcludeThumbs",preferences.excludeThumbs);write(L"Parallel",preferences.parallel);write(L"PropagateZone",preferences.zone);write(L"CodePageIndex",preferences.codePageIndex);
  write(L"ShowTree",preferences.showTree);write(L"ShowStatus",preferences.showStatus);write(L"Theme",preferences.theme);
  write(L"Escape",preferences.escape);write(L"Grid",preferences.grid);write(L"FullRow",preferences.fullRow);
  write(L"StartHome",preferences.startHome);write(L"TestAfter",preferences.test);write(L"Level",preferences.level);RegCloseKey(key);return ok;
}
enum { Open=100, CreateFiles, CreateFolder, Extract, Test, Connect, Cancel, Refresh, Home, About, Exit, SelectAll, CopyNames, CloseArchive, SaveAs, OpenFolder, NewWindow, Topmost, ToggleStatus, AppFolder, HelpPage, IssuesPage, Website, FindFile, Preferences, ToggleTree };
constexpr UINT Finished=WM_APP+1, Startup=WM_APP+2;
struct Result { std::vector<kz::Entry> entries; bool listing=false; std::string message; bool success=true; };
struct CreateRequest {
  std::vector<kz::fs::path> inputs;
  kz::fs::path output;
  kz::Options options;
  bool testAfter=false;
  std::wstring suggested;
};
int px(int v) { return MulDiv(v,dpi,96); }
std::wstring text(HWND control) {
  int n=GetWindowTextLengthW(control); std::wstring s(n+1,L'\0');
  GetWindowTextW(control,s.data(),n+1); s.resize(n); return s;
}
HWND control(const wchar_t* cls,const wchar_t* title,DWORD style,int id=0) {
  HWND item=CreateWindowExW(0,cls,title,WS_CHILD|WS_VISIBLE|style,0,0,1,1,window,reinterpret_cast<HMENU>(static_cast<intptr_t>(id)),nullptr,nullptr);
  SendMessageW(item,WM_SETFONT,reinterpret_cast<WPARAM>(font),TRUE); return item;
}
std::vector<kz::fs::path> choose(bool folder,bool multiple=false,bool save=false,const std::wstring& name=L"",HWND owner=nullptr) {
  IFileDialog* dialog=nullptr;
  if(FAILED(CoCreateInstance(save?CLSID_FileSaveDialog:CLSID_FileOpenDialog,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&dialog)))) throw std::runtime_error("파일 선택창을 열 수 없습니다.");
  DWORD flags; dialog->GetOptions(&flags); flags|=FOS_FORCEFILESYSTEM|FOS_NOCHANGEDIR;
  if(folder)flags|=FOS_PICKFOLDERS;
  if(multiple)flags|=FOS_ALLOWMULTISELECT;
  dialog->SetOptions(flags);
  if(!name.empty())dialog->SetFileName(name.c_str());
  std::vector<kz::fs::path> results;
  if(SUCCEEDED(dialog->Show(owner?owner:window))) {
    auto add=[&](IShellItem* item) {
      PWSTR p=nullptr; if(SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH,&p))) {results.emplace_back(p);CoTaskMemFree(p);}
    };
    if(multiple) {
      IFileOpenDialog* open=nullptr;
      if(SUCCEEDED(dialog->QueryInterface(IID_PPV_ARGS(&open)))) {
        IShellItemArray* items=nullptr;
        if(SUCCEEDED(open->GetResults(&items))) {
          DWORD count;items->GetCount(&count);
          for(DWORD i=0;i<count;++i){IShellItem* item=nullptr;if(SUCCEEDED(items->GetItemAt(i,&item))){add(item);item->Release();}}
          items->Release();
        }open->Release();
      }
    } else {IShellItem* item=nullptr;if(SUCCEEDED(dialog->GetResult(&item))){add(item);item->Release();}}
  }
  dialog->Release();return results;
}
void layout() {
  RECT r;GetClientRect(window,&r);int w=r.right,h=r.bottom;
  MoveWindow(homeOpen,w/2-px(162),h/2-px(75),px(160),px(130),TRUE);
  MoveWindow(homeCreate,w/2+px(2),h/2-px(75),px(160),px(130),TRUE);
  MoveWindow(logo,px(20),px(20),px(60),px(60),TRUE);
  MoveWindow(statusText,px(15),h-px(28),std::max(px(100),w-px(30)),px(24),TRUE);
  const int ids[]={Open,CreateFiles,CreateFolder,Extract,Test,Cancel,Refresh,Home};
  for(int i=0;i<8;++i)MoveWindow(GetDlgItem(window,ids[i]+1000),px(10+i*86),px(12),px(80),px(30),TRUE);
  MoveWindow(pathText,px(12),px(56),w-px(24),px(24),TRUE);
  MoveWindow(passwordLabel,px(12),px(91),px(70),px(24),TRUE);
  MoveWindow(password,px(85),px(86),px(220),px(28),TRUE);
  int treeWidth=treeVisible?px(220):0;
  MoveWindow(tree,px(12),px(124),px(212),std::max(px(60),h-px(170)),TRUE);
  MoveWindow(list,px(12)+treeWidth,px(124),std::max(px(100),w-px(24)-treeWidth),std::max(px(60),h-px(170)),TRUE);
  ShowWindow(tree,!home&&treeVisible?SW_SHOW:SW_HIDE);
}
void showHome(bool value) {
  home=value;
  for(auto c:workspace)ShowWindow(c,value?SW_HIDE:SW_SHOW);
  for(auto c:{logo,homeOpen,homeCreate})ShowWindow(c,value?SW_SHOW:SW_HIDE);
  layout();InvalidateRect(window,nullptr,TRUE);
}
void updateDialog(HWND dialog) {
  int fmt=static_cast<int>(SendDlgItemMessageW(dialog,IDC_FORMAT,CB_GETCURSEL,0,0));
  bool tar=fmt==2, seven=fmt==1;
  EnableWindow(GetDlgItem(dialog,IDC_SPLIT),!tar);
  int selected=static_cast<int>(SendDlgItemMessageW(dialog,IDC_SPLIT,CB_GETCURSEL,0,0));
  EnableWindow(GetDlgItem(dialog,IDC_CUSTOM_SPLIT),!tar&&selected==static_cast<int>(kz::customSplitIndex));
  EnableWindow(GetDlgItem(dialog,IDC_PASSWORD),!tar);
  EnableWindow(GetDlgItem(dialog,IDC_LEVEL),!tar);
  EnableWindow(GetDlgItem(dialog,IDC_HIDE_NAMES),seven);
  if(!seven)CheckDlgButton(dialog,IDC_HIDE_NAMES,BST_UNCHECKED);
}
void suggestOutput(HWND dialog,CreateRequest& request) {
  auto current=text(GetDlgItem(dialog,IDC_OUTPUT));
  if(!current.empty()&&current!=request.suggested)return;
  int fmt=static_cast<int>(SendDlgItemMessageW(dialog,IDC_FORMAT,CB_GETCURSEL,0,0));
  request.suggested=request.inputs.empty()?L"":kz::suggestedArchive(request.inputs,fmt==1?"7z":fmt==2?"tar":"zip").wstring();
  SetDlgItemTextW(dialog,IDC_OUTPUT,request.suggested.c_str());
}
INT_PTR CALLBACK createProcedure(HWND dialog,UINT msg,WPARAM wp,LPARAM lp) {
  auto request=reinterpret_cast<CreateRequest*>(GetWindowLongPtrW(dialog,DWLP_USER));
  if(msg==WM_INITDIALOG) {
    request=reinterpret_cast<CreateRequest*>(lp);SetWindowLongPtrW(dialog,DWLP_USER,lp);
    for(auto s:{L"ZIP",L"7Z",L"TAR"})SendDlgItemMessageW(dialog,IDC_FORMAT,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(s));
    SendDlgItemMessageW(dialog,IDC_FORMAT,CB_SETCURSEL,0,0);
    for(const auto& preset:kz::splitPresets) {
      auto label=kz::widen(preset.label);SendDlgItemMessageW(dialog,IDC_SPLIT,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(label.c_str()));
    }
    SendDlgItemMessageW(dialog,IDC_SPLIT,CB_SETCURSEL,0,0);
    SendDlgItemMessageW(dialog,IDC_SPLIT,CB_SETMINVISIBLE,21,0);
    for(auto s:{L"압축하지 않음",L"빠르게",L"보통",L"최대로"})SendDlgItemMessageW(dialog,IDC_LEVEL,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(s));
    SendDlgItemMessageW(dialog,IDC_LEVEL,CB_SETCURSEL,preferences.level,0);CheckDlgButton(dialog,IDC_TEST_AFTER,preferences.test?BST_CHECKED:BST_UNCHECKED);
    SendDlgItemMessageW(dialog,IDC_CUSTOM_SPLIT,EM_SETCUEBANNER,TRUE,reinterpret_cast<LPARAM>(L"예: 100m, 2g"));
    for(const auto& p:request->inputs)SendDlgItemMessageW(dialog,IDC_INPUTS,LB_ADDSTRING,0,reinterpret_cast<LPARAM>(p.c_str()));
    if(!request->inputs.empty()) {
      auto output=kz::suggestedArchive(request->inputs);request->suggested=output.wstring();
      SetDlgItemTextW(dialog,IDC_OUTPUT,output.c_str());
    }
    RECT parent,rect;GetWindowRect(window,&parent);GetWindowRect(dialog,&rect);
    SetWindowPos(dialog,nullptr,parent.left+(parent.right-parent.left-(rect.right-rect.left))/2,parent.top+(parent.bottom-parent.top-(rect.bottom-rect.top))/2,0,0,SWP_NOSIZE|SWP_NOZORDER);
    updateDialog(dialog);return TRUE;
  }
  if(msg==WM_CLOSE){SetDlgItemTextW(dialog,IDC_PASSWORD,L"");EndDialog(dialog,IDCANCEL);return TRUE;}
  if(msg!=WM_COMMAND || !request)return FALSE;
  int id=LOWORD(wp);
  try {
    if(id==IDCANCEL){SetDlgItemTextW(dialog,IDC_PASSWORD,L"");EndDialog(dialog,IDCANCEL);return TRUE;}
    if((id==IDC_FORMAT||id==IDC_SPLIT)&&HIWORD(wp)==CBN_SELCHANGE) {
      if(id==IDC_FORMAT) {
        auto output=kz::fs::path(text(GetDlgItem(dialog,IDC_OUTPUT)));
        int index=static_cast<int>(SendDlgItemMessageW(dialog,IDC_FORMAT,CB_GETCURSEL,0,0));
        if(!output.empty()){bool automatic=output.wstring()==request->suggested;output.replace_extension(index==1?L".7z":index==2?L".tar":L".zip");SetDlgItemTextW(dialog,IDC_OUTPUT,output.c_str());if(automatic)request->suggested=output.wstring();}
      }
      updateDialog(dialog);return TRUE;
    }
    if(id==IDC_ADD_FILES||id==IDC_ADD_FOLDER) {
      auto files=choose(id==IDC_ADD_FOLDER,id==IDC_ADD_FILES,false,L"",dialog);
      for(const auto& p:files) {
        if(std::find(request->inputs.begin(),request->inputs.end(),p)!=request->inputs.end())continue;
        request->inputs.push_back(p);SendDlgItemMessageW(dialog,IDC_INPUTS,LB_ADDSTRING,0,reinterpret_cast<LPARAM>(p.c_str()));
      }
      suggestOutput(dialog,*request);
      return TRUE;
    }
    if(id==IDC_CLEAR_INPUTS){request->inputs.clear();SendDlgItemMessageW(dialog,IDC_INPUTS,LB_RESETCONTENT,0,0);suggestOutput(dialog,*request);return TRUE;}
    if(id==IDC_BROWSE_OUTPUT) {
      int fmt=static_cast<int>(SendDlgItemMessageW(dialog,IDC_FORMAT,CB_GETCURSEL,0,0));
      auto output=text(GetDlgItem(dialog,IDC_OUTPUT));
      auto files=choose(false,false,true,output.empty()?kz::suggestedArchive(request->inputs,fmt==1?"7z":fmt==2?"tar":"zip").filename().wstring():output,dialog);
      if(!files.empty())SetDlgItemTextW(dialog,IDC_OUTPUT,files[0].c_str());return TRUE;
    }
    if(id==IDOK) {
      int fmt=static_cast<int>(SendDlgItemMessageW(dialog,IDC_FORMAT,CB_GETCURSEL,0,0));
      kz::Options options;options.unicodeNames=preferences.unicodeNames;options.storeTimes=preferences.storeTimes;options.excludeThumbs=preferences.excludeThumbs;options.multiThread=preferences.parallel;options.format=fmt==1?"7z":fmt==2?"tar":"zip";
      int levels[]={0,1,5,9};int level=static_cast<int>(SendDlgItemMessageW(dialog,IDC_LEVEL,CB_GETCURSEL,0,0));
      options.level=levels[std::clamp(level,0,3)];
      if(fmt!=2) {
        options.password=kz::narrow(text(GetDlgItem(dialog,IDC_PASSWORD)));
        options.split=kz::splitArgument(static_cast<size_t>(SendDlgItemMessageW(dialog,IDC_SPLIT,CB_GETCURSEL,0,0)),kz::narrow(text(GetDlgItem(dialog,IDC_CUSTOM_SPLIT))));
        options.hideNames=fmt==1&&IsDlgButtonChecked(dialog,IDC_HIDE_NAMES)==BST_CHECKED;
      }
      auto path=text(GetDlgItem(dialog,IDC_OUTPUT));
      if(path.empty())throw std::runtime_error("저장할 파일 이름을 지정하세요.");
      request->output=kz::fs::absolute(kz::fs::path(path));
      if(!kz::fs::is_directory(request->output.parent_path()))throw std::runtime_error("저장할 상위 폴더가 존재하지 않습니다.");
      if(request->output.filename().empty())throw std::runtime_error("파일 이름이 필요합니다.");
      kz::createArgs(options,request->output,request->inputs);
      request->options=std::move(options);request->testAfter=IsDlgButtonChecked(dialog,IDC_TEST_AFTER)==BST_CHECKED;
      SetDlgItemTextW(dialog,IDC_PASSWORD,L"");EndDialog(dialog,IDOK);return TRUE;
    }
  }catch(const std::exception& e){SetDlgItemTextW(dialog,IDC_DIALOG_ERROR,kz::widen(e.what()).c_str());}
  return FALSE;
}
std::string normalized(std::string name) {
  std::replace(name.begin(),name.end(),'\\','/');
  while(!name.empty()&&name.back()=='/')name.pop_back();
  return name;
}
void displayFolder(const std::string& folder) {
  currentFolder=folder;std::map<std::string,kz::Entry> children;
  std::string prefix=folder.empty()?"":folder+"/";
  for(const auto& original:archiveEntries){
    auto name=normalized(original.name);if(name.size()<=prefix.size()||name.compare(0,prefix.size(),prefix)!=0)continue;
    auto tail=name.substr(prefix.size());auto slash=tail.find('/');
    auto childName=tail.substr(0,slash);auto fullName=prefix+childName;
    if(slash!=std::string::npos){if(!children.count(childName)){kz::Entry e;e.name=fullName;e.directory=true;children[childName]=e;}}
    else{auto e=original;e.name=fullName;children[childName]=e;}
  }
  visibleEntries.clear();
  for(bool directories:{true,false})for(const auto& pair:children)if(pair.second.directory==directories)visibleEntries.push_back(pair.second);
  SendMessageW(list,WM_SETREDRAW,FALSE,0);ListView_DeleteAllItems(list);
  for(size_t i=0;i<visibleEntries.size();++i){
    const auto& e=visibleEntries[i];auto name=e.name.substr(prefix.size());
    std::wstring values[]={kz::widen((e.directory?"[폴더] ":"")+name),e.directory?L"":std::to_wstring(e.size),e.directory?L"":std::to_wstring(e.packed),kz::widen(e.modified),e.encrypted?L"암호":L""};
    LVITEMW item{};item.mask=LVIF_TEXT;item.iItem=static_cast<int>(i);item.pszText=values[0].data();ListView_InsertItem(list,&item);
    for(int j=1;j<5;++j)ListView_SetItemText(list,static_cast<int>(i),j,values[j].data());
  }
  SendMessageW(list,WM_SETREDRAW,TRUE,0);InvalidateRect(list,nullptr,TRUE);
}
void display(const std::vector<kz::Entry>& entries) {
  archiveEntries=entries;buildingTree=true;TreeView_DeleteAllItems(tree);treePaths.clear();treeItems.clear();
  auto insert=[&](const std::string& path,const std::wstring& label,HTREEITEM parent){
    TVINSERTSTRUCTW item{};item.hParent=parent;item.hInsertAfter=TVI_LAST;item.item.mask=TVIF_TEXT|TVIF_PARAM;
    item.item.pszText=const_cast<wchar_t*>(label.c_str());item.item.lParam=static_cast<LPARAM>(treePaths.size());treePaths.push_back(path);
    auto handle=TreeView_InsertItem(tree,&item);treeItems[path]=handle;return handle;
  };
  auto root=insert("",archive.filename().wstring(),TVI_ROOT);
  std::map<std::string,bool> folders;
  for(const auto& e:entries){
    auto path=normalized(e.name);if(e.directory)folders[path]=true;
    size_t offset=0;while((offset=path.find('/',offset))!=std::string::npos){folders[path.substr(0,offset)]=true;++offset;}
  }
  for(const auto& folder:folders){
    auto slash=folder.first.rfind('/');auto parent=slash==std::string::npos?"":folder.first.substr(0,slash);
    auto label=slash==std::string::npos?folder.first:folder.first.substr(slash+1);
    insert(folder.first,kz::widen(label),treeItems.at(parent));
  }
  TreeView_Expand(tree,root,TVE_EXPAND);buildingTree=false;TreeView_SelectItem(tree,root);displayFolder("");
}
std::chrono::steady_clock::time_point jobStarted,phaseStarted;
int lastPhase=-1, lastProgress=-2;
bool progressMarquee=false;
std::wstring durationText(long long seconds) {
  return std::to_wstring(seconds/60)+L"분 "+std::to_wstring(seconds%60)+L"초";
}
INT_PTR CALLBACK progressProcedure(HWND dialog,UINT msg,WPARAM wp,LPARAM) {
  if(msg==WM_INITDIALOG){
    lastProgress=-2;progressMarquee=false;
    SendDlgItemMessageW(dialog,IDC_PROGRESS_BAR,PBM_SETRANGE32,0,100);
    RECT owner{},rect{},work{};GetWindowRect(window,&owner);GetWindowRect(dialog,&rect);
    MONITORINFO monitor{sizeof(monitor)};GetMonitorInfoW(MonitorFromWindow(window,MONITOR_DEFAULTTONEAREST),&monitor);work=monitor.rcWork;
    int width=rect.right-rect.left,height=rect.bottom-rect.top;
    int x=std::max(work.left,std::min(owner.left+(owner.right-owner.left-width)/2,work.right-width));
    int y=std::max(work.top,std::min(owner.top+(owner.bottom-owner.top-height)/2,work.bottom-height));
    SetWindowPos(dialog,nullptr,x,y,0,0,SWP_NOSIZE|SWP_NOZORDER);
    SetTimer(dialog,1,250,nullptr);return TRUE;
  }
  if(msg==WM_TIMER&&busy&&engine){
    const auto now=std::chrono::steady_clock::now();
    const int phase=engine->phase(),percent=engine->progress();
    if(phase!=lastPhase){lastPhase=phase;phaseStarted=now;}
    const wchar_t* label=phase==1?L"압축 중":phase==2?L"압축 해제 중":phase==3?L"검사 중":L"파일 확인 중";
    auto title=std::wstring(label)+(percent>=0?L" · "+std::to_wstring(percent)+L"%":L"...");

    HWND bar=GetDlgItem(dialog,IDC_PROGRESS_BAR);
    bool marquee=percent<0;
    if(marquee!=progressMarquee){
      auto style=GetWindowLongPtrW(bar,GWL_STYLE);
      if(!marquee)SendMessageW(bar,PBM_SETMARQUEE,FALSE,0);
      SetWindowLongPtrW(bar,GWL_STYLE,marquee?style|PBS_MARQUEE:style&~PBS_MARQUEE);
      if(marquee)SendMessageW(bar,PBM_SETMARQUEE,TRUE,40);
      progressMarquee=marquee;
    }
    if(percent>=0&&percent!=lastProgress){SendMessageW(bar,PBM_SETPOS,percent,0);lastProgress=percent;}
    auto elapsed=std::chrono::duration_cast<std::chrono::seconds>(now-jobStarted).count();
    auto stage=std::chrono::duration_cast<std::chrono::seconds>(now-phaseStarted).count();
    auto remaining=percent>0&&percent<100&&stage>=1?durationText(stage*(100-percent)/percent):percent==100?L"마무리 중":L"계산 중";
    title+=L" · 경과 "+durationText(elapsed);
    if(text(dialog)!=title)SetWindowTextW(dialog,title.c_str());
    auto info=std::wstring(label)+L" · 예상 남은 시간 "+remaining;
    if(!IsWindowEnabled(GetDlgItem(dialog,IDCANCEL)))info=L"취소 중 · 임시 파일을 정리하고 있습니다...";
    if(text(GetDlgItem(dialog,IDC_PROGRESS_STATUS))!=info)SetDlgItemTextW(dialog,IDC_PROGRESS_STATUS,info.c_str());return TRUE;
  }
  if(msg==WM_CLOSE||(msg==WM_COMMAND&&LOWORD(wp)==IDCANCEL)){
    if(busy){if(engine)engine->cancel();SetDlgItemTextW(dialog,IDC_PROGRESS_STATUS,L"취소 중...");EnableWindow(GetDlgItem(dialog,IDCANCEL),FALSE);}
    else{KillTimer(dialog,1);DestroyWindow(dialog);progressWindow=nullptr;EnableWindow(window,TRUE);SetActiveWindow(window);}
    return TRUE;
  }
  return FALSE;
}
void start(std::function<Result()> task) {
  if(busy)return;
  if(!engine)throw std::runtime_error("설정 → 압축 엔진 연결에서 공식 7z.exe를 선택하세요.");
  if(worker.joinable())worker.join();
  engine->configureRead(codePages[preferences.codePageIndex],preferences.parallel,preferences.zone);
  busy=true;engine->resetCancel();for(auto c:controls)EnableWindow(c,FALSE);
  EnableWindow(GetDlgItem(window,Cancel+1000),TRUE);SetWindowTextW(statusText,L"작업 중…  도구 메뉴에서 취소할 수 있습니다.");
  jobStarted=phaseStarted=std::chrono::steady_clock::now();lastPhase=-1;
  progressWindow=CreateDialogParamW(GetModuleHandleW(nullptr),MAKEINTRESOURCEW(IDD_PROGRESS),window,progressProcedure,0);
  if(progressWindow){EnableWindow(window,FALSE);ShowWindow(progressWindow,SW_SHOW);}
  worker=std::thread([task=std::move(task)]{
    auto result=std::make_unique<Result>();
    try{*result=task();}catch(const std::exception& e){result->message=e.what();result->success=false;}
    PostMessageW(window,Finished,0,reinterpret_cast<LPARAM>(result.release()));
  });
}
void reload() {
  if(archive.empty())return;
  const auto p=kz::narrow(text(password));const auto input=archive;showHome(false);
  start([=]{auto entries=engine->list(input,p);return Result{std::move(entries),true,"목록 읽기 완료"};});
}
void createArchive(std::vector<kz::fs::path> inputs={}) {
  CreateRequest request;request.inputs=std::move(inputs);
  if(DialogBoxParamW(GetModuleHandleW(nullptr),MAKEINTRESOURCEW(IDD_CREATE),window,createProcedure,reinterpret_cast<LPARAM>(&request))!=IDOK)return;
  start([request=std::move(request)] {
    auto destination=engine->create(request.options,request.output,request.inputs);
    if(request.testAfter) {
      auto output=destination;

      engine->test(output,request.options.password);
    }
    return Result{{},false,"압축 완료"+std::string(request.testAfter?" · 검사 통과: ":": ")+destination.u8string()};
  });
}
void copyNames() {
  std::wstring names;int row=-1;
  while((row=ListView_GetNextItem(list,row,LVNI_SELECTED))!=-1){
    names+=kz::widen(visibleEntries.at(static_cast<size_t>(row)).name);names+=L"\r\n";
  }
  if(names.empty()||!OpenClipboard(window))return;
  auto memory=GlobalAlloc(GMEM_MOVEABLE,(names.size()+1)*sizeof(wchar_t));
  if(memory){auto p=GlobalLock(memory);if(p){memcpy(p,names.c_str(),(names.size()+1)*sizeof(wchar_t));GlobalUnlock(memory);EmptyClipboard();if(!SetClipboardData(CF_UNICODETEXT,memory))GlobalFree(memory);}else GlobalFree(memory);}
  CloseClipboard();
}
INT_PTR CALLBACK findProcedure(HWND dialog,UINT msg,WPARAM wp,LPARAM) {
  if(msg==WM_INITDIALOG)return TRUE;
  if(msg==WM_CLOSE){EndDialog(dialog,IDCANCEL);return TRUE;}
  if(msg!=WM_COMMAND)return FALSE;
  if(LOWORD(wp)==IDCANCEL){EndDialog(dialog,IDCANCEL);return TRUE;}
  if(LOWORD(wp)==IDOK) {
    const auto query=text(GetDlgItem(dialog,IDC_FIND_TEXT));if(query.empty())return TRUE;
    const int count=ListView_GetItemCount(list),selected=ListView_GetNextItem(list,-1,LVNI_SELECTED);
    for(int n=1;n<=count;++n){
      const int row=(selected+n+count)%count;
      wchar_t name[32768];ListView_GetItemText(list,row,0,name,32768);
      if(FindNLSStringEx(LOCALE_NAME_INVARIANT,FIND_FROMSTART|NORM_IGNORECASE,name,-1,query.c_str(),static_cast<int>(query.size()),nullptr,nullptr,nullptr,0)>=0){
        showHome(false);ListView_SetItemState(list,-1,0,LVIS_SELECTED|LVIS_FOCUSED);
        ListView_SetItemState(list,row,LVIS_SELECTED|LVIS_FOCUSED,LVIS_SELECTED|LVIS_FOCUSED);ListView_EnsureVisible(list,row,FALSE);
        SetDlgItemTextW(dialog,IDC_FIND_RESULT,name);return TRUE;
      }
    }
    SetDlgItemTextW(dialog,IDC_FIND_RESULT,L"일치하는 파일이 없습니다.");return TRUE;
  }
  return FALSE;
}
void settingsPage(HWND dialog) {
  int page=static_cast<int>(SendDlgItemMessageW(dialog,IDC_CATEGORIES,LB_GETCURSEL,0,0));
  const wchar_t* titles[]={L"일반 설정",L"파일 연결",L"탐색기 메뉴",L"압축 풀기",L"압축하기",L"보기/편집",L"보안 설정",L"고급 설정",L"언어 설정"};
  SetDlgItemTextW(dialog,IDC_SETTINGS_TITLE,titles[std::clamp(page,0,8)]);
  for(int id:{IDC_PREF_ESCAPE,IDC_PREF_GRID,IDC_PREF_FULLROW,IDC_PREF_HOME})ShowWindow(GetDlgItem(dialog,id),page==0?SW_SHOW:SW_HIDE);
  for(int id:{IDC_PREF_TEST,IDC_PREF_LEVEL,IDC_PREF_UNICODE,IDC_PREF_TIMES,IDC_PREF_THUMBS})ShowWindow(GetDlgItem(dialog,id),page==4?SW_SHOW:SW_HIDE);
  for(int id:{IDC_ASSOC_LIST,IDC_ASSOC_APPLY,IDC_DEFAULT_APPS})ShowWindow(GetDlgItem(dialog,id),page==1?SW_SHOW:SW_HIDE);
  for(int id:{IDC_SHELL_INSTALL,IDC_SHELL_REMOVE})ShowWindow(GetDlgItem(dialog,id),page==2?SW_SHOW:SW_HIDE);
  for(int id:{IDC_PREF_TREE,IDC_PREF_STATUS,IDC_PREF_COLOR})ShowWindow(GetDlgItem(dialog,id),page==5?SW_SHOW:SW_HIDE);
  for(int id:{IDC_PREF_PARALLEL,IDC_PREF_CODEPAGE})ShowWindow(GetDlgItem(dialog,id),page==3?SW_SHOW:SW_HIDE);
  ShowWindow(GetDlgItem(dialog,IDC_PREF_ZONE),page==6?SW_SHOW:SW_HIDE);
  ShowWindow(GetDlgItem(dialog,IDC_PREF_EXPORT),page==7?SW_SHOW:SW_HIDE);
  const wchar_t* notes[]={
    L"설정은 현재 Windows 사용자에게 저장됩니다. 확인을 누르면 즉시 적용됩니다.",
    L"확장자를 선택하고 연결 프로그램 등록을 누르면 즉시 적용됩니다. 기본 앱 변경은 Windows 기본 앱 설정에서 선택하세요.",
    L"등록/제거 버튼은 즉시 적용됩니다. 압축 옵션 창과 파일명.zip 바로 압축 메뉴를 등록합니다.\n\nWindows 11 첫 우클릭 메뉴와 세부 항목 설정은 개발 중입니다.",
    L"선택한 폴더에 압축 내용을 직접 해제합니다. 같은 이름은 덮어쓰지 않습니다.\n\nZIP 코드페이지는 기존 ZIP의 파일명 해석에 적용됩니다. UTF-8 표시가 있는 파일은 원래 인코딩을 사용합니다. 멀티코어 활용 범위는 압축 방식에 따라 다릅니다.",
    L"선택한 압축 방법과 검사 옵션이 새 압축 창의 기본값으로 적용됩니다.",
    L"폴더 트리, 상태 표시줄, 테마 색상을 선택하고 확인하면 저장됩니다. 눈금선과 행 선택은 일반 설정에서 변경합니다.\n\n이미지 미리보기 및 글꼴 선택은 개발 중입니다.",
    L"해제 전 경로 탈출, 링크, 중복 경로를 검사합니다.\n\n다운로드 출처(Zone.Identifier)를 지원하는 파일 시스템에서 원본 정보를 전달합니다. 악성코드 검사는 아직 구현되지 않았습니다.",
    L"현재 한도: 항목 100,000개 / 선언된 해제 크기 20 GiB.\n\n환경설정을 INI 텍스트로 내보낼 수 있습니다. 암호는 포함하지 않습니다. 임시 폴더 및 상세 성능 설정은 개발 중입니다.",
    L"한국어\n\n추가 언어 번역은 개발 중입니다."
  };
  SetDlgItemTextW(dialog,IDC_SETTINGS_NOTE,notes[std::clamp(page,0,8)]);
}
void fillSettings(HWND dialog,const PreferencesData& p) {
  pendingTheme=p.theme;
  CheckDlgButton(dialog,IDC_PREF_UNICODE,p.unicodeNames?BST_CHECKED:BST_UNCHECKED);
  CheckDlgButton(dialog,IDC_PREF_TIMES,p.storeTimes?BST_CHECKED:BST_UNCHECKED);
  CheckDlgButton(dialog,IDC_PREF_THUMBS,p.excludeThumbs?BST_CHECKED:BST_UNCHECKED);
  CheckDlgButton(dialog,IDC_PREF_PARALLEL,p.parallel?BST_CHECKED:BST_UNCHECKED);
  CheckDlgButton(dialog,IDC_PREF_ZONE,p.zone?BST_CHECKED:BST_UNCHECKED);
  SendDlgItemMessageW(dialog,IDC_PREF_CODEPAGE,CB_SETCURSEL,p.codePageIndex,0);
  CheckDlgButton(dialog,IDC_PREF_TREE,p.showTree?BST_CHECKED:BST_UNCHECKED);
  CheckDlgButton(dialog,IDC_PREF_STATUS,p.showStatus?BST_CHECKED:BST_UNCHECKED);
  CheckDlgButton(dialog,IDC_PREF_ESCAPE,p.escape?BST_CHECKED:BST_UNCHECKED);
  CheckDlgButton(dialog,IDC_PREF_GRID,p.grid?BST_CHECKED:BST_UNCHECKED);
  CheckDlgButton(dialog,IDC_PREF_FULLROW,p.fullRow?BST_CHECKED:BST_UNCHECKED);
  CheckDlgButton(dialog,IDC_PREF_HOME,p.startHome?BST_CHECKED:BST_UNCHECKED);
  CheckDlgButton(dialog,IDC_PREF_TEST,p.test?BST_CHECKED:BST_UNCHECKED);
  SendDlgItemMessageW(dialog,IDC_PREF_LEVEL,CB_SETCURSEL,p.level,0);
}
INT_PTR CALLBACK settingsProcedure(HWND dialog,UINT msg,WPARAM wp,LPARAM) {
  if(msg==WM_INITDIALOG){
    for(auto title:{L"일반 설정",L"파일 연결",L"탐색기 메뉴",L"압축 풀기",L"압축하기",L"보기/편집",L"보안 설정",L"고급 설정",L"언어 설정"})
      SendDlgItemMessageW(dialog,IDC_CATEGORIES,LB_ADDSTRING,0,reinterpret_cast<LPARAM>(title));
    for(auto title:{L"압축하지 않음",L"빠르게",L"보통",L"최대로"})
      SendDlgItemMessageW(dialog,IDC_PREF_LEVEL,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(title));
    for(auto label:{L"ZIP 코드페이지: 자동",L"한국어 (949)",L"일본어 (932)",L"중국어 간체 (936)",L"중국어 번체 (950)",L"서유럽어 (1252)",L"UTF-8 (65001)"})
      SendDlgItemMessageW(dialog,IDC_PREF_CODEPAGE,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(label));
    for(size_t i=0;i<kz::win::extensions().size();++i){
      const auto& ext=kz::win::extensions()[i];
      SendDlgItemMessageW(dialog,IDC_ASSOC_LIST,LB_ADDSTRING,0,reinterpret_cast<LPARAM>(ext.c_str()));
      SendDlgItemMessageW(dialog,IDC_ASSOC_LIST,LB_SETSEL,kz::win::associated(ext),i);
    }
    SendDlgItemMessageW(dialog,IDC_CATEGORIES,LB_SETCURSEL,0,0);fillSettings(dialog,preferences);settingsPage(dialog);return TRUE;
  }
  if(msg==WM_CTLCOLORLISTBOX&&reinterpret_cast<HWND>(wp)){
    SetBkColor(reinterpret_cast<HDC>(wp),RGB(255,225,122));SetTextColor(reinterpret_cast<HDC>(wp),RGB(65,48,12));
    SetDCBrushColor(reinterpret_cast<HDC>(wp),RGB(255,225,122));return reinterpret_cast<INT_PTR>(GetStockObject(DC_BRUSH));
  }
  if(msg==WM_CLOSE){EndDialog(dialog,IDCANCEL);return TRUE;}
  if(msg!=WM_COMMAND)return FALSE;
  if(LOWORD(wp)==IDC_CATEGORIES&&HIWORD(wp)==LBN_SELCHANGE){settingsPage(dialog);return TRUE;}
  if(LOWORD(wp)==IDC_ASSOC_APPLY){
    try{
      std::vector<bool> selected;for(size_t i=0;i<kz::win::extensions().size();++i)selected.push_back(SendDlgItemMessageW(dialog,IDC_ASSOC_LIST,LB_GETSEL,i,0)>0);
      kz::win::associations(selected);SetDlgItemTextW(dialog,IDC_SETTINGS_NOTE,L"연결 프로그램 목록에 등록했습니다. 기본 앱 변경은 Windows 설정에서 선택하세요.");
    }catch(const std::exception& e){SetDlgItemTextW(dialog,IDC_SETTINGS_NOTE,kz::widen(e.what()).c_str());}return TRUE;
  }
  if(LOWORD(wp)==IDC_DEFAULT_APPS){ShellExecuteW(dialog,L"open",L"ms-settings:defaultapps",nullptr,nullptr,SW_SHOWNORMAL);return TRUE;}
  if(LOWORD(wp)==IDC_SHELL_INSTALL||LOWORD(wp)==IDC_SHELL_REMOVE){
    try{kz::win::shellRegistration(LOWORD(wp)==IDC_SHELL_INSTALL);SetDlgItemTextW(dialog,IDC_SETTINGS_NOTE,L"탐색기 메뉴 설정을 적용했습니다. 새 우클릭 메뉴부터 적용됩니다.");}
    catch(const std::exception& e){SetDlgItemTextW(dialog,IDC_SETTINGS_NOTE,kz::widen(e.what()).c_str());}return TRUE;
  }
  if(LOWORD(wp)==IDC_PREF_COLOR){
    static COLORREF custom[16]{};CHOOSECOLORW choice{sizeof(choice)};choice.hwndOwner=dialog;choice.rgbResult=pendingTheme;choice.lpCustColors=custom;choice.Flags=CC_FULLOPEN|CC_RGBINIT;
    if(ChooseColorW(&choice))pendingTheme=choice.rgbResult;return TRUE;
  }
  if(LOWORD(wp)==IDC_PREF_EXPORT){
    try{
      auto paths=choose(false,false,true,L"EoriniZip-settings.ini",dialog);if(paths.empty())return TRUE;
      std::ofstream output(paths[0],std::ios::binary);
      output<<"[EoriniZip]\nVersion=1\nEscape="<<preferences.escape<<"\nGrid="<<preferences.grid<<"\nFullRow="<<preferences.fullRow<<"\nStartHome="<<preferences.startHome
        <<"\nTestAfter="<<preferences.test<<"\nLevel="<<preferences.level<<"\nShowTree="<<preferences.showTree<<"\nShowStatus="<<preferences.showStatus<<"\nTheme="<<preferences.theme
        <<"\nUnicodeNames="<<preferences.unicodeNames<<"\nStoreTimes="<<preferences.storeTimes<<"\nExcludeThumbs="<<preferences.excludeThumbs<<"\nParallel="<<preferences.parallel
        <<"\nPropagateZone="<<preferences.zone<<"\nCodePageIndex="<<preferences.codePageIndex<<"\n";
      if(!output)throw std::runtime_error("설정 파일을 저장할 수 없습니다.");
      SetDlgItemTextW(dialog,IDC_SETTINGS_NOTE,L"현재 저장된 환경설정을 내보냈습니다.");
    }catch(const std::exception& e){SetDlgItemTextW(dialog,IDC_SETTINGS_NOTE,kz::widen(e.what()).c_str());}return TRUE;
  }
  if(LOWORD(wp)==IDC_PREF_RESET){fillSettings(dialog,PreferencesData{});return TRUE;}
  if(LOWORD(wp)==IDCANCEL){EndDialog(dialog,IDCANCEL);return TRUE;}
  if(LOWORD(wp)==IDOK){
    auto previous=preferences;
    preferences.unicodeNames=IsDlgButtonChecked(dialog,IDC_PREF_UNICODE)==BST_CHECKED;
    preferences.storeTimes=IsDlgButtonChecked(dialog,IDC_PREF_TIMES)==BST_CHECKED;
    preferences.excludeThumbs=IsDlgButtonChecked(dialog,IDC_PREF_THUMBS)==BST_CHECKED;
    preferences.parallel=IsDlgButtonChecked(dialog,IDC_PREF_PARALLEL)==BST_CHECKED;
    preferences.zone=IsDlgButtonChecked(dialog,IDC_PREF_ZONE)==BST_CHECKED;
    preferences.codePageIndex=std::clamp(static_cast<int>(SendDlgItemMessageW(dialog,IDC_PREF_CODEPAGE,CB_GETCURSEL,0,0)),0,6);preferences.showTree=IsDlgButtonChecked(dialog,IDC_PREF_TREE)==BST_CHECKED;preferences.showStatus=IsDlgButtonChecked(dialog,IDC_PREF_STATUS)==BST_CHECKED;preferences.theme=pendingTheme;
    preferences.escape=IsDlgButtonChecked(dialog,IDC_PREF_ESCAPE)==BST_CHECKED;
    preferences.grid=IsDlgButtonChecked(dialog,IDC_PREF_GRID)==BST_CHECKED;
    preferences.fullRow=IsDlgButtonChecked(dialog,IDC_PREF_FULLROW)==BST_CHECKED;
    preferences.startHome=IsDlgButtonChecked(dialog,IDC_PREF_HOME)==BST_CHECKED;
    preferences.test=IsDlgButtonChecked(dialog,IDC_PREF_TEST)==BST_CHECKED;
    preferences.level=static_cast<int>(SendDlgItemMessageW(dialog,IDC_PREF_LEVEL,CB_GETCURSEL,0,0));
    if(!savePreferences()){preferences=previous;MessageBoxW(dialog,L"환경 설정을 저장할 수 없습니다.",L"어린이zip",MB_OK|MB_ICONERROR);return TRUE;}
    ListView_SetExtendedListViewStyle(list,LVS_EX_DOUBLEBUFFER|(preferences.fullRow?LVS_EX_FULLROWSELECT:0)|(preferences.grid?LVS_EX_GRIDLINES:0));
    treeVisible=preferences.showTree;statusVisible=preferences.showStatus;ShowWindow(statusText,statusVisible?SW_SHOW:SW_HIDE);layout();InvalidateRect(window,nullptr,TRUE);
    EndDialog(dialog,IDOK);return TRUE;
  }
  return FALSE;
}
void command(int id) {
  if(id==Cancel){if(engine)engine->cancel();return;}
  if(busy)return;
  try {
    if(id==ToggleTree){treeVisible=!treeVisible;layout();return;}
    if(id==Preferences){DialogBoxParamW(GetModuleHandleW(nullptr),MAKEINTRESOURCEW(IDD_SETTINGS),window,settingsProcedure,0);return;}
    if(id==Exit){SendMessageW(window,WM_CLOSE,0,0);return;}
    if(id==About){MessageBoxW(window,L"어린이zip · 네이티브 개발 버전\n\nZIP / 7Z / TAR 압축 및 해제\nALZ / EGG / ZPAQ, LOCK / EZC 복호화는 개발 예정입니다.\n\n분할 메뉴의 서비스 이름은 용량 프리셋이며 현재 서비스의 업로드 한도를 보장하지 않습니다.",L"어린이zip 정보",MB_OK);return;}
    if(id==Home){showHome(!home);return;}
    if(id==CloseArchive){archive.clear();archiveEntries.clear();visibleEntries.clear();TreeView_DeleteAllItems(tree);ListView_DeleteAllItems(list);SetWindowTextW(pathText,L"");SetWindowTextW(password,L"");showHome(true);return;}
    if(id==Topmost){onTop=!onTop;SetWindowPos(window,onTop?HWND_TOPMOST:HWND_NOTOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE);return;}
    if(id==ToggleStatus){statusVisible=!statusVisible;ShowWindow(statusText,statusVisible?SW_SHOW:SW_HIDE);return;}
    if(id==NewWindow||id==AppFolder){
      wchar_t executable[32768];GetModuleFileNameW(nullptr,executable,32768);
      auto target=id==NewWindow?kz::fs::path(executable):kz::fs::path(executable).parent_path();
      if(reinterpret_cast<INT_PTR>(ShellExecuteW(window,L"open",target.c_str(),nullptr,nullptr,SW_SHOWNORMAL))<=32)throw std::runtime_error("창을 열 수 없습니다.");
      return;
    }
    if(id==HelpPage||id==IssuesPage||id==Website){
      const wchar_t* target=id==HelpPage?L"https://github.com/sjhdeux0114/EoriniZip#readme":id==IssuesPage?L"https://github.com/sjhdeux0114/EoriniZip/issues":L"https://github.com/sjhdeux0114/EoriniZip";
      ShellExecuteW(window,L"open",target,nullptr,nullptr,SW_SHOWNORMAL);return;
    }
    if(id==FindFile){if(archive.empty())return;DialogBoxParamW(GetModuleHandleW(nullptr),MAKEINTRESOURCEW(IDD_FIND),window,findProcedure,0);return;}
    if(id==SelectAll){ListView_SetItemState(list,-1,LVIS_SELECTED,LVIS_SELECTED);return;}
    if(id==CopyNames){copyNames();return;}
    if(id==Connect){auto files=choose(false);if(files.empty())return;engine=std::make_unique<kz::Engine>(files[0]);SetWindowTextW(statusText,L"7-Zip 엔진 연결됨");return;}
    if(id==Open){auto files=choose(false);if(files.empty())return;archive=files[0];ListView_DeleteAllItems(list);SetWindowTextW(pathText,archive.c_str());reload();return;}
    if(id==Refresh){reload();return;}
    if(id==CreateFiles){createArchive();return;}
    if(id==CreateFolder){auto inputs=choose(true);if(!inputs.empty())createArchive(inputs);return;}
    if(archive.empty())throw std::runtime_error("압축파일을 먼저 여세요.");
    if(id==OpenFolder){ShellExecuteW(window,L"open",archive.parent_path().c_str(),nullptr,nullptr,SW_SHOWNORMAL);return;}
    if(id==SaveAs){
      if(archive.extension()==L".001")throw std::runtime_error("분할 파일은 모든 볼륨을 함께 복사해야 합니다. 압축 파일이 있는 폴더 열기를 이용하세요.");
      auto paths=choose(false,false,true,archive.filename().wstring());if(paths.empty())return;
      if(!kz::fs::copy_file(archive,paths[0],kz::fs::copy_options::none))throw std::runtime_error("복사하지 못했습니다.");
      SetWindowTextW(statusText,L"다른 이름으로 저장 완료");return;
    }
    const auto p=kz::narrow(text(password));const auto input=archive;
    if(id==Extract){auto dirs=choose(true);if(dirs.empty())return;start([=]{return Result{{},false,"해제 완료: "+engine->extract(input,dirs[0],p).u8string()};});}
    else if(id==Test)start([=]{engine->test(input,p);return Result{{},false,"압축파일 검사 완료"};});
  }catch(const std::exception& e){MessageBoxW(window,kz::widen(e.what()).c_str(),L"어린이zip",MB_OK|MB_ICONINFORMATION);}
}
HMENU menus() {
  HMENU bar=CreateMenu();
  auto add=[&](const wchar_t* name){HMENU m=CreatePopupMenu();AppendMenuW(bar,MF_POPUP,reinterpret_cast<UINT_PTR>(m),name);return m;};
  auto item=[](HMENU m,UINT id,const wchar_t* label){AppendMenuW(m,MF_STRING,id,label);};
  auto pending=[](HMENU m,const wchar_t* label){AppendMenuW(m,MF_STRING|MF_GRAYED,0,label);};
  auto sep=[](HMENU m){AppendMenuW(m,MF_SEPARATOR,0,nullptr);};
  auto file=add(L"파일(&F)");
  item(file,Open,L"압축 파일 열기\tCtrl+O");pending(file,L"최근 파일");item(file,CreateFiles,L"새로 압축\tCtrl+N");sep(file);
  item(file,CloseArchive,L"압축 파일 닫기\tF4");item(file,SaveAs,L"다른 이름으로 저장...");
  pending(file,L"압축 파일 삭제\tCtrl+Del");pending(file,L"압축 파일 완전 삭제\tShift+Ctrl+Del");
  item(file,OpenFolder,L"압축 파일이 있는 폴더 열기");item(file,Test,L"압축 파일 테스트\tCtrl+T");
  pending(file,L"압축 파일 악성코드 검사\tCtrl+I");item(file,Extract,L"압축 풀기\tCtrl+E");sep(file);
  item(file,NewWindow,L"새창 띄우기\tCtrl+Shift+N");pending(file,L"새창 띄우기(관리자 권한)\tCtrl+Alt+N");sep(file);item(file,Exit,L"종료\tCtrl+W");
  auto edit=add(L"편집(&E)");
  pending(edit,L"이름 바꾸기\tF2");pending(edit,L"파일 추가\tIns");pending(edit,L"파일 삭제\tDel");pending(edit,L"압축 파일 설명 편집\tCtrl+Shift+M");sep(edit);
  pending(edit,L"클립보드로 파일 복사하기\tCtrl+C");pending(edit,L"클립보드에서 파일 붙여 넣기\tCtrl+V");
  item(edit,CopyNames,L"파일의 경로명을 클립보드로 복사");item(edit,SelectAll,L"모두 선택\tCtrl+A");
  auto find=add(L"찾기(&I)");item(find,FindFile,L"파일 찾기\tCtrl+F");
  auto settings=add(L"설정(&S)");item(settings,Preferences,L"환경 설정\tF5");item(settings,Connect,L"압축 엔진 연결...");pending(settings,L"암호 관리자...");
  auto view=add(L"보기(&V)");item(view,Home,L"시작 화면\tF6");item(view,ToggleTree,L"트리 컨트롤\tF7");pending(view,L"압축 파일 설명\tF8");
  pending(view,L"이미지 파일 미리 보기 사용");item(view,ToggleStatus,L"상태 표시줄");pending(view,L"툴바");pending(view,L"파일 목록");pending(view,L"칼럼 설정");sep(view);
  item(view,Topmost,L"항상 위에 표시");pending(view,L"색상 설정...");item(view,Refresh,L"압축 파일 다시 읽기");
  auto tools=add(L"도구(&T)");pending(tools,L"손상된 압축 파일 복구...");pending(tools,L"압축 파일 암호 복구...");sep(tools);
  pending(tools,L"AESLocker 파일 해제...");pending(tools,L"EasyCrypt 2.4 파일 해제...");sep(tools);item(tools,Cancel,L"작업 취소");
  auto help=add(L"도움말(&A)");item(help,About,L"정보\tF1");item(help,AppFolder,L"프로그램 설치 폴더 열기");sep(help);
  item(help,HelpPage,L"도움말 페이지 열기");item(help,IssuesPage,L"문의 / 오류 신고");item(help,Website,L"홈페이지 열기");return bar;
}
void updateMenus() {
  HMENU m=GetMenu(window);
  for(int id=Open;id<=ToggleTree;++id)EnableMenuItem(m,id,MF_BYCOMMAND|(busy&&id!=Cancel?MF_GRAYED:MF_ENABLED));
  for(int id:{CloseArchive,SaveAs,OpenFolder,Test,Extract,FindFile,SelectAll,CopyNames,Refresh})
    EnableMenuItem(m,id,MF_BYCOMMAND|(!busy&&!archive.empty()?MF_ENABLED:MF_GRAYED));
  CheckMenuItem(m,ToggleTree,MF_BYCOMMAND|(treeVisible?MF_CHECKED:MF_UNCHECKED));
  CheckMenuItem(m,Home,MF_BYCOMMAND|(home?MF_CHECKED:MF_UNCHECKED));
  CheckMenuItem(m,Topmost,MF_BYCOMMAND|(onTop?MF_CHECKED:MF_UNCHECKED));
  CheckMenuItem(m,ToggleStatus,MF_BYCOMMAND|(statusVisible?MF_CHECKED:MF_UNCHECKED));
}void paintHomeButton(DRAWITEMSTRUCT* d) {
  HDC dc=d->hDC;RECT r=d->rcItem;
  auto brush=CreateSolidBrush(d->itemState&ODS_SELECTED?RGB(255,241,184):RGB(255,255,250));
  FillRect(dc,&r,brush);DeleteObject(brush);
  auto pen=CreatePen(PS_SOLID,px(1),RGB(151,117,20));auto oldPen=SelectObject(dc,pen);auto oldBrush=SelectObject(dc,GetStockObject(NULL_BRUSH));
  Rectangle(dc,r.left,r.top,r.right,r.bottom);
  int x=(r.right+r.left)/2-px(14),y=r.top+px(28);
  Rectangle(dc,x-px(4),y-px(4),x+px(23),y+px(29));Rectangle(dc,x,y,x+px(27),y+px(33));
  if(d->CtlID==Open){MoveToEx(dc,x+px(6),y+px(17),nullptr);LineTo(dc,x+px(22),y+px(17));MoveToEx(dc,x+px(16),y+px(11),nullptr);LineTo(dc,x+px(22),y+px(17));LineTo(dc,x+px(16),y+px(23));}
  else {for(int i=0;i<5;++i){int xx=x+px(i%2?13:9);Rectangle(dc,xx,y+px(3+i*4),xx+px(5),y+px(7+i*4));}Rectangle(dc,x+px(9),y+px(24),x+px(18),y+px(30));}
  SelectObject(dc,oldBrush);SelectObject(dc,oldPen);DeleteObject(pen);
  SelectObject(dc,font);SetBkMode(dc,TRANSPARENT);SetTextColor(dc,d->itemState&ODS_DISABLED?RGB(150,140,110):RGB(65,48,12));
  r.top+=px(82);DrawTextW(dc,d->CtlID==Open?L"압축 파일 열기":L"새로 압축하기",-1,&r,DT_CENTER|DT_TOP|DT_SINGLELINE);
  if(d->itemState&ODS_FOCUS){r=d->rcItem;InflateRect(&r,-px(5),-px(5));DrawFocusRect(dc,&r);}
}
LRESULT CALLBACK procedure(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp) {
  switch(msg) {
    case WM_CREATE:{
      window=hwnd;loadPreferences();treeVisible=preferences.showTree;statusVisible=preferences.showStatus;HDC dc=GetDC(hwnd);dpi=GetDeviceCaps(dc,LOGPIXELSX);ReleaseDC(hwnd,dc);
      font=CreateFontW(-px(14),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
      footerBrush=CreateSolidBrush(RGB(174,125,13));SetMenu(hwnd,menus());
      logo=control(L"STATIC",L"",SS_ICON);SendMessageW(logo,STM_SETICON,reinterpret_cast<WPARAM>(LoadImageW(GetModuleHandleW(nullptr),MAKEINTRESOURCEW(1),IMAGE_ICON,px(60),px(60),LR_SHARED)),0);
      homeOpen=control(L"BUTTON",L"압축 파일 열기",BS_OWNERDRAW|WS_TABSTOP,Open);
      homeCreate=control(L"BUTTON",L"새로 압축하기",BS_OWNERDRAW|WS_TABSTOP,CreateFiles);
      // Workspace IDs differ for duplicate home buttons; commands route to the same actions.
      const wchar_t* labels[]={L"열기",L"새 압축",L"폴더 압축",L"압축 풀기",L"검사",L"취소",L"다시 읽기",L"기본 화면"};
      const int ids[]={Open,CreateFiles,CreateFolder,Extract,Test,Cancel,Refresh,Home};
      for(int i=0;i<8;++i){auto b=control(L"BUTTON",labels[i],WS_TABSTOP,ids[i]+1000);workspace.push_back(b);controls.push_back(b);}
      pathText=control(L"STATIC",L"",SS_PATHELLIPSIS);
      passwordLabel=control(L"STATIC",L"비밀번호",0);
      password=control(L"EDIT",L"",WS_BORDER|ES_PASSWORD|ES_AUTOHSCROLL|WS_TABSTOP);
      SendMessageW(password,EM_SETCUEBANNER,TRUE,reinterpret_cast<LPARAM>(L"입력 후 다시 읽기"));
      tree=control(WC_TREEVIEWW,L"",TVS_HASBUTTONS|TVS_HASLINES|TVS_LINESATROOT|TVS_SHOWSELALWAYS|WS_BORDER|WS_TABSTOP);
      list=control(WC_LISTVIEWW,L"",LVS_REPORT|LVS_SHOWSELALWAYS|WS_BORDER|WS_TABSTOP);
      ListView_SetExtendedListViewStyle(list,LVS_EX_DOUBLEBUFFER|(preferences.fullRow?LVS_EX_FULLROWSELECT:0)|(preferences.grid?LVS_EX_GRIDLINES:0));
      const wchar_t* columns[]={L"이름",L"원본 크기 (B)",L"압축 크기 (B)",L"수정 날짜",L"암호"};int widths[]={280,120,120,170,60};
      for(int i=0;i<5;++i){LVCOLUMNW c{};c.mask=LVCF_TEXT|LVCF_WIDTH;c.pszText=const_cast<wchar_t*>(columns[i]);c.cx=px(widths[i]);ListView_InsertColumn(list,i,&c);}
      workspace.insert(workspace.end(),{pathText,passwordLabel,password,tree,list});controls.insert(controls.end(),{homeOpen,homeCreate,password});
      statusText=control(L"STATIC",L"어린이zip · 설정 메뉴에서 압축 엔진을 연결하세요.",SS_LEFT|SS_PATHELLIPSIS);
      wchar_t executablePath[32768];GetModuleFileNameW(nullptr,executablePath,32768);auto appDir=kz::fs::path(executablePath).parent_path();
      for(const auto& candidate:{appDir/L"7zip/7z.exe",appDir.parent_path()/L"vendor/7zip/7z.exe",kz::fs::path(L"C:/Program Files/7-Zip/7z.exe")})
        if(kz::fs::exists(candidate)){engine=std::make_unique<kz::Engine>(candidate);SetWindowTextW(statusText,L"어린이zip · 압축파일을 열거나 새로 압축해 보세요.");break;}
      showHome(preferences.startHome);ShowWindow(statusText,statusVisible?SW_SHOW:SW_HIDE);return 0;
    }
    case WM_SIZE:layout();InvalidateRect(hwnd,nullptr,TRUE);return 0;
    case WM_ERASEBKGND:return 1;
    case WM_PAINT:{
      PAINTSTRUCT ps;HDC dc=BeginPaint(hwnd,&ps);RECT r;GetClientRect(hwnd,&r);
      if(home){
        auto color=preferences.theme;
        auto component=[](BYTE value){return static_cast<COLOR16>(value<<8);};
        auto pale=[](BYTE value){return static_cast<COLOR16>((value+(255-value)/2)<<8);};
        TRIVERTEX vertices[2]={{0,0,pale(GetRValue(color)),pale(GetGValue(color)),pale(GetBValue(color)),0},{r.right,r.bottom,component(GetRValue(color)),component(GetGValue(color)),component(GetBValue(color)),0}};
        GRADIENT_RECT gradient{0,1};GradientFill(dc,vertices,2,&gradient,1,GRADIENT_FILL_RECT_V);
      }else FillRect(dc,&r,reinterpret_cast<HBRUSH>(COLOR_WINDOW+1));
      r.top=r.bottom-px(36);FillRect(dc,&r,footerBrush);EndPaint(hwnd,&ps);return 0;
    }
    case WM_CTLCOLORSTATIC:if(reinterpret_cast<HWND>(lp)==statusText){SetTextColor(reinterpret_cast<HDC>(wp),RGB(255,255,244));SetBkColor(reinterpret_cast<HDC>(wp),RGB(174,125,13));return reinterpret_cast<LRESULT>(footerBrush);}break;
    case WM_DRAWITEM:paintHomeButton(reinterpret_cast<DRAWITEMSTRUCT*>(lp));return TRUE;
    case Startup:{
      int argc=0;auto argv=CommandLineToArgvW(GetCommandLineW(),&argc);
      if(argv&&argc>=3){
        std::wstring mode=argv[1];kz::fs::path input=argv[2];
        try{
          std::vector<kz::fs::path> inputs;for(int i=2;i<argc;++i)inputs.emplace_back(argv[i]);
          if(mode==L"--compress")createArchive(inputs);
          else if(mode==L"--compress-quick"){
            start([inputs] {return Result{{},false,"압축 완료: "+engine->createQuick(inputs).u8string()};});
          }
          else if(mode==L"--open"||mode==L"--extract-folder"){
            archive=input;SetWindowTextW(pathText,archive.c_str());showHome(false);
            if(mode==L"--open")reload();else start([=]{return Result{{},false,"해제 완료: "+[&]{
              auto name=input.stem();if(input.extension()==L".001")name=name.stem();
              auto desired=input.parent_path()/name;auto destination=desired;
              for(unsigned n=1;!kz::fs::create_directory(destination);++n)destination=kz::fs::path(desired.wstring()+L" ("+std::to_wstring(n)+L")");
              try{return engine->extract(input,destination).u8string();}catch(...){std::error_code ec;kz::fs::remove(destination,ec);throw;}
            }()};});
          }
        }catch(const std::exception& e){SetWindowTextW(statusText,kz::widen(e.what()).c_str());}
      }
      if(argv)LocalFree(argv);return 0;
    }
    case WM_NOTIFY:{
      auto header=reinterpret_cast<NMHDR*>(lp);
      if(header->hwndFrom==tree&&header->code==TVN_SELCHANGEDW&&!buildingTree){
        auto change=reinterpret_cast<NMTREEVIEWW*>(lp);auto index=static_cast<size_t>(change->itemNew.lParam);
        if(index<treePaths.size())displayFolder(treePaths[index]);return 0;
      }
      if(header->hwndFrom==list&&header->code==NM_DBLCLK){
        int row=ListView_GetNextItem(list,-1,LVNI_SELECTED);
        if(row>=0&&static_cast<size_t>(row)<visibleEntries.size()&&visibleEntries[row].directory){
          auto it=treeItems.find(visibleEntries[row].name);if(it!=treeItems.end()){TreeView_SelectItem(tree,it->second);TreeView_Expand(tree,it->second,TVE_EXPAND);}
        }return 0;
      }
      break;
    }
    case WM_INITMENUPOPUP:updateMenus();return 0;
    case WM_COMMAND:{
      int id=LOWORD(wp);if(id>=Open+1000&&id<=Home+1000)id-=1000;
      if((HIWORD(wp)==0||HIWORD(wp)==1)&&id>=Open&&id<=ToggleTree)command(id);return 0;
    }
    case Finished:{
      std::unique_ptr<Result> result(reinterpret_cast<Result*>(lp));if(worker.joinable())worker.join();busy=false;
      for(auto c:controls)EnableWindow(c,TRUE);if(result->listing){showHome(false);display(result->entries);}
      SetWindowTextW(statusText,kz::widen(result->message).c_str());
      if(progressWindow){
        KillTimer(progressWindow,1);SetWindowTextW(progressWindow,result->success?L"작업 완료 - 어린이zip":L"작업 중단 - 어린이zip");
        SetDlgItemTextW(progressWindow,IDC_PROGRESS_STATUS,kz::widen(result->message).c_str());
        SendDlgItemMessageW(progressWindow,IDC_PROGRESS_BAR,PBM_SETMARQUEE,FALSE,0);
        auto bar=GetDlgItem(progressWindow,IDC_PROGRESS_BAR);
        SetWindowLongPtrW(bar,GWL_STYLE,GetWindowLongPtrW(bar,GWL_STYLE)&~PBS_MARQUEE);
        if(result->success)SendMessageW(bar,PBM_SETPOS,100,0);
        else SendMessageW(bar,PBM_SETSTATE,PBST_ERROR,0);
        auto elapsed=std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now()-jobStarted).count();
        SetWindowTextW(progressWindow,((result->success?L"완료":L"중단")+std::wstring(L" · 소요 ")+durationText(elapsed)+L" - 어린이zip").c_str());
        SetDlgItemTextW(progressWindow,IDCANCEL,L"닫기");EnableWindow(GetDlgItem(progressWindow,IDCANCEL),TRUE);
      }return 0;
    }
    case WM_GETMINMAXINFO:{auto info=reinterpret_cast<MINMAXINFO*>(lp);info->ptMinTrackSize={px(730),px(460)};return 0;}
    case WM_CLOSE:if(busy){MessageBoxW(hwnd,L"작업을 취소한 뒤 창을 닫아 주세요.",L"어린이zip",MB_OK);return 0;}DestroyWindow(hwnd);return 0;
    case WM_DESTROY:if(worker.joinable())worker.join();DeleteObject(font);DeleteObject(footerBrush);PostQuitMessage(0);return 0;
  }
  return DefWindowProcW(hwnd,msg,wp,lp);
}
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR,int show) {
  CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);INITCOMMONCONTROLSEX cc{sizeof(cc),ICC_LISTVIEW_CLASSES|ICC_PROGRESS_CLASS|ICC_TREEVIEW_CLASSES};InitCommonControlsEx(&cc);
  WNDCLASSW cls{};cls.lpfnWndProc=procedure;cls.hInstance=instance;cls.lpszClassName=L"EoriniZipNative";
  cls.hCursor=LoadCursorW(nullptr,IDC_ARROW);cls.hIcon=LoadIconW(instance,MAKEINTRESOURCEW(1));RegisterClassW(&cls);
  HWND main=CreateWindowW(cls.lpszClassName,L"어린이zip",WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN,CW_USEDEFAULT,CW_USEDEFAULT,840,620,nullptr,nullptr,instance,nullptr);
  ShowWindow(main,show);UpdateWindow(main);PostMessageW(main,Startup,0,0);
  ACCEL keys[]={{FVIRTKEY|FCONTROL,'O',Open},{FVIRTKEY|FCONTROL,'N',CreateFiles},{FVIRTKEY|FCONTROL,'A',SelectAll},{FVIRTKEY|FCONTROL,'F',FindFile},{FVIRTKEY|FCONTROL,'T',Test},{FVIRTKEY|FCONTROL,'E',Extract},{FVIRTKEY|FCONTROL,'W',Exit},{FVIRTKEY|FCONTROL|FSHIFT,'N',NewWindow},{FVIRTKEY,VK_F4,CloseArchive},{FVIRTKEY,VK_F6,Home},{FVIRTKEY,VK_F1,About},{FVIRTKEY,VK_F5,Preferences},{FVIRTKEY,VK_F7,ToggleTree}};
  HACCEL accelerators=CreateAcceleratorTableW(keys,static_cast<int>(std::size(keys)));
  MSG msg;while(GetMessageW(&msg,nullptr,0,0)>0){
    // Preserve ordinary editing shortcuts in the password field.
    if(progressWindow&&IsDialogMessageW(progressWindow,&msg))continue;
    bool editing=GetFocus()==password;
    if(msg.message==WM_KEYDOWN&&msg.wParam==VK_ESCAPE&&preferences.escape&&!editing){SendMessageW(main,WM_CLOSE,0,0);continue;}
    if((editing||!TranslateAcceleratorW(main,accelerators,&msg))&&!IsDialogMessageW(main,&msg)){TranslateMessage(&msg);DispatchMessageW(&msg);}
  }
  DestroyAcceleratorTable(accelerators);CoUninitialize();return 0;
}
