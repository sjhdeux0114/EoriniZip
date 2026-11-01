#include "core.hpp"
#include "split_presets.hpp"
#include "archive_name.hpp"
#include "preferences.hpp"

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
#include "preview_window.hpp"
#include "windows_clipboard.hpp"
#include "windows_passwords.hpp"
#include <algorithm>
#include <chrono>
#include <map>
#include <fstream>
#include "ui_language.hpp"
namespace {
BOOL uiWindowText(HWND target,LPCWSTR value){auto translated=kz::ui::editable(target)?std::wstring(value?value:L""):kz::ui::label(value?value:L"");return SetWindowTextW(target,translated.c_str());}
BOOL uiDialogText(HWND target,int id,LPCWSTR value){auto control=GetDlgItem(target,id);return uiWindowText(control,value);}
int uiMessageBox(HWND owner,LPCWSTR value,LPCWSTR caption,UINT flags){auto message=kz::ui::label(value?value:L""),title=kz::ui::label(caption?caption:L"");return MessageBoxW(owner,message.c_str(),title.c_str(),flags);}
LRESULT uiDialogMessage(HWND target,int id,UINT message,WPARAM wp,LPARAM lp){
  if(lp&&(message==CB_ADDSTRING||(message==LB_ADDSTRING&&id==IDC_CATEGORIES))){auto translated=kz::ui::label(reinterpret_cast<LPCWSTR>(lp));return SendDlgItemMessageW(target,id,message,wp,reinterpret_cast<LPARAM>(translated.c_str()));}
  return SendDlgItemMessageW(target,id,message,wp,lp);
}
}
#define SetWindowTextW uiWindowText
#define SetDlgItemTextW uiDialogText
#define MessageBoxW uiMessageBox
#define SendDlgItemMessageW uiDialogMessage
namespace {
HWND progressWindow=nullptr, tree=nullptr;
HWND window, list, statusText, pathText, password, passwordLabel, logo, homeOpen, homeCreate;
HFONT font;
HFONT titleFont=nullptr, subFont=nullptr;
HIMAGELIST entryImages=nullptr;
int folderIcon=-1,fileIcon=-1;
HBRUSH footerBrush=nullptr, bgBrush=nullptr;
std::vector<HWND> workspace, controls;
std::unique_ptr<kz::Engine> engine;
std::thread worker;
std::function<void(std::string)> passwordRetry;
bool askArchivePassword(std::wstring& secret);
kz::fs::path archive;
int jobKind=0;
bool busy=false, home=true, onTop=false, statusVisible=true, treeVisible=true, buildingTree=false;
std::vector<kz::Entry> archiveEntries,visibleEntries;
std::vector<std::string> treePaths;
std::map<std::string,HTREEITEM> treeItems;
std::string currentFolder;
std::vector<kz::fs::path> clipboardRoots;
int sortColumn=0;bool sortAscending=true;HMENU recentMenu=nullptr;std::vector<kz::fs::path> recentFiles;
constexpr int RecentFirst=400,RecentClear=410;
int dpi=96;
using PreferencesData=kz::Preferences;
PreferencesData preferences;
kz::fs::path temporaryFolder,pendingTemporaryFolder;
const int codePages[]={0,949,932,936,950,1252,65001};
const char* creationFormats[]={"zip","7z","tar","zpaq","gzip","bzip2","xz","7z","wim","iso"};
const char* creationExtensions[]={"zip","7z","tar","zpaq","gz","bz2","xz","exe","wim","iso"};
const char* formatForIndex(int index){return creationFormats[std::clamp(index,0,9)];}
const char* extensionForIndex(int index){return creationExtensions[std::clamp(index,0,9)];}
COLORREF pendingTheme=RGB(37,99,235);
constexpr const wchar_t* preferencesKey=L"Software\\EoriniZip\\Preferences";
void loadPreferences() {
  wchar_t temp[32768]{};DWORD tempSize=sizeof(temp);
  if(RegGetValueW(HKEY_CURRENT_USER,preferencesKey,L"TemporaryFolder",RRF_RT_REG_SZ,nullptr,temp,&tempSize)==ERROR_SUCCESS&&kz::fs::is_directory(temp))temporaryFolder=temp;
  for(const auto& field:kz::preferenceFields()){
    std::wstring name(field.name,field.name+strlen(field.name));DWORD value=0,size=sizeof(value);
    if(RegGetValueW(HKEY_CURRENT_USER,preferencesKey,name.c_str(),RRF_RT_REG_DWORD,nullptr,&value,&size)==ERROR_SUCCESS&&value<=static_cast<DWORD>(field.maximum))
      preferences.*(field.member)=static_cast<int>(value);
  }
  kz::ui::english=preferences.language==1;
}
bool savePreferences() {
  HKEY key;if(RegCreateKeyExW(HKEY_CURRENT_USER,preferencesKey,0,nullptr,0,KEY_SET_VALUE,nullptr,&key,nullptr)!=ERROR_SUCCESS)return false;
  bool ok=true;
  for(const auto& field:kz::preferenceFields()){
    std::wstring name(field.name,field.name+strlen(field.name));DWORD value=preferences.*(field.member);
    if(RegSetValueExW(key,name.c_str(),0,REG_DWORD,reinterpret_cast<const BYTE*>(&value),sizeof(value))!=ERROR_SUCCESS)ok=false;
  }
  RegCloseKey(key);return ok;
}
enum { Open=100, CreateFiles, CreateFolder, Extract, Test, Connect, Cancel, Refresh, Home, About, Exit, SelectAll, CopyNames, CloseArchive, SaveAs, OpenFolder, NewWindow, Topmost, ToggleStatus, AppFolder, HelpPage, IssuesPage, Website, FindFile, Preferences, ToggleTree, ColorSettings, ExtractSelected, ExtractSmart, DecryptEasy, AddEntries, DeleteEntries, RenameEntry, DeleteArchive, DeleteArchivePermanent, NewAdminWindow, PreviewEntry, CopyFiles, PasteFiles, ExternalEdit, EditComment, ScanArchive, SavePassword, LoadPassword, DeletePassword, RecoverPassword, RepairZip, CheckUpdates, ToggleToolbar, ViewSettings, LastCommand=ViewSettings };
constexpr UINT Finished=WM_APP+1, Startup=WM_APP+2;
struct Result { std::vector<kz::Entry> entries; bool listing=false; std::string message; bool success=true; bool passwordNeeded=false; kz::fs::path previewFile,previewRoot,clipboardRoot,editFile,editRoot,editArchive; std::string editName,editPassword; uintmax_t editSize=0; kz::fs::file_time_type editStamp{}; std::string recoveredPassword; };
struct CreateRequest {
  std::vector<kz::fs::path> inputs;
  kz::fs::path output;
  kz::Options options;
  bool testAfter=false;
  std::wstring suggested;
};
int px(int v) { return MulDiv(v,dpi,96); }
typedef HRESULT (WINAPI *pfnSetWindowTheme)(HWND, LPCWSTR, LPCWSTR);
void applyModernTheme(HWND target) {
  static HMODULE hUxTheme = LoadLibraryW(L"uxtheme.dll");
  if(hUxTheme){
    static auto setWindowTheme = reinterpret_cast<pfnSetWindowTheme>(GetProcAddress(hUxTheme, "SetWindowTheme"));
    if(setWindowTheme) setWindowTheme(target, L"Explorer", nullptr);
  }
}
void applyViewPreferences(){
  const wchar_t* families[]={L"Segoe UI",L"Malgun Gothic",L"Gulim"};const int sizes[]={12,14,16,18,20};
  HFONT next=CreateFontW(-px(sizes[preferences.fontSize]),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,families[preferences.fontFamily]);
  if(next){EnumChildWindows(window,[](HWND child,LPARAM value)->BOOL{SendMessageW(child,WM_SETFONT,static_cast<WPARAM>(value),TRUE);return TRUE;},reinterpret_cast<LPARAM>(next));auto previous=font;font=next;if(previous)DeleteObject(previous);}
  HFONT nextTitle=CreateFontW(-px(sizes[preferences.fontSize]+2),0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,families[preferences.fontFamily]);
  if(nextTitle){auto prev=titleFont;titleFont=nextTitle;if(prev)DeleteObject(prev);}
  HFONT nextSub=CreateFontW(-px(std::max(10,sizes[preferences.fontSize]-2)),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,families[preferences.fontFamily]);
  if(nextSub){auto prev=subFont;subFont=nextSub;if(prev)DeleteObject(prev);}
  const int widths[]={120,120,170,60};for(int column=1;column<5;++column)ListView_SetColumnWidth(list,column,(preferences.columnMask&(1<<(column-1)))?px(widths[column-1]):0);
}
std::wstring text(HWND control) {
  int n=GetWindowTextLengthW(control); std::wstring s(n+1,L'\0');
  GetWindowTextW(control,s.data(),n+1); s.resize(n); return s;
}
HWND control(const wchar_t* cls,const wchar_t* title,DWORD style,int id=0) {
  auto translated=kz::ui::label(title);
  HWND item=CreateWindowExW(0,cls,translated.c_str(),WS_CHILD|WS_VISIBLE|style,0,0,1,1,window,reinterpret_cast<HMENU>(static_cast<intptr_t>(id)),nullptr,nullptr);
  SendMessageW(item,WM_SETFONT,reinterpret_cast<WPARAM>(font),TRUE); return item;
}
std::vector<kz::fs::path> choose(bool folder,bool multiple=false,bool save=false,const std::wstring& name=L"",HWND owner=nullptr) {
  IFileDialog* dialog=nullptr;
  if(FAILED(CoCreateInstance(save?CLSID_FileSaveDialog:CLSID_FileOpenDialog,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&dialog)))) throw std::runtime_error("?åÏùº ?†ÌÉùÏ∞ΩÏùÑ ?????ÜÏäµ?àÎã§.");
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
  MoveWindow(homeOpen,w/2-px(175),h/2-px(85),px(170),px(145),TRUE);
  MoveWindow(homeCreate,w/2+px(5),h/2-px(85),px(170),px(145),TRUE);
  MoveWindow(logo,w/2-px(30),h/2-px(165),px(60),px(60),TRUE);
  MoveWindow(statusText,px(15),h-px(27),std::max(px(100),w-px(30)),px(22),TRUE);
  const int ids[]={Open,CreateFiles,CreateFolder,Extract,Test,Cancel,Refresh,Home};
  for(int i=0;i<8;++i){auto button=GetDlgItem(window,ids[i]+1000);MoveWindow(button,px(10+i*86),px(10),px(80),px(30),TRUE);ShowWindow(button,!home&&preferences.showToolbar?SW_SHOW:SW_HIDE);}
  const int offset=preferences.showToolbar?0:-42;
  MoveWindow(pathText,px(12),px(56+offset),w-px(24),px(24),TRUE);
  MoveWindow(passwordLabel,px(12),px(91+offset),px(70),px(24),TRUE);
  MoveWindow(password,px(85),px(86+offset),px(220),px(28),TRUE);
  int treeWidth=treeVisible?px(220):0;
  MoveWindow(tree,px(12),px(124+offset),px(212),std::max(px(60),h-px(170+offset)),TRUE);
  MoveWindow(list,px(12)+treeWidth,px(124+offset),std::max(px(100),w-px(24)-treeWidth),std::max(px(60),h-px(170+offset)),TRUE);
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
  bool tar=fmt==2, seven=fmt==1||fmt==7,stream=fmt>=4&&fmt<=6;
  EnableWindow(GetDlgItem(dialog,IDC_SPLIT),fmt==0||fmt==1);
  int selected=static_cast<int>(SendDlgItemMessageW(dialog,IDC_SPLIT,CB_GETCURSEL,0,0));
  EnableWindow(GetDlgItem(dialog,IDC_CUSTOM_SPLIT),(fmt==0||fmt==1)&&selected==static_cast<int>(kz::customSplitIndex));
  EnableWindow(GetDlgItem(dialog,IDC_PASSWORD),!tar&&!stream&&fmt<8);
  EnableWindow(GetDlgItem(dialog,IDC_LEVEL),!tar&&fmt<8);
  EnableWindow(GetDlgItem(dialog,IDC_HIDE_NAMES),seven);
  if(!seven)CheckDlgButton(dialog,IDC_HIDE_NAMES,BST_UNCHECKED);
}
void suggestOutput(HWND dialog,CreateRequest& request) {
  auto current=text(GetDlgItem(dialog,IDC_OUTPUT));
  if(!current.empty()&&current!=request.suggested)return;
  int fmt=static_cast<int>(SendDlgItemMessageW(dialog,IDC_FORMAT,CB_GETCURSEL,0,0));
  request.suggested=request.inputs.empty()?L"":kz::suggestedArchive(request.inputs,extensionForIndex(fmt)).wstring();
  SetDlgItemTextW(dialog,IDC_OUTPUT,request.suggested.c_str());
}
INT_PTR CALLBACK createProcedure(HWND dialog,UINT msg,WPARAM wp,LPARAM lp) {
  auto request=reinterpret_cast<CreateRequest*>(GetWindowLongPtrW(dialog,DWLP_USER));
  if(msg==WM_INITDIALOG) {kz::ui::dialog(dialog);
    request=reinterpret_cast<CreateRequest*>(lp);SetWindowLongPtrW(dialog,DWLP_USER,lp);
    for(auto s:{L"ZIP",L"7Z",L"TAR",L"ZPAQ",L"GZIP (?åÏùº ?òÎÇò)",L"BZIP2 (?åÏùº ?òÎÇò)",L"XZ (?åÏùº ?òÎÇò)",L"SFX (7Z ?êÏ≤¥ ?¥Ï†ú EXE)",L"WIM (ÎπÑÏïïÏ∂?",L"ISO (?¥Îçî ?¥Ïö©)"})SendDlgItemMessageW(dialog,IDC_FORMAT,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(s));
    SendDlgItemMessageW(dialog,IDC_FORMAT,CB_SETCURSEL,0,0);
    for(const auto& preset:kz::splitPresets) {
      auto label=kz::widen(preset.label);SendDlgItemMessageW(dialog,IDC_SPLIT,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(label.c_str()));
    }
    SendDlgItemMessageW(dialog,IDC_SPLIT,CB_SETCURSEL,0,0);
    SendDlgItemMessageW(dialog,IDC_SPLIT,CB_SETMINVISIBLE,21,0);
    for(auto s:{L"?ïÏ∂ï?òÏ? ?äÏùå",L"Îπ†Î•¥Í≤?,L"Î≥¥ÌÜµ",L"ÏµúÎ?Î°?})SendDlgItemMessageW(dialog,IDC_LEVEL,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(s));
    SendDlgItemMessageW(dialog,IDC_LEVEL,CB_SETCURSEL,preferences.level,0);CheckDlgButton(dialog,IDC_TEST_AFTER,preferences.test?BST_CHECKED:BST_UNCHECKED);
    SendDlgItemMessageW(dialog,IDC_CUSTOM_SPLIT,EM_SETCUEBANNER,TRUE,reinterpret_cast<LPARAM>(L"?? 100m, 2g"));
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
        if(!output.empty()){bool automatic=output.wstring()==request->suggested;output.replace_extension(kz::fs::u8path(std::string(".")+extensionForIndex(index)));SetDlgItemTextW(dialog,IDC_OUTPUT,output.c_str());if(automatic)request->suggested=output.wstring();}
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
      auto files=choose(false,false,true,output.empty()?kz::suggestedArchive(request->inputs,extensionForIndex(fmt)).filename().wstring():output,dialog);
      if(!files.empty())SetDlgItemTextW(dialog,IDC_OUTPUT,files[0].c_str());return TRUE;
    }
    if(id==IDOK) {
      int fmt=static_cast<int>(SendDlgItemMessageW(dialog,IDC_FORMAT,CB_GETCURSEL,0,0));
      kz::Options options;options.unicodeNames=preferences.unicodeNames;options.storeTimes=preferences.storeTimes;options.excludeThumbs=preferences.excludeThumbs;options.multiThread=preferences.parallel;options.format=formatForIndex(fmt);
      int levels[]={0,1,5,9};int level=static_cast<int>(SendDlgItemMessageW(dialog,IDC_LEVEL,CB_GETCURSEL,0,0));
      options.level=levels[std::clamp(level,0,3)];
      options.selfExtracting=fmt==7;
      if(fmt!=2&&!(fmt>=4&&fmt<=6)&&fmt<8) {
        options.password=kz::narrow(text(GetDlgItem(dialog,IDC_PASSWORD)));
        if(fmt==0||fmt==1)options.split=kz::splitArgument(static_cast<size_t>(SendDlgItemMessageW(dialog,IDC_SPLIT,CB_GETCURSEL,0,0)),kz::narrow(text(GetDlgItem(dialog,IDC_CUSTOM_SPLIT))));
        options.hideNames=(fmt==1||fmt==7)&&IsDlgButtonChecked(dialog,IDC_HIDE_NAMES)==BST_CHECKED;
      }
      auto path=text(GetDlgItem(dialog,IDC_OUTPUT));
      if(path.empty())throw std::runtime_error("?Ä?•Ìï† ?åÏùº ?¥Î¶Ñ??ÏßÄ?ïÌïò?∏Ïöî.");
      request->output=kz::fs::absolute(kz::fs::path(path));
      if(!kz::fs::is_directory(request->output.parent_path()))throw std::runtime_error("?Ä?•Ìï† ?ÅÏúÑ ?¥ÎçîÍ∞Ä Ï°¥Ïû¨?òÏ? ?äÏäµ?àÎã§.");
      if(request->output.filename().empty())throw std::runtime_error("?åÏùº ?¥Î¶Ñ???ÑÏöî?©Îãà??");
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
void rememberArchive(){
  if(archive.empty())return;
  recentFiles.erase(std::remove(recentFiles.begin(),recentFiles.end(),archive),recentFiles.end());recentFiles.insert(recentFiles.begin(),archive);if(recentFiles.size()>10)recentFiles.resize(10);
  HKEY key;if(RegCreateKeyExW(HKEY_CURRENT_USER,L"Software\\EoriniZip\\Recent",0,nullptr,0,KEY_SET_VALUE,nullptr,&key,nullptr)==ERROR_SUCCESS){
    for(size_t i=0;i<10;++i){auto name=std::to_wstring(i);if(i<recentFiles.size()){auto value=recentFiles[i].wstring();RegSetValueExW(key,name.c_str(),0,REG_SZ,reinterpret_cast<const BYTE*>(value.c_str()),static_cast<DWORD>((value.size()+1)*sizeof(wchar_t)));}else RegDeleteValueW(key,name.c_str());}RegCloseKey(key);
  }
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
  std::stable_sort(visibleEntries.begin(),visibleEntries.end(),[](const auto& a,const auto& b){
    if(a.directory!=b.directory)return a.directory;
    int result=0;
    if(sortColumn==1)result=a.size<b.size?-1:a.size>b.size?1:0;
    else if(sortColumn==2)result=a.packed<b.packed?-1:a.packed>b.packed?1:0;
    else if(sortColumn==3)result=a.modified.compare(b.modified);
    else if(sortColumn==4)result=static_cast<int>(a.encrypted)-static_cast<int>(b.encrypted);
    if(!result)result=a.name.compare(b.name);return sortAscending?result<0:result>0;
  });
  SendMessageW(list,WM_SETREDRAW,FALSE,0);ListView_DeleteAllItems(list);
  for(size_t i=0;i<visibleEntries.size();++i){
    const auto& e=visibleEntries[i];auto name=e.name.substr(prefix.size());
    std::wstring values[]={kz::widen(kz::composeFilename(name)),e.directory?L"":std::to_wstring(e.size),e.directory?L"":std::to_wstring(e.packed),kz::widen(e.modified),e.encrypted?L"?îÌò∏":L""};
    LVITEMW item{};item.mask=LVIF_TEXT;item.iImage=e.directory?folderIcon:fileIcon;if(item.iImage>=0)item.mask|=LVIF_IMAGE;
    item.iItem=static_cast<int>(i);item.pszText=values[0].data();ListView_InsertItem(list,&item);
    for(int j=1;j<5;++j)ListView_SetItemText(list,static_cast<int>(i),j,values[j].data());
  }
  SendMessageW(list,WM_SETREDRAW,TRUE,0);InvalidateRect(list,nullptr,TRUE);
}
void display(const std::vector<kz::Entry>& entries) {
  archiveEntries=entries;buildingTree=true;TreeView_DeleteAllItems(tree);treePaths.clear();treeItems.clear();
  auto insert=[&](const std::string& path,const std::wstring& label,HTREEITEM parent){
    TVINSERTSTRUCTW item{};item.hParent=parent;item.hInsertAfter=TVI_LAST;item.item.mask=TVIF_TEXT|TVIF_PARAM;
    item.item.pszText=const_cast<wchar_t*>(label.c_str());item.item.lParam=static_cast<LPARAM>(treePaths.size());treePaths.push_back(path);
    item.item.iImage=item.item.iSelectedImage=path.empty()?fileIcon:folderIcon;
    if(item.item.iImage>=0)item.item.mask|=TVIF_IMAGE|TVIF_SELECTEDIMAGE;
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
    insert(folder.first,kz::widen(kz::composeFilename(label)),treeItems.at(parent));
  }
  TreeView_Expand(tree,root,TVE_EXPAND);buildingTree=false;TreeView_SelectItem(tree,root);displayFolder("");
}
std::chrono::steady_clock::time_point jobStarted,phaseStarted;
int lastPhase=-1, lastProgress=-2;
bool progressMarquee=false;
std::wstring durationText(long long seconds) {
  return std::to_wstring(seconds/60)+L"Î∂?"+std::to_wstring(seconds%60)+L"Ï¥?;
}
INT_PTR CALLBACK progressProcedure(HWND dialog,UINT msg,WPARAM wp,LPARAM) {
  if(msg==WM_INITDIALOG){kz::ui::dialog(dialog);
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
    const wchar_t* label=phase==1?L"?ïÏ∂ï Ï§?:phase==2?L"?ïÏ∂ï ?¥Ï†ú Ï§?:phase==3?L"Í≤Ä??Ï§?:L"?åÏùº ?ïÏù∏ Ï§?;
    auto title=std::wstring(label)+(percent>=0?L" ¬∑ "+std::to_wstring(percent)+L"%":L"...");

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
    auto remaining=percent>0&&percent<100&&stage>=1?durationText(stage*(100-percent)/percent):percent==100?L"ÎßàÎ¨¥Î¶?Ï§?:L"Í≥ÑÏÇ∞ Ï§?;
    title+=L" ¬∑ Í≤ΩÍ≥º "+durationText(elapsed);
    if(text(dialog)!=title)SetWindowTextW(dialog,title.c_str());
    auto info=std::wstring(label)+L" ¬∑ ?àÏÉÅ ?®Ï? ?úÍ∞Ñ "+remaining;
    if(!IsWindowEnabled(GetDlgItem(dialog,IDCANCEL)))info=L"Ï∑®ÏÜå Ï§?¬∑ ?ÑÏãú ?åÏùº???ïÎ¶¨?òÍ≥† ?àÏäµ?àÎã§...";
    if(text(GetDlgItem(dialog,IDC_PROGRESS_STATUS))!=info)SetDlgItemTextW(dialog,IDC_PROGRESS_STATUS,info.c_str());return TRUE;
  }
  if(msg==WM_CLOSE||(msg==WM_COMMAND&&LOWORD(wp)==IDCANCEL)){
    if(busy){if(engine)engine->cancel();SetDlgItemTextW(dialog,IDC_PROGRESS_STATUS,L"Ï∑®ÏÜå Ï§?..");EnableWindow(GetDlgItem(dialog,IDCANCEL),FALSE);}
    else{KillTimer(dialog,1);DestroyWindow(dialog);progressWindow=nullptr;EnableWindow(window,TRUE);SetActiveWindow(window);}
    return TRUE;
  }
  return FALSE;
}
void start(std::function<Result()> task,int kind=0,std::function<void(std::string)> retry={}) {
  if(busy)return;
  if(!engine)throw std::runtime_error("?§Ï†ï ???ïÏ∂ï ?îÏßÑ ?∞Í≤∞?êÏÑú Í≥µÏãù 7z.exeÎ•??†ÌÉù?òÏÑ∏??");
  if(worker.joinable())worker.join();
  engine->configureRead(codePages[preferences.codePageIndex],preferences.parallel,preferences.zone);
  engine->configureTemporaryDirectory(temporaryFolder);
  passwordRetry=std::move(retry);jobKind=kind;busy=true;engine->resetCancel();for(auto c:controls)EnableWindow(c,FALSE);
  EnableWindow(GetDlgItem(window,Cancel+1000),TRUE);SetWindowTextW(statusText,L"?ëÏóÖ Ï§ë‚Ä? ?ÑÍµ¨ Î©îÎâ¥?êÏÑú Ï∑®ÏÜå?????àÏäµ?àÎã§.");
  jobStarted=phaseStarted=std::chrono::steady_clock::now();lastPhase=-1;
  progressWindow=CreateDialogParamW(GetModuleHandleW(nullptr),MAKEINTRESOURCEW(IDD_PROGRESS),window,progressProcedure,0);
  if(progressWindow){EnableWindow(window,FALSE);ShowWindow(progressWindow,SW_SHOW);}
  worker=std::thread([task=std::move(task)]{
    auto result=std::make_unique<Result>();
    try{*result=task();}catch(const kz::PasswordRequired& e){result->message=e.what();result->success=false;result->passwordNeeded=true;}catch(const std::exception& e){result->message=e.what();result->success=false;}
    PostMessageW(window,Finished,0,reinterpret_cast<LPARAM>(result.release()));
  });
}
// One retry path serves open, app extraction and Explorer extraction.
void archiveTask(const kz::fs::path& input,int mode,const kz::fs::path& destination={},const std::vector<std::string>& selected={},const std::string& secret="") {
  start([=]{
    auto rows=engine->list(input,secret);
    if(secret.empty()&&std::any_of(rows.begin(),rows.end(),[](const auto& e){return e.encrypted;}))throw kz::PasswordRequired("?îÌò∏?îÎêú ?ïÏ∂ï ?åÏùº?ÖÎãà??");
    if(mode==0){
      if(std::any_of(rows.begin(),rows.end(),[](const auto& e){return e.encrypted;}))engine->test(input,secret);
      return Result{std::move(rows),true,"Î™©Î°ù ?ΩÍ∏∞ ?ÑÎ£å"};
    }
    if(mode==4){engine->test(input,secret);return Result{{},false,"?ïÏ∂ï ?åÏùº Í≤Ä???ÑÎ£å"};}
    const auto target=destination.empty()?kz::fs::absolute(input).parent_path():destination;
    auto output=(mode==2||mode==3)?engine->extractSmart(input,target,secret,mode==3):engine->extract(input,target,secret,selected);
    return Result{{},false,"?¥Ï†ú ?ÑÎ£å: "+output.u8string()};
  },mode==0?0:mode==4?3:2,[=](std::string next){
    SetWindowTextW(password,kz::widen(next).c_str());
    archiveTask(input,mode,destination,selected,next);
  });
}
void batchExtractTask(const std::vector<kz::fs::path>& inputs,std::map<std::wstring,std::string> secrets={}) {
  auto needed=std::make_shared<kz::fs::path>();
  start([inputs,secrets,needed]{
    // Validate passwords for every archive before publishing any batch output.
    for(const auto& input:inputs){
      *needed=input;auto it=secrets.find(input.wstring());auto secret=it==secrets.end()?std::string{}:it->second;
      auto entries=engine->list(input,secret);
      if(std::any_of(entries.begin(),entries.end(),[](const auto& e){return e.encrypted;})){
        if(secret.empty())throw kz::PasswordRequired("?îÌò∏?îÎêú ?ïÏ∂ï ?åÏùº: "+input.filename().u8string());
        engine->test(input,secret);
      }
    }
    size_t completed=0;
    for(const auto& input:inputs){auto it=secrets.find(input.wstring());auto secret=it==secrets.end()?std::string{}:it->second;
      try{engine->extractSmart(input,kz::fs::absolute(input).parent_path(),secret,true);++completed;}
      catch(const std::exception& e){throw std::runtime_error(std::to_string(completed)+"Í∞??ÑÎ£å ??Ï§ëÎã®: "+input.filename().u8string()+"\n"+e.what());}}
    return Result{{},false,std::to_string(completed)+"Í∞?Í∞úÎ≥Ñ ?¥Ï†ú ?ÑÎ£å"};
  },2,[inputs,secrets,needed](std::string value)mutable{secrets[needed->wstring()]=std::move(value);batchExtractTask(inputs,std::move(secrets));});
}void reload() {
  if(archive.empty())return;showHome(false);
  archiveTask(archive,0,{}, {},kz::narrow(text(password)));
}void createArchive(std::vector<kz::fs::path> inputs={}) {
  CreateRequest request;request.inputs=std::move(inputs);
  if(DialogBoxParamW(GetModuleHandleW(nullptr),MAKEINTRESOURCEW(IDD_CREATE),window,createProcedure,reinterpret_cast<LPARAM>(&request))!=IDOK)return;
  start([request=std::move(request)] {
    auto destination=engine->create(request.options,request.output,request.inputs);
    if(request.testAfter) {
      auto output=destination;

      engine->test(output,request.options.password);
    }
    return Result{{},false,"?ïÏ∂ï ?ÑÎ£å"+std::string(request.testAfter?" ¬∑ Í≤Ä???µÍ≥º: ":": ")+destination.u8string()};
  },1);
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
  if(msg==WM_INITDIALOG){kz::ui::dialog(dialog);return TRUE;}
  if(msg==WM_CLOSE){EndDialog(dialog,IDCANCEL);return TRUE;}
  if(msg!=WM_COMMAND)return FALSE;
  if(LOWORD(wp)==IDCANCEL){EndDialog(dialog,IDCANCEL);return TRUE;}
  if(LOWORD(wp)==IDOK) {
    const auto query=text(GetDlgItem(dialog,IDC_FIND_TEXT));if(query.empty())return TRUE;
    const size_t count=archiveEntries.size();
    auto cursor=static_cast<size_t>(GetWindowLongPtrW(dialog,DWLP_USER));
    for(size_t n=0;n<count;++n){
      const size_t index=(cursor+n)%count;
      auto name=kz::widen(archiveEntries[index].name);
      if(FindNLSStringEx(LOCALE_NAME_INVARIANT,FIND_FROMSTART|NORM_IGNORECASE,name.c_str(),static_cast<int>(name.size()),query.c_str(),static_cast<int>(query.size()),nullptr,nullptr,nullptr,0)<0)continue;
      const auto path=normalized(archiveEntries[index].name);
      const auto slash=path.rfind('/');
      const auto parent=slash==std::string::npos?std::string{}:path.substr(0,slash);
      showHome(false);
      auto found=treeItems.find(parent);
      if(found!=treeItems.end()){TreeView_SelectItem(tree,found->second);TreeView_EnsureVisible(tree,found->second);}
      displayFolder(parent);
      ListView_SetItemState(list,-1,0,LVIS_SELECTED|LVIS_FOCUSED);
      for(size_t row=0;row<visibleEntries.size();++row)if(normalized(visibleEntries[row].name)==path){
        ListView_SetItemState(list,static_cast<int>(row),LVIS_SELECTED|LVIS_FOCUSED,LVIS_SELECTED|LVIS_FOCUSED);
        ListView_EnsureVisible(list,static_cast<int>(row),FALSE);break;
      }
      SetWindowLongPtrW(dialog,DWLP_USER,static_cast<LONG_PTR>((index+1)%count));
      SetDlgItemTextW(dialog,IDC_FIND_RESULT,name.c_str());return TRUE;
    }
    SetDlgItemTextW(dialog,IDC_FIND_RESULT,L"?ºÏπò?òÎäî ?åÏùº???ÜÏäµ?àÎã§.");return TRUE;
  }
  return FALSE;
}
void settingsPage(HWND dialog) {
  int page=static_cast<int>(SendDlgItemMessageW(dialog,IDC_CATEGORIES,LB_GETCURSEL,0,0));
  const wchar_t* titles[]={L"?ºÎ∞ò ?§Ï†ï",L"?åÏùº ?∞Í≤∞",L"?êÏÉâÍ∏?Î©îÎâ¥",L"?ïÏ∂ï ?ÄÍ∏?,L"?ïÏ∂ï?òÍ∏∞",L"Î≥¥Í∏∞/?∏Ïßë",L"Î≥¥Ïïà ?§Ï†ï",L"Í≥†Í∏â ?§Ï†ï",L"?∏Ïñ¥ ?§Ï†ï"};
  SetDlgItemTextW(dialog,IDC_SETTINGS_TITLE,titles[std::clamp(page,0,8)]);
  for(int id:{IDC_PREF_ESCAPE,IDC_PREF_GRID,IDC_PREF_FULLROW,IDC_PREF_HOME})ShowWindow(GetDlgItem(dialog,id),page==0?SW_SHOW:SW_HIDE);
  for(int id:{IDC_PREF_TEST,IDC_PREF_LEVEL,IDC_PREF_UNICODE,IDC_PREF_TIMES,IDC_PREF_THUMBS,IDC_PREF_CLOSE_CREATE})ShowWindow(GetDlgItem(dialog,id),page==4?SW_SHOW:SW_HIDE);
  for(int id:{IDC_ASSOC_LIST,IDC_ASSOC_APPLY,IDC_DEFAULT_APPS,IDC_ASSOC_ALL,IDC_ASSOC_NONE})ShowWindow(GetDlgItem(dialog,id),page==1?SW_SHOW:SW_HIDE);
  for(int id:{IDC_SHELL_INSTALL,IDC_SHELL_REMOVE})ShowWindow(GetDlgItem(dialog,id),page==2?SW_SHOW:SW_HIDE);
  for(int id:{IDC_PREF_TREE,IDC_PREF_STATUS,IDC_PREF_COLOR,IDC_PREF_FONT,IDC_PREF_FONT_SIZE,IDC_PREF_COLUMNS})ShowWindow(GetDlgItem(dialog,id),page==5?SW_SHOW:SW_HIDE);
  for(int id:{IDC_PREF_PARALLEL,IDC_PREF_CODEPAGE,IDC_PREF_CLOSE_EXTRACT})ShowWindow(GetDlgItem(dialog,id),page==3?SW_SHOW:SW_HIDE);
  ShowWindow(GetDlgItem(dialog,IDC_PREF_ZONE),page==6?SW_SHOW:SW_HIDE);
  for(int id:{IDC_PREF_EXPORT,IDC_PREF_IMPORT,IDC_PREF_TEMP,IDC_PREF_TEMP_RESET,IDC_PREF_UPDATE})ShowWindow(GetDlgItem(dialog,id),page==7?SW_SHOW:SW_HIDE);
  ShowWindow(GetDlgItem(dialog,IDC_PREF_LANGUAGE),page==8?SW_SHOW:SW_HIDE);
  const wchar_t* notes[]={
    L"?§Ï†ï?Ä ?ÑÏû¨ Windows ?¨Ïö©?êÏóêÍ≤??Ä?•Îê©?àÎã§. ?ïÏù∏???ÑÎ•¥Î©?Ï¶âÏãú ?ÅÏö©?©Îãà??",
    L"?ïÏû•?êÎ? ?†ÌÉù?òÍ≥† ?∞Í≤∞ ?ÑÎ°úÍ∑∏Îû® ?±Î°ù???ÑÎ•¥Î©?Ï¶âÏãú ?ÅÏö©?©Îãà?? Í∏∞Î≥∏ ??Î≥ÄÍ≤ΩÏ? Windows Í∏∞Î≥∏ ???§Ï†ï?êÏÑú ?†ÌÉù?òÏÑ∏??",
    L"?¥Îûò??Î©îÎâ¥ ?±Î°ù/?úÍ±∞??Ï¶âÏãú ?ÅÏö©?©Îãà?? Windows 11 Ï≤?Î©îÎâ¥???§Ïπò ?ÑÎ°úÍ∑∏Îû®?êÏÑú ?úÎ™Ö ?®ÌÇ§ÏßÄÎ•??±Î°ù?©Îãà?? ?¥Î¶∞?¥zip ?òÏúÑ Î©îÎâ¥?êÏÑú ?ïÏ∂ï/?¥Ï†ú Î™ÖÎ†π???¨Ïö©?????àÏäµ?àÎã§.",
    L"?†ÌÉù???¥Îçî???ïÏ∂ï ?¥Ïö©??ÏßÅÏ†ë ?¥Ï†ú?©Îãà?? Í∞ôÏ? ?¥Î¶Ñ?Ä ??ñ¥?∞Ï? ?äÏäµ?àÎã§.\n\nZIP ÏΩîÎìú?òÏù¥ÏßÄ??Í∏∞Ï°¥ ZIP???åÏùºÎ™??¥ÏÑù???ÅÏö©?©Îãà?? UTF-8 ?úÏãúÍ∞Ä ?àÎäî ?åÏùº?Ä ?êÎûò ?∏ÏΩî?©ÏùÑ ?¨Ïö©?©Îãà?? Î©Ä?∞ÏΩî???úÏö© Î≤îÏúÑ???ïÏ∂ï Î∞©Ïãù???∞Îùº ?§Î¶Ö?àÎã§.",
    L"?†ÌÉù???ïÏ∂ï Î∞©Î≤ïÍ≥?Í≤Ä???µÏÖò?????ïÏ∂ï Ï∞ΩÏùò Í∏∞Î≥∏Í∞íÏúºÎ°??ÅÏö©?©Îãà??",
    L"?¥Îçî ?∏Î¶¨, ?ÅÌÉú ?úÏãúÏ§? ?åÎßà, ?åÏùº Î™©Î°ù Í∏ÄÍº¥Í≥º ?¨Í∏∞Î•??§Ï†ï?©Îãà?? ÏπºÎüº Î≤ÑÌäº?êÏÑú ?¨Í∏∞/?†Ïßú/?îÌò∏ ???úÏãúÎ•??†ÌÉù?©Îãà?? ?¥Î?ÏßÄ¬∑?çÏä§?∏Îäî Î≥¥Í∏∞ Î©îÎâ¥?êÏÑú ÎØ∏Î¶¨Î≥????àÏäµ?àÎã§.",
    L"?¥Ï†ú ??Í≤ΩÎ°ú ?àÏ∂ú, ÎßÅÌÅ¨, ?ïÍ∑ú????Ï§ëÎ≥µ Í≤ΩÎ°úÎ•?Í≤Ä?¨Ìï©?àÎã§. ?§Ïö¥Î°úÎìú Ï∂úÏ≤òÎ•?Î≥¥Ï°¥?????àÏäµ?àÎã§. ?åÏùº Î©îÎâ¥??Microsoft Defender Í≤Ä?¨Ï? ?ÑÏû¨ ?¨Ïö©???êÍ≤© Ï¶ùÎ™Ö Í¥ÄÎ¶¨ÏûêÎ•??µÌïú Î™ÖÏãú???îÌò∏ ?Ä?•ÏùÑ ÏßÄ?êÌï©?àÎã§.",
    L"?ÑÏû¨ ?úÎèÑ: ??™© 100,000Í∞?/ ?†Ïñ∏???¥Ï†ú ?¨Í∏∞ 20 GiB. ?òÍ≤Ω?§Ï†ï INI?êÎäî ?îÌò∏Î•??¨Ìï®?òÏ? ?äÏäµ?àÎã§. ?ÑÏãú ?¥Îçî Í≤ΩÎ°ú????PC?êÎßå ?Ä?•Ìï©?àÎã§. ?¥Ï†ú Í≤∞Í≥º???Ä???¥Îçî?êÏÑú Í≤ÄÏ¶????¥Îèô?©Îãà??",
    L"?úÍµ≠??/ English\n\nÎ©îÎâ¥?Ä Í∏∞Î≥∏ ?Ä?îÏÉÅ???∏Ïñ¥???§Ïùå ?§ÌñâÎ∂Ä???ÅÏö©?©Îãà?? ?ºÎ? ?îÏßÑ ?§Î•ò?Ä ?ÅÏÑ∏ ?àÎÇ¥???úÍµ≠?¥Î°ú ?úÏãú?????àÏäµ?àÎã§."
  };
  const wchar_t* englishNotes[]={
    L"Preferences are stored for the current Windows user and applied when you choose OK.",
    L"Register the selected extensions, then choose default apps in Windows Settings.",
    L"Classic menu registration applies immediately. The installer registers the Windows 11 identity package. Compression and extraction commands appear under EoriniZip.",
    L"Extract directly to the chosen folder. Existing names get a number. ZIP code pages apply to legacy names; UTF-8 names retain their encoding. macOS decomposed names are composed when extracted.",
    L"The selected level and test option are defaults for new archives. GZIP/BZIP2/XZ take one file. ISO takes one folder. WIM is stored without compression.",
    L"Choose the folder tree, status bar, theme, list font and size. The columns button selects size, date and encryption columns. Preview images and text from the View menu.",
    L"Extraction checks paths, links and Unicode name collisions. Origin information can be preserved. Use File > Microsoft Defender scan and Settings > Save password for explicit current-user credential storage.",
    L"Limits: 100,000 entries / 20 GiB declared expanded data. INI export excludes passwords. Temporary folder paths are stored only on this PC. Updates require a valid trusted installer signed by the same signer as the installed app.",
    L"Korean / English. Menu and basic dialog labels change after restarting. Some detailed messages and engine errors can remain Korean."
  };
  SetDlgItemTextW(dialog,IDC_SETTINGS_NOTE,(kz::ui::english?englishNotes:notes)[std::clamp(page,0,8)]);
}
void fillSettings(HWND dialog,const PreferencesData& p) {
  pendingTheme=p.theme;
  SendDlgItemMessageW(dialog,IDC_PREF_LANGUAGE,CB_SETCURSEL,p.language,0);
  CheckDlgButton(dialog,IDC_PREF_UPDATE,p.checkUpdates?BST_CHECKED:BST_UNCHECKED);
  SendDlgItemMessageW(dialog,IDC_PREF_FONT,CB_SETCURSEL,p.fontFamily,0);SendDlgItemMessageW(dialog,IDC_PREF_FONT_SIZE,CB_SETCURSEL,p.fontSize,0);
  SetWindowLongPtrW(GetDlgItem(dialog,IDC_PREF_COLUMNS),GWLP_USERDATA,p.columnMask);
  CheckDlgButton(dialog,IDC_PREF_CLOSE_CREATE,p.closeAfterCreate?BST_CHECKED:BST_UNCHECKED);
  CheckDlgButton(dialog,IDC_PREF_CLOSE_EXTRACT,p.closeAfterExtract?BST_CHECKED:BST_UNCHECKED);
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
INT_PTR CALLBACK settingsProcedure(HWND dialog,UINT msg,WPARAM wp,LPARAM lp) {
  if(msg==WM_INITDIALOG){kz::ui::dialog(dialog);
    SendDlgItemMessageW(dialog,IDC_PREF_LANGUAGE,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(L"?úÍµ≠??));SendDlgItemMessageW(dialog,IDC_PREF_LANGUAGE,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(L"English"));
    pendingTemporaryFolder=temporaryFolder;
    for(auto family:{L"Segoe UI",L"ÎßëÏ? Í≥†Îîï",L"Íµ¥Î¶º"})SendDlgItemMessageW(dialog,IDC_PREF_FONT,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(family));
    for(auto size:{L"Í∏ÄÍº??¨Í∏∞: 12",L"Í∏ÄÍº??¨Í∏∞: 14",L"Í∏ÄÍº??¨Í∏∞: 16",L"Í∏ÄÍº??¨Í∏∞: 18",L"Í∏ÄÍº??¨Í∏∞: 20"})SendDlgItemMessageW(dialog,IDC_PREF_FONT_SIZE,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(size));
    for(auto title:{L"?ºÎ∞ò ?§Ï†ï",L"?åÏùº ?∞Í≤∞",L"?êÏÉâÍ∏?Î©îÎâ¥",L"?ïÏ∂ï ?ÄÍ∏?,L"?ïÏ∂ï?òÍ∏∞",L"Î≥¥Í∏∞/?∏Ïßë",L"Î≥¥Ïïà ?§Ï†ï",L"Í≥†Í∏â ?§Ï†ï",L"?∏Ïñ¥ ?§Ï†ï"})
      SendDlgItemMessageW(dialog,IDC_CATEGORIES,LB_ADDSTRING,0,reinterpret_cast<LPARAM>(title));
    for(auto title:{L"?ïÏ∂ï?òÏ? ?äÏùå",L"Îπ†Î•¥Í≤?,L"Î≥¥ÌÜµ",L"ÏµúÎ?Î°?})
      SendDlgItemMessageW(dialog,IDC_PREF_LEVEL,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(title));
    for(auto label:{L"ZIP ÏΩîÎìú?òÏù¥ÏßÄ: ?êÎèô",L"?úÍµ≠??(949)",L"?ºÎ≥∏??(932)",L"Ï§ëÍµ≠??Í∞ÑÏ≤¥ (936)",L"Ï§ëÍµ≠??Î≤àÏ≤¥ (950)",L"?úÏú†?ΩÏñ¥ (1252)",L"UTF-8 (65001)"})
      SendDlgItemMessageW(dialog,IDC_PREF_CODEPAGE,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(label));
    for(size_t i=0;i<kz::win::extensions().size();++i){
      const auto& ext=kz::win::extensions()[i];
      SendDlgItemMessageW(dialog,IDC_ASSOC_LIST,LB_ADDSTRING,0,reinterpret_cast<LPARAM>(ext.c_str()));
      SendDlgItemMessageW(dialog,IDC_ASSOC_LIST,LB_SETSEL,kz::win::associated(ext),i);
    }
    SendDlgItemMessageW(dialog,IDC_CATEGORIES,LB_SETCURSEL,std::clamp(static_cast<int>(lp),0,8),0);fillSettings(dialog,preferences);settingsPage(dialog);return TRUE;
  }
  if(msg==WM_CTLCOLORLISTBOX&&reinterpret_cast<HWND>(wp)){
    SetBkColor(reinterpret_cast<HDC>(wp),RGB(255,225,122));SetTextColor(reinterpret_cast<HDC>(wp),RGB(65,48,12));
    SetDCBrushColor(reinterpret_cast<HDC>(wp),RGB(255,225,122));return reinterpret_cast<INT_PTR>(GetStockObject(DC_BRUSH));
  }
  if(msg==WM_CLOSE){EndDialog(dialog,IDCANCEL);return TRUE;}
  if(msg!=WM_COMMAND)return FALSE;
  if(LOWORD(wp)==IDC_CATEGORIES&&HIWORD(wp)==LBN_SELCHANGE){settingsPage(dialog);return TRUE;}
  if(LOWORD(wp)==IDC_ASSOC_ALL||LOWORD(wp)==IDC_ASSOC_NONE){
    SendDlgItemMessageW(dialog,IDC_ASSOC_LIST,LB_SETSEL,LOWORD(wp)==IDC_ASSOC_ALL,static_cast<LPARAM>(-1));
    return TRUE;
  }
  if(LOWORD(wp)==IDC_ASSOC_APPLY){
    try{
      std::vector<bool> selected;for(size_t i=0;i<kz::win::extensions().size();++i)selected.push_back(SendDlgItemMessageW(dialog,IDC_ASSOC_LIST,LB_GETSEL,i,0)>0);
      kz::win::associations(selected);SetDlgItemTextW(dialog,IDC_SETTINGS_NOTE,L"?∞Í≤∞ ?ÑÎ°úÍ∑∏Îû® Î™©Î°ù???±Î°ù?àÏäµ?àÎã§. Í∏∞Î≥∏ ??Î≥ÄÍ≤ΩÏ? Windows ?§Ï†ï?êÏÑú ?†ÌÉù?òÏÑ∏??");
    }catch(const std::exception& e){SetDlgItemTextW(dialog,IDC_SETTINGS_NOTE,kz::widen(e.what()).c_str());}return TRUE;
  }
  if(LOWORD(wp)==IDC_DEFAULT_APPS){ShellExecuteW(dialog,L"open",L"ms-settings:defaultapps",nullptr,nullptr,SW_SHOWNORMAL);return TRUE;}
  if(LOWORD(wp)==IDC_SHELL_INSTALL||LOWORD(wp)==IDC_SHELL_REMOVE){
    try{kz::win::shellRegistration(LOWORD(wp)==IDC_SHELL_INSTALL);SetDlgItemTextW(dialog,IDC_SETTINGS_NOTE,L"?êÏÉâÍ∏?Î©îÎâ¥ ?§Ï†ï???ÅÏö©?àÏäµ?àÎã§. ???∞ÌÅ¥Î¶?Î©îÎâ¥Î∂Ä???ÅÏö©?©Îãà??");}
    catch(const std::exception& e){SetDlgItemTextW(dialog,IDC_SETTINGS_NOTE,kz::widen(e.what()).c_str());}return TRUE;
  }
  if(LOWORD(wp)==IDC_PREF_COLOR){
    static COLORREF custom[16]{};CHOOSECOLORW choice{sizeof(choice)};choice.hwndOwner=dialog;choice.rgbResult=pendingTheme;choice.lpCustColors=custom;choice.Flags=CC_FULLOPEN|CC_RGBINIT;
    if(ChooseColorW(&choice))pendingTheme=choice.rgbResult;return TRUE;
  }
  if(LOWORD(wp)==IDC_PREF_TEMP||LOWORD(wp)==IDC_PREF_TEMP_RESET){
    if(LOWORD(wp)==IDC_PREF_TEMP_RESET)pendingTemporaryFolder.clear();else{auto folders=choose(true,false,false,L"",dialog);if(folders.empty())return TRUE;pendingTemporaryFolder=folders[0];}
    SetDlgItemTextW(dialog,IDC_SETTINGS_NOTE,(L"?ïÏù∏???ÑÎ•¥Î©??ÑÏãú ?¥Îçî ?§Ï†ï???Ä?•Ìï©?àÎã§: "+(pendingTemporaryFolder.empty()?std::wstring(L"OS Í∏∞Î≥∏ ?ÑÏãú ?¥Îçî"):pendingTemporaryFolder.wstring())).c_str());return TRUE;
  }
  if(LOWORD(wp)==IDC_PREF_COLUMNS){
    HMENU menu=CreatePopupMenu();const wchar_t* labels[]={L"?êÎ≥∏ ?¨Í∏∞",L"?ïÏ∂ï ?¨Í∏∞",L"?òÏ†ï ?†Ïßú",L"?îÌò∏"};int mask=static_cast<int>(GetWindowLongPtrW(GetDlgItem(dialog,IDC_PREF_COLUMNS),GWLP_USERDATA));
    for(int i=0;i<4;++i)AppendMenuW(menu,MF_STRING|((mask&(1<<i))?MF_CHECKED:0),i+1,labels[i]);POINT point;GetCursorPos(&point);auto selected=TrackPopupMenu(menu,TPM_RETURNCMD|TPM_NONOTIFY,point.x,point.y,0,dialog,nullptr);DestroyMenu(menu);
    if(selected>=1&&selected<=4)SetWindowLongPtrW(GetDlgItem(dialog,IDC_PREF_COLUMNS),GWLP_USERDATA,mask^(1<<(selected-1)));return TRUE;
  }
  if(LOWORD(wp)==IDC_PREF_EXPORT){
    try{
      auto paths=choose(false,false,true,L"EoriniZip-settings.ini",dialog);if(paths.empty())return TRUE;
      std::ofstream output(paths[0],std::ios::binary);
      output<<kz::exportPreferences(preferences);
      output.close();
      if(!output)throw std::runtime_error("?§Ï†ï ?åÏùº???Ä?•Ìï† ???ÜÏäµ?àÎã§.");
      SetDlgItemTextW(dialog,IDC_SETTINGS_NOTE,L"?ÑÏû¨ ?Ä?•Îêú ?òÍ≤Ω?§Ï†ï???¥Î≥¥?àÏäµ?àÎã§.");
    }catch(const std::exception& e){SetDlgItemTextW(dialog,IDC_SETTINGS_NOTE,kz::widen(e.what()).c_str());}return TRUE;
  }
  if(LOWORD(wp)==IDC_PREF_IMPORT){
    try{
      auto paths=choose(false,false,false,L"",dialog);if(paths.empty())return TRUE;
      std::ifstream input(paths[0],std::ios::binary);
      if(!input)throw std::runtime_error("?§Ï†ï ?åÏùº???ΩÏùÑ ???ÜÏäµ?àÎã§.");
      std::string contents(65537,'\0');input.read(contents.data(),static_cast<std::streamsize>(contents.size()));
      if(input.bad())throw std::runtime_error("?§Ï†ï ?åÏùº ?ΩÍ∏∞???§Ìå®?àÏäµ?àÎã§.");
      contents.resize(static_cast<size_t>(input.gcount()));
      auto imported=kz::importPreferences(contents);
      fillSettings(dialog,imported);
      SetDlgItemTextW(dialog,IDC_SETTINGS_NOTE,L"?§Ï†ï??Í∞Ä?∏Ïôî?µÎãà?? ?ïÏù∏???ÑÎ•¥Î©??Ä?•ÎêòÍ≥? Ï∑®ÏÜå?òÎ©¥ Í∏∞Ï°¥ ?§Ï†ï???†Ï??©Îãà??");
    }catch(const std::exception& e){SetDlgItemTextW(dialog,IDC_SETTINGS_NOTE,kz::widen(e.what()).c_str());}return TRUE;
  }
  if(LOWORD(wp)==IDC_PREF_RESET){fillSettings(dialog,PreferencesData{});pendingTemporaryFolder.clear();return TRUE;}
  if(LOWORD(wp)==IDCANCEL){EndDialog(dialog,IDCANCEL);return TRUE;}
  if(LOWORD(wp)==IDOK){
    auto previous=preferences;
    preferences.closeAfterCreate=IsDlgButtonChecked(dialog,IDC_PREF_CLOSE_CREATE)==BST_CHECKED;
    preferences.closeAfterExtract=IsDlgButtonChecked(dialog,IDC_PREF_CLOSE_EXTRACT)==BST_CHECKED;
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
    preferences.checkUpdates=IsDlgButtonChecked(dialog,IDC_PREF_UPDATE)==BST_CHECKED;
    preferences.language=std::clamp(static_cast<int>(SendDlgItemMessageW(dialog,IDC_PREF_LANGUAGE,CB_GETCURSEL,0,0)),0,1);
    preferences.fontFamily=std::clamp(static_cast<int>(SendDlgItemMessageW(dialog,IDC_PREF_FONT,CB_GETCURSEL,0,0)),0,2);preferences.fontSize=std::clamp(static_cast<int>(SendDlgItemMessageW(dialog,IDC_PREF_FONT_SIZE,CB_GETCURSEL,0,0)),0,4);preferences.columnMask=static_cast<int>(GetWindowLongPtrW(GetDlgItem(dialog,IDC_PREF_COLUMNS),GWLP_USERDATA));
    if(!savePreferences()){preferences=previous;MessageBoxW(dialog,L"?òÍ≤Ω ?§Ï†ï???Ä?•Ìï† ???ÜÏäµ?àÎã§.",L"?¥Î¶∞?¥zip",MB_OK|MB_ICONERROR);return TRUE;}
    try{kz::win::registryString(preferencesKey,L"TemporaryFolder",pendingTemporaryFolder.wstring());temporaryFolder=pendingTemporaryFolder;}catch(const std::exception& e){MessageBoxW(dialog,kz::widen(e.what()).c_str(),L"?¥Î¶∞?¥zip",MB_OK|MB_ICONERROR);return TRUE;}
    applyViewPreferences();
    if(preferences.language!=previous.language)MessageBoxW(dialog,L"?∏Ïñ¥ ?§Ï†ï?Ä ?§Ïùå ?§ÌñâÎ∂Ä???ÅÏö©?©Îãà??",L"?¥Î¶∞?¥zip",MB_OK|MB_ICONINFORMATION);
    applyModernTheme(list);applyModernTheme(tree);
      ListView_SetExtendedListViewStyle(list,LVS_EX_DOUBLEBUFFER|(preferences.fullRow?LVS_EX_FULLROWSELECT:0)|(preferences.grid?LVS_EX_GRIDLINES:0));
    treeVisible=preferences.showTree;statusVisible=preferences.showStatus;ShowWindow(statusText,statusVisible?SW_SHOW:SW_HIDE);layout();InvalidateRect(window,nullptr,TRUE);
    EndDialog(dialog,IDOK);return TRUE;
  }
  return FALSE;
}
INT_PTR CALLBACK decryptProcedure(HWND dialog,UINT msg,WPARAM wp,LPARAM lp) {
  if(msg==WM_INITDIALOG){kz::ui::dialog(dialog);SetWindowLongPtrW(dialog,DWLP_USER,lp);SendDlgItemMessageW(dialog,IDC_PASSWORD,EM_SETLIMITTEXT,4096,0);return TRUE;}
  if(msg==WM_CLOSE||(msg==WM_COMMAND&&LOWORD(wp)==IDCANCEL)){SetDlgItemTextW(dialog,IDC_PASSWORD,L"");EndDialog(dialog,IDCANCEL);return TRUE;}
  if(msg==WM_COMMAND&&LOWORD(wp)==IDOK){
    auto value=text(GetDlgItem(dialog,IDC_PASSWORD));if(value.empty())return TRUE;
    *reinterpret_cast<std::wstring*>(GetWindowLongPtrW(dialog,DWLP_USER))=std::move(value);
    SetDlgItemTextW(dialog,IDC_PASSWORD,L"");EndDialog(dialog,IDOK);return TRUE;
  }return FALSE;
}
bool askArchivePassword(std::wstring& secret) {
  return DialogBoxParamW(GetModuleHandleW(nullptr),MAKEINTRESOURCEW(IDD_DECRYPT),window,decryptProcedure,reinterpret_cast<LPARAM>(&secret))==IDOK;
}void decryptFile(const kz::fs::path& input) {
  std::wstring secret;
  if(DialogBoxParamW(GetModuleHandleW(nullptr),MAKEINTRESOURCEW(IDD_DECRYPT),window,decryptProcedure,reinterpret_cast<LPARAM>(&secret))!=IDOK)return;
  auto passwordValue=kz::narrow(secret);SecureZeroMemory(secret.data(),secret.size()*sizeof(wchar_t));secret.clear();
  start([input,passwordValue=std::move(passwordValue)]{return Result{{},false,"EasyCrypt ?¥Ï†ú Î∞?Î¨¥Í≤∞??Í≤Ä???ÑÎ£å: "+engine->decryptEasyCrypt(input,kz::fs::absolute(input).parent_path(),passwordValue).u8string()};},2);
}
INT_PTR CALLBACK renameProcedure(HWND dialog,UINT msg,WPARAM wp,LPARAM lp){
  if(msg==WM_INITDIALOG){kz::ui::dialog(dialog);SetWindowLongPtrW(dialog,DWLP_USER,lp);auto value=reinterpret_cast<std::wstring*>(lp);SetDlgItemTextW(dialog,IDC_RENAME_TEXT,value->c_str());SendDlgItemMessageW(dialog,IDC_RENAME_TEXT,EM_SETLIMITTEXT,255,0);return TRUE;}
  if(msg==WM_CLOSE||(msg==WM_COMMAND&&LOWORD(wp)==IDCANCEL)){EndDialog(dialog,IDCANCEL);return TRUE;}
  if(msg==WM_COMMAND&&LOWORD(wp)==IDOK){auto value=text(GetDlgItem(dialog,IDC_RENAME_TEXT));if(value.empty()||value.find_first_of(L"/\\")!=std::wstring::npos)return TRUE;*reinterpret_cast<std::wstring*>(GetWindowLongPtrW(dialog,DWLP_USER))=value;EndDialog(dialog,IDOK);return TRUE;}return FALSE;
}
void previewTask(const kz::fs::path& input,const std::string& name,const std::string& secret){
  start([=]{
    auto entries=engine->list(input,secret);auto found=std::find_if(entries.begin(),entries.end(),[&](const auto& e){return normalized(e.name)==normalized(name);});
    if(found==entries.end()||found->directory||found->size>32*1024*1024)throw std::runtime_error("ÎØ∏Î¶¨Î≥¥Í∏∞ ??™©???ÜÍ±∞??32 MiB ?úÎèÑÎ•?Ï¥àÍ≥º?àÏäµ?àÎã§.");
    if(found->encrypted&&secret.empty())throw kz::PasswordRequired("ÎØ∏Î¶¨Î≥¥Í∏∞???îÌò∏Í∞Ä ?ÑÏöî?©Îãà??");
    auto relative=kz::fs::u8path(kz::composeFilename(normalized(name)));if(!kz::win::imagePreview(relative)&&!kz::win::textPreview(relative))throw std::runtime_error("?¥Î?ÏßÄ(PNG/JPG/BMP/GIF)?Ä ?çÏä§???åÏùºÎß?ÎØ∏Î¶¨Î≥¥Í∏∞?????àÏäµ?àÎã§.");
    kz::fs::path root;for(int n=0;n<100;++n){auto candidate=engine->temporaryDirectory()/("eorinizip-preview-"+std::to_string(GetCurrentProcessId())+"-"+std::to_string(GetTickCount64())+"-"+std::to_string(n));if(kz::fs::create_directory(candidate)){root=candidate;break;}}
    if(root.empty())throw std::runtime_error("ÎØ∏Î¶¨Î≥¥Í∏∞ ?ÑÏãú ?¥ÎçîÎ•?ÎßåÎì§ ???ÜÏäµ?àÎã§.");
    try{engine->extract(input,root,secret,{name});Result result;result.message="ÎØ∏Î¶¨Î≥¥Í∏∞ Ï§ÄÎπ??ÑÎ£å";result.previewRoot=root;result.previewFile=root/relative;return result;}
    catch(...){std::error_code ec;kz::fs::remove_all(root,ec);throw;}
  },0,[=](std::string next){SetWindowTextW(password,kz::widen(next).c_str());previewTask(input,name,next);});
}
void editTask(const kz::fs::path& input,const std::string& action,const std::vector<std::string>& names,const std::string& secret){
  start([=]{engine->edit(input,action,names,secret);return Result{engine->list(input,secret),true,"?ïÏ∂ï ?åÏùº ?òÏ†ï Î∞?Í≤Ä???ÑÎ£å"};},1,[=](std::string next){SetWindowTextW(password,kz::widen(next).c_str());editTask(input,action,names,next);});
}
INT_PTR CALLBACK commentProcedure(HWND dialog,UINT message,WPARAM wp,LPARAM lp){
  if(message==WM_INITDIALOG){kz::ui::dialog(dialog);SetWindowLongPtrW(dialog,DWLP_USER,lp);SetDlgItemTextW(dialog,IDC_COMMENT_TEXT,reinterpret_cast<std::wstring*>(lp)->c_str());SendDlgItemMessageW(dialog,IDC_COMMENT_TEXT,EM_SETLIMITTEXT,65535,0);return TRUE;}
  if(message==WM_CLOSE||(message==WM_COMMAND&&LOWORD(wp)==IDCANCEL)){EndDialog(dialog,IDCANCEL);return TRUE;}
  if(message==WM_COMMAND&&LOWORD(wp)==IDOK){*reinterpret_cast<std::wstring*>(GetWindowLongPtrW(dialog,DWLP_USER))=text(GetDlgItem(dialog,IDC_COMMENT_TEXT));EndDialog(dialog,IDOK);return TRUE;}return FALSE;
}
void commentTask(const kz::fs::path& input,const std::string& value,const std::string& secret){
  start([=]{engine->setComment(input,value,secret);return Result{{},false,"ZIP ?§Î™Ö ?Ä??Î∞?Î¨¥Í≤∞??Í≤Ä???ÑÎ£å"};},1,[=](std::string next){SetWindowTextW(password,kz::widen(next).c_str());commentTask(input,value,next);});
}
kz::fs::path taskFolder(){
  for(int n=0;n<100;++n){auto path=engine->temporaryDirectory()/("eorinizip-transfer-"+std::to_string(GetCurrentProcessId())+"-"+std::to_string(GetTickCount64())+"-"+std::to_string(n));if(kz::fs::create_directory(path))return path;}
  throw std::runtime_error("?ÑÏãú ?¥ÎçîÎ•?ÎßåÎì§ ???ÜÏäµ?àÎã§.");
}
void transferTask(const kz::fs::path& input,const std::vector<std::string>& names,const std::string& secret,bool edit){
  start([=]{
    const auto size=kz::fs::file_size(input);const auto stamp=kz::fs::last_write_time(input);
    auto entries=engine->list(input,secret);
    if(edit){auto found=std::find_if(entries.begin(),entries.end(),[&](const auto& e){return normalized(e.name)==normalized(names[0]);});
      if(found==entries.end()||found->directory||found->size>32*1024*1024||!kz::win::textPreview(kz::fs::u8path(found->name)))throw std::runtime_error("?∏Î? ?∏Ïßë?Ä 32 MiB ?¥Ìïò ?çÏä§???åÏùº ?òÎÇòÎ•??†ÌÉù?òÏÑ∏??");}
    auto root=taskFolder();
    try{
      engine->extract(input,root,secret,names);Result result;result.message="?¥Î¶ΩÎ≥¥Îìú???åÏùº Î≥µÏÇ¨ ?ÑÎ£å (?ÑÎ°úÍ∑∏Îû®???¥Ïñ¥ ???ÅÌÉú?êÏÑú Î∂ôÏó¨?£Ïúº?∏Ïöî)";
      if(edit){result.editFile=root/kz::fs::u8path(kz::composeFilename(normalized(names[0])));result.editRoot=root;result.editName=names[0];result.editArchive=input;result.editPassword=secret;result.editSize=size;result.editStamp=stamp;}
      else result.clipboardRoot=root;
      return result;
    }catch(...){std::error_code ec;kz::fs::remove_all(root,ec);throw;}
  },0,[=](std::string next){SetWindowTextW(password,kz::widen(next).c_str());transferTask(input,names,next,edit);});
}
void launchUpdateCheck(bool quiet){
  auto script=kz::fs::path(kz::win::executablePath()).parent_path()/L"check-updates.ps1";
  if(!kz::fs::is_regular_file(script))throw std::runtime_error("?ÖÎç∞?¥Ìä∏ ?ïÏù∏ ?§ÌÅ¨Î¶ΩÌä∏Í∞Ä ?§Ïπò ?¥Îçî???ÜÏäµ?àÎã§.");
  wchar_t system[MAX_PATH];GetSystemDirectoryW(system,MAX_PATH);auto powershell=kz::fs::path(system)/L"WindowsPowerShell/v1.0/powershell.exe";
  auto line=kz::quoteWindows(powershell.wstring())+L" -NoProfile -ExecutionPolicy Bypass -File "+kz::quoteWindows(script.wstring())+L" -CurrentVersion 0.5.2"+(quiet?L" -Quiet":L"");
  STARTUPINFOW startup{sizeof(startup)};PROCESS_INFORMATION process{};
  if(!CreateProcessW(powershell.c_str(),line.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&startup,&process))throw std::runtime_error("?ÖÎç∞?¥Ìä∏ ?ïÏù∏???úÏûë?????ÜÏäµ?àÎã§.");
  CloseHandle(process.hProcess);CloseHandle(process.hThread);
}
bool easyCryptFile(const kz::fs::path& file){auto ext=file.extension().wstring();return _wcsicmp(ext.c_str(),L".ezc")==0;}void command(int id) {
  if(id==Cancel){if(engine)engine->cancel();return;}
  if(busy)return;
  try {
    if(id>=RecentFirst&&id<RecentFirst+10){auto index=static_cast<size_t>(id-RecentFirst);if(index>=recentFiles.size())return;archive=recentFiles[index];SetWindowTextW(password,L"");SetWindowTextW(pathText,archive.c_str());reload();return;}
    if(id==RecentClear){recentFiles.clear();RegDeleteTreeW(HKEY_CURRENT_USER,L"Software\\EoriniZip\\Recent");return;}
    if(id==NewAdminWindow){auto executable=kz::win::executablePath();ShellExecuteW(window,L"runas",executable.c_str(),nullptr,nullptr,SW_SHOWNORMAL);return;}
    if(id==ToggleTree){treeVisible=!treeVisible;preferences.showTree=treeVisible;if(!savePreferences())throw std::runtime_error("?§Ï†ï???Ä?•Ìï† ???ÜÏäµ?àÎã§.");layout();return;}
    if(id==ToggleToolbar){preferences.showToolbar=!preferences.showToolbar;if(!savePreferences())throw std::runtime_error("?§Ï†ï???Ä?•Ìï† ???ÜÏäµ?àÎã§.");layout();return;}
    if(id==CheckUpdates){launchUpdateCheck(false);return;}
    if(id==ViewSettings){DialogBoxParamW(GetModuleHandleW(nullptr),MAKEINTRESOURCEW(IDD_SETTINGS),window,settingsProcedure,5);return;}
    if(id==Preferences||id==ColorSettings){DialogBoxParamW(GetModuleHandleW(nullptr),MAKEINTRESOURCEW(IDD_SETTINGS),window,settingsProcedure,id==ColorSettings?5:0);return;}
    if(id==Exit){SendMessageW(window,WM_CLOSE,0,0);return;}
    if(id==About){MessageBoxW(window,L"?¥Î¶∞?¥zip 0.5.1 ¬∑ ?§Ïù¥?∞Î∏å Í∞úÎ∞ú Î≤ÑÏ†Ñ\n\nZIP / 7Z / TAR ?ïÏ∂ï Î∞??¥Ï†ú\nALZ / EGG / RAR ?¥Ï†ú, ZPAQ ?ïÏ∂ï¬∑?¥Ï†ú\nEasyCrypt 2.4 Î≥µÌò∏??n\nÎ∂ÑÌï† Î©îÎâ¥???úÎπÑ???¥Î¶Ñ?Ä ?©Îüâ ?ÑÎ¶¨?ãÏù¥Î©??ÑÏû¨ ?úÎπÑ?§Ïùò ?ÖÎ°ú???úÎèÑÎ•?Î≥¥Ïû•?òÏ? ?äÏäµ?àÎã§.",L"?¥Î¶∞?¥zip ?ïÎ≥¥",MB_OK);return;}
    if(id==Home){showHome(!home);return;}
    if(id==CloseArchive){archive.clear();archiveEntries.clear();visibleEntries.clear();TreeView_DeleteAllItems(tree);ListView_DeleteAllItems(list);SetWindowTextW(pathText,L"");SetWindowTextW(password,L"");showHome(true);return;}
    if(id==Topmost){onTop=!onTop;SetWindowPos(window,onTop?HWND_TOPMOST:HWND_NOTOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE);return;}
    if(id==ToggleStatus){statusVisible=!statusVisible;preferences.showStatus=statusVisible;if(!savePreferences())throw std::runtime_error("?§Ï†ï???Ä?•Ìï† ???ÜÏäµ?àÎã§.");ShowWindow(statusText,statusVisible?SW_SHOW:SW_HIDE);layout();return;}
    if(id==NewWindow||id==AppFolder){
      wchar_t executable[32768];GetModuleFileNameW(nullptr,executable,32768);
      auto target=id==NewWindow?kz::fs::path(executable):kz::fs::path(executable).parent_path();
      if(reinterpret_cast<INT_PTR>(ShellExecuteW(window,L"open",target.c_str(),nullptr,nullptr,SW_SHOWNORMAL))<=32)throw std::runtime_error("Ï∞ΩÏùÑ ?????ÜÏäµ?àÎã§.");
      return;
    }
    if(id==HelpPage||id==IssuesPage||id==Website){
      const wchar_t* target=id==HelpPage?L"https://github.com/sjhdeux0114/EoriniZip#readme":id==IssuesPage?L"https://github.com/sjhdeux0114/EoriniZip/issues":L"https://github.com/sjhdeux0114/EoriniZip";
      ShellExecuteW(window,L"open",target,nullptr,nullptr,SW_SHOWNORMAL);return;
    }
    if(id==FindFile){if(archive.empty())return;DialogBoxParamW(GetModuleHandleW(nullptr),MAKEINTRESOURCEW(IDD_FIND),window,findProcedure,0);return;}
    if(id==SelectAll){ListView_SetItemState(list,-1,LVIS_SELECTED,LVIS_SELECTED);return;}
    if(id==CopyNames){copyNames();return;}
    if(id==Connect){auto files=choose(false);if(files.empty())return;engine=std::make_unique<kz::Engine>(files[0]);SetWindowTextW(statusText,L"7-Zip ?îÏßÑ ?∞Í≤∞??);return;}
    if(id==DecryptEasy){auto files=choose(false);if(!files.empty())decryptFile(files[0]);return;}
    if(id==RepairZip){auto files=choose(false);if(files.empty())return;auto folders=choose(true);if(folders.empty())return;auto input=files[0],parent=folders[0];start([=]{return Result{{},false,"ZIP Î°úÏª¨ ??™© Î≥µÍµ¨ Î∞?Í≤Ä???ÑÎ£å: "+engine->repairZip(input,parent).u8string()};},3);return;}
    if(id==Open){auto files=choose(false);if(files.empty())return;if(easyCryptFile(files[0])){decryptFile(files[0]);return;}archive=files[0];SetWindowTextW(password,L"");ListView_DeleteAllItems(list);SetWindowTextW(pathText,archive.c_str());reload();return;}
    if(id==Refresh){reload();return;}
    if(id==CreateFiles){createArchive();return;}
    if(id==CreateFolder){auto inputs=choose(true);if(!inputs.empty())createArchive(inputs);return;}
    if(archive.empty())throw std::runtime_error("?ïÏ∂ï?åÏùº??Î®ºÏ? ?¨ÏÑ∏??");
    if(id==OpenFolder){ShellExecuteW(window,L"open",archive.parent_path().c_str(),nullptr,nullptr,SW_SHOWNORMAL);return;}
    if(id==DeleteArchive||id==DeleteArchivePermanent){
      if(archive.extension()==L".001")throw std::runtime_error("Î∂ÑÌï† ?åÏùº?Ä ?êÏÉâÍ∏∞Ïóê??Î™®Îì† Î≥ºÎ•®???®Íªò Í¥ÄÎ¶¨Ìïò?∏Ïöî.");
      auto path=archive.wstring();path.push_back(L'\0');path.push_back(L'\0');
      SHFILEOPSTRUCTW operation{};operation.hwnd=window;operation.wFunc=FO_DELETE;operation.pFrom=path.c_str();operation.fFlags=id==DeleteArchive?FOF_ALLOWUNDO:0;
      if(SHFileOperationW(&operation)!=0)throw std::runtime_error("?ïÏ∂ï ?åÏùº????†ú?òÏ? Î™ªÌñà?µÎãà??");
      if(!operation.fAnyOperationsAborted)command(CloseArchive);return;
    }
    if(id==SaveAs){
      if(archive.extension()==L".001")throw std::runtime_error("Î∂ÑÌï† ?åÏùº?Ä Î™®Îì† Î≥ºÎ•®???®Íªò Î≥µÏÇ¨?¥Ïïº ?©Îãà?? ?ïÏ∂ï ?åÏùº???àÎäî ?¥Îçî ?¥Í∏∞Î•??¥Ïö©?òÏÑ∏??");
      auto paths=choose(false,false,true,archive.filename().wstring());if(paths.empty())return;
      if(!kz::fs::copy_file(archive,paths[0],kz::fs::copy_options::none))throw std::runtime_error("Î≥µÏÇ¨?òÏ? Î™ªÌñà?µÎãà??");
      SetWindowTextW(statusText,L"?§Î•∏ ?¥Î¶Ñ?ºÎ°ú ?Ä???ÑÎ£å");return;
    }
    const auto p=kz::narrow(text(password));const auto input=archive;
    if(id==SavePassword){
      auto secret=p;if(secret.empty()){std::wstring value;if(!askArchivePassword(value))return;secret=kz::narrow(value);SecureZeroMemory(value.data(),value.size()*sizeof(wchar_t));}
      if(MessageBoxW(window,L"?ÑÏû¨ ?ïÏ∂ï ?åÏùº???îÌò∏Î•???Windows ?¨Ïö©?êÏùò ?êÍ≤© Ï¶ùÎ™Ö Í¥ÄÎ¶¨Ïûê???Ä?•Ìï†ÍπåÏöî? INI ?§Ï†ï ?åÏùº?êÎäî ?¨Ìï®?òÏ? ?äÏäµ?àÎã§.",L"?îÌò∏ ?Ä??,MB_YESNO|MB_ICONQUESTION)==IDYES){kz::win::saveArchivePassword(input,secret);SetWindowTextW(statusText,L"?ÑÏû¨ ?¨Ïö©???êÍ≤© Ï¶ùÎ™Ö Í¥ÄÎ¶¨Ïûê???îÌò∏Î•??Ä?•Ìñà?µÎãà??");}
      SecureZeroMemory(secret.data(),secret.size());return;
    }
    if(id==LoadPassword){auto secret=kz::win::loadArchivePassword(input);SetWindowTextW(password,kz::widen(secret).c_str());SecureZeroMemory(secret.data(),secret.size());reload();return;}
    if(id==DeletePassword){if(MessageBoxW(window,L"???ïÏ∂ï ?åÏùº???Ä?•Ìïú ?îÌò∏Î•??êÍ≤© Ï¶ùÎ™Ö Í¥ÄÎ¶¨Ïûê?êÏÑú ??†ú?†Íπå??",L"?Ä???îÌò∏ ??†ú",MB_YESNO|MB_ICONQUESTION)==IDYES){kz::win::deleteArchivePassword(input);SetWindowTextW(statusText,L"?Ä???îÌò∏Î•???†ú?àÏäµ?àÎã§.");}return;}
    if(id==RecoverPassword){
      auto files=choose(false);if(files.empty())return;auto candidatesFile=files[0];
      start([=]{
        if(kz::fs::file_size(candidatesFile)>4*1024*1024)throw std::runtime_error("?îÌò∏ ?ÑÎ≥¥ ?åÏùº?Ä UTF-8 ?çÏä§??4 MiB ?¥ÌïòÎ°??†ÌÉù?òÏÑ∏??");
        std::ifstream file(candidatesFile,std::ios::binary);if(!file)throw std::runtime_error("?îÌò∏ ?ÑÎ≥¥ ?åÏùº???ΩÏùÑ ???ÜÏäµ?àÎã§.");
        std::vector<std::string> candidates;std::string line;bool first=true;
        while(std::getline(file,line)){if(first&&line.compare(0,3,"\xef\xbb\xbf")==0)line.erase(0,3);first=false;if(!line.empty()&&line.back()=='\r')line.pop_back();kz::widen(line);if(!line.empty())candidates.push_back(line);if(candidates.size()>10000)throw std::runtime_error("?îÌò∏ ?ÑÎ≥¥??10,000Í∞??¥ÌïòÎ°?ÏßÄ?ïÌïò?∏Ïöî.");}
        if(file.bad())throw std::runtime_error("?îÌò∏ ?ÑÎ≥¥ ?ΩÍ∏∞ ?§Î•ò?ÖÎãà??");
        Result result;result.recoveredPassword=engine->recoverPassword(input,candidates);result.message="?ÑÎ≥¥ Î™©Î°ù?êÏÑú ?îÌò∏Î•?Ï∞æÏïÑ ?ïÏù∏?àÏäµ?àÎã§.";for(auto& candidate:candidates)SecureZeroMemory(candidate.data(),candidate.size());return result;
      },3);return;
    }
    if(id==EditComment){auto value=kz::widen(engine->comment(input));if(DialogBoxParamW(GetModuleHandleW(nullptr),MAKEINTRESOURCEW(IDD_COMMENT),window,commentProcedure,reinterpret_cast<LPARAM>(&value))==IDOK)commentTask(input,kz::narrow(value),p);return;}
    if(id==ScanArchive){
      wchar_t programFiles[32768];if(!GetEnvironmentVariableW(L"ProgramFiles",programFiles,32768))throw std::runtime_error("Windows Defender Í≤ΩÎ°úÎ•?Ï∞æÏùÑ ???ÜÏäµ?àÎã§.");
      auto scanner=kz::fs::path(programFiles)/L"Windows Defender/MpCmdRun.exe";
      if(!kz::fs::is_regular_file(scanner))throw std::runtime_error("Windows Defender Í≤Ä???ÑÍµ¨Í∞Ä ?§Ïπò?òÏ? ?äÏïò?µÎãà??");
      start([=]{engine->scanFile(scanner,input);return Result{{},false,"Microsoft Defender Í≤Ä???ÑÎ£å: ?êÏ? Î≥¥Í≥† ?ÜÏùå. ?îÌò∏?îÎêú ?¥Î? ?åÏùº?Ä ?¥Ï†ú ??Î≥ÑÎèÑÎ°?Í≤Ä?¨Ìïò?∏Ïöî."};},3);return;
    }
    if(id==CopyFiles||id==ExternalEdit){
      std::vector<std::string> names;int row=-1;while((row=ListView_GetNextItem(list,row,LVNI_SELECTED))!=-1)names.push_back(visibleEntries.at(static_cast<size_t>(row)).name);
      if(names.empty()||(id==ExternalEdit&&names.size()!=1))throw std::runtime_error("?ëÏóÖ????™©???†ÌÉù?òÏÑ∏?? ?∏Î? ?∏Ïßë?Ä ?åÏùº ?òÎÇòÎß?Í∞Ä?•Ìï©?àÎã§.");
      transferTask(input,names,p,id==ExternalEdit);return;
    }
    if(id==PasteFiles){
      auto files=kz::win::clipboardFiles(window);std::vector<std::string> names;
      for(const auto& file:files){if(!kz::fs::is_regular_file(file)||kz::fs::is_symlink(file))throw std::runtime_error("Î∂ôÏó¨?£Í∏∞???ºÎ∞ò ?åÏùºÎß?ÏßÄ?êÌï©?àÎã§. ?¥Îçî???àÎ°ú ?ïÏ∂ï???¨Ïö©?òÏÑ∏??");names.push_back(file.u8string());}
      if(names.empty())return;
      if(MessageBoxW(window,L"?¥Î¶ΩÎ≥¥Îìú???åÏùº???ïÏ∂ï ?åÏùº ÏµúÏÉÅ?ÑÏóê Ï∂îÍ??©Îãà?? Í∞ôÏ? ?¥Î? ?¥Î¶Ñ?Ä ÍµêÏ≤¥?©Îãà?? Í≥ÑÏÜç?†Íπå??",L"?åÏùº Î∂ôÏó¨?£Í∏∞",MB_YESNO|MB_ICONQUESTION)==IDYES)editTask(input,"a",names,p);
      return;
    }
    if(id==PreviewEntry){int row=ListView_GetNextItem(list,-1,LVNI_SELECTED);if(row<0||ListView_GetNextItem(list,row,LVNI_SELECTED)>=0)throw std::runtime_error("ÎØ∏Î¶¨Î≥??åÏùº ?òÎÇòÎ•??†ÌÉù?òÏÑ∏??");previewTask(input,visibleEntries.at(row).name,p);return;}
    if(id==AddEntries||id==DeleteEntries||id==RenameEntry){
      std::vector<std::string> names;
      if(id==AddEntries){auto files=choose(false,true);if(files.empty())return;for(const auto& file:files)names.push_back(file.u8string());
        if(MessageBoxW(window,L"?†ÌÉù???åÏùº???ïÏ∂ï ?åÏùº??ÏµúÏÉÅ?ÑÏóê Ï∂îÍ??©Îãà?? Í∞ôÏ? ?¥Î¶Ñ???¥Î? ?åÏùº?Ä Î∞îÎÄùÎãà?? Í≥ÑÏÜç?†Íπå??",L"?åÏùº Ï∂îÍ?",MB_YESNO|MB_ICONQUESTION)!=IDYES)return;
      }else{
        int row=-1;while((row=ListView_GetNextItem(list,row,LVNI_SELECTED))!=-1)names.push_back(visibleEntries.at(static_cast<size_t>(row)).name);
        if(names.empty())throw std::runtime_error("?òÏ†ï????™©??Î®ºÏ? ?†ÌÉù?òÏÑ∏??");
        if(id==DeleteEntries&&MessageBoxW(window,L"?†ÌÉù????™©???ïÏ∂ï ?åÏùº?êÏÑú ??†ú?†Íπå??",L"?åÏùº ??†ú",MB_YESNO|MB_ICONQUESTION)!=IDYES)return;
        if(id==RenameEntry){
          if(names.size()!=1)throw std::runtime_error("?¥Î¶Ñ??Î∞îÍ? ??™© ?òÎÇòÎ•??†ÌÉù?òÏÑ∏??");
          auto old=names[0];std::replace(old.begin(),old.end(),'\\','/');while(!old.empty()&&old.back()=='/')old.pop_back();
          auto slash=old.rfind('/');auto prefix=slash==std::string::npos?std::string{}:old.substr(0,slash+1);
          auto name=kz::widen(old.substr(prefix.size()));
          if(DialogBoxParamW(GetModuleHandleW(nullptr),MAKEINTRESOURCEW(IDD_RENAME),window,renameProcedure,reinterpret_cast<LPARAM>(&name))!=IDOK)return;
          names.push_back(prefix+kz::narrow(name));
        }
      }
      editTask(input,id==AddEntries?"a":id==DeleteEntries?"d":"rn",names,p);return;
    }
    if(id==ExtractSelected){
      std::vector<std::string> selected;int row=-1;while((row=ListView_GetNextItem(list,row,LVNI_SELECTED))!=-1)selected.push_back(visibleEntries.at(static_cast<size_t>(row)).name);
      if(selected.empty())throw std::runtime_error("?¥Ï†ú????™©??Î®ºÏ? ?†ÌÉù?òÏÑ∏??");auto dirs=choose(true);if(dirs.empty())return;archiveTask(input,1,dirs[0],selected,p);return;
    }
    if(id==ExtractSmart||id==Extract){auto dirs=choose(true);if(dirs.empty())return;archiveTask(input,id==ExtractSmart?2:1,dirs[0],{},p);return;}
    if(id==Test)archiveTask(input,4,{}, {},p);  }catch(const std::exception& e){MessageBoxW(window,kz::widen(e.what()).c_str(),L"?¥Î¶∞?¥zip",MB_OK|MB_ICONINFORMATION);}
}
HMENU menus() {
  HMENU bar=CreateMenu();
  auto add=[&](const wchar_t* name){HMENU m=CreatePopupMenu();auto translated=kz::ui::label(name);AppendMenuW(bar,MF_POPUP,reinterpret_cast<UINT_PTR>(m),translated.c_str());return m;};
  auto item=[](HMENU m,UINT id,const wchar_t* label){auto translated=kz::ui::label(label);AppendMenuW(m,MF_STRING,id,translated.c_str());};
  auto sep=[](HMENU m){AppendMenuW(m,MF_SEPARATOR,0,nullptr);};
  auto file=add(L"?åÏùº(&F)");
  item(file,Open,L"?ïÏ∂ï ?åÏùº ?¥Í∏∞\tCtrl+O");recentMenu=CreatePopupMenu();AppendMenuW(file,MF_POPUP,reinterpret_cast<UINT_PTR>(recentMenu),L"ÏµúÍ∑º ?åÏùº");item(file,CreateFiles,L"?àÎ°ú ?ïÏ∂ï\tCtrl+N");sep(file);
  item(file,CloseArchive,L"?ïÏ∂ï ?åÏùº ?´Í∏∞\tF4");item(file,SaveAs,L"?§Î•∏ ?¥Î¶Ñ?ºÎ°ú ?Ä??..");
  item(file,DeleteArchive,L"?ïÏ∂ï ?åÏùº ??†ú\tCtrl+Del");item(file,DeleteArchivePermanent,L"?ïÏ∂ï ?åÏùº ?ÑÏ†Ñ ??†ú\tShift+Ctrl+Del");
  item(file,OpenFolder,L"?ïÏ∂ï ?åÏùº???àÎäî ?¥Îçî ?¥Í∏∞");item(file,Test,L"?ïÏ∂ï ?åÏùº ?åÏä§??tCtrl+T");
  item(file,ScanArchive,L"Microsoft DefenderÎ°??ïÏ∂ï ?åÏùº Í≤Ä??tCtrl+I");item(file,Extract,L"?ïÏ∂ï ?ÄÍ∏?tCtrl+E");item(file,ExtractSelected,L"?†ÌÉù????™© ?ÄÍ∏?..");item(file,ExtractSmart,L"?åÏïÑ???ÄÍ∏?..");sep(file);
  item(file,NewWindow,L"?àÏ∞Ω ?ÑÏö∞Í∏?tCtrl+Shift+N");item(file,NewAdminWindow,L"?àÏ∞Ω ?ÑÏö∞Í∏?Í¥ÄÎ¶¨Ïûê Í∂åÌïú)\tCtrl+Alt+N");sep(file);item(file,Exit,L"Ï¢ÖÎ£å\tCtrl+W");
  auto edit=add(L"?∏Ïßë(&E)");
  item(edit,RenameEntry,L"?¥Î¶Ñ Î∞îÍæ∏Í∏?tF2");item(edit,AddEntries,L"?åÏùº Ï∂îÍ?\tIns");item(edit,DeleteEntries,L"?åÏùº ??†ú\tDel");item(edit,EditComment,L"ZIP ?ïÏ∂ï ?§Î™Ö ?∏Ïßë\tCtrl+Shift+M");sep(edit);
  item(edit,CopyFiles,L"?¥Î¶ΩÎ≥¥ÎìúÎ°??åÏùº Î≥µÏÇ¨?òÍ∏∞\tCtrl+C");item(edit,PasteFiles,L"?¥Î¶ΩÎ≥¥Îìú?êÏÑú ?åÏùº Î∂ôÏó¨ ?£Í∏∞\tCtrl+V");item(edit,ExternalEdit,L"?çÏä§???åÏùº ?∏Î? ?∏Ïßë ??Î∞òÏòÅ...");
  item(edit,CopyNames,L"?åÏùº??Í≤ΩÎ°úÎ™ÖÏùÑ ?¥Î¶ΩÎ≥¥ÎìúÎ°?Î≥µÏÇ¨");item(edit,SelectAll,L"Î™®Îëê ?†ÌÉù\tCtrl+A");
  auto find=add(L"Ï∞æÍ∏∞(&I)");item(find,FindFile,L"?åÏùº Ï∞æÍ∏∞\tCtrl+F");
  auto settings=add(L"?§Ï†ï(&S)");item(settings,Preferences,L"?òÍ≤Ω ?§Ï†ï\tF5");item(settings,Connect,L"?ïÏ∂ï ?îÏßÑ ?∞Í≤∞...");sep(settings);item(settings,SavePassword,L"?ÑÏû¨ ?åÏùº ?îÌò∏ ?Ä??..");item(settings,LoadPassword,L"?Ä?•Ìïú ?îÌò∏ Î∂àÎü¨?§Í∏∞");item(settings,DeletePassword,L"?ÑÏû¨ ?åÏùº ?Ä???îÌò∏ ??†ú...");
  auto view=add(L"Î≥¥Í∏∞(&V)");item(view,Home,L"?úÏûë ?îÎ©¥\tF6");item(view,ToggleTree,L"?∏Î¶¨ Ïª®Ìä∏Î°?tF7");item(view,EditComment,L"ZIP ?ïÏ∂ï ?§Î™Ö\tF8");
  item(view,PreviewEntry,L"?†ÌÉù ?åÏùº ÎØ∏Î¶¨Î≥¥Í∏∞");item(view,ToggleStatus,L"?ÅÌÉú ?úÏãúÏ§?);item(view,ToggleToolbar,L"?¥Î∞î");item(view,ViewSettings,L"?åÏùº Î™©Î°ù Í∏ÄÍº?ÏπºÎüº ?§Ï†ï...");sep(view);
  item(view,Topmost,L"??ÉÅ ?ÑÏóê ?úÏãú");item(view,ColorSettings,L"?âÏÉÅ ?§Ï†ï...");item(view,Refresh,L"?ïÏ∂ï ?åÏùº ?§Ïãú ?ΩÍ∏∞");
  auto tools=add(L"?ÑÍµ¨(&T)");item(tools,RepairZip,L"ZIP Ï§ëÏïô ?îÎ†â?∞Î¶¨ Î≥µÍµ¨ (ÎπÑÏïî?∏Ìôî store/deflate)...");item(tools,RecoverPassword,L"?îÌò∏ ?ÑÎ≥¥ Î™©Î°ù?ºÎ°ú Î≥µÍµ¨ (UTF-8, ??Ï§ÑÏóê ?òÎÇò)...");sep(tools);
  item(tools,DecryptEasy,L"EasyCrypt 2.4 ?åÏùº ?¥Ï†ú...");sep(tools);item(tools,Cancel,L"?ëÏóÖ Ï∑®ÏÜå");
  auto help=add(L"?ÑÏ?Îß?&A)");item(help,About,L"?ïÎ≥¥\tF1");item(help,AppFolder,L"?ÑÎ°úÍ∑∏Îû® ?§Ïπò ?¥Îçî ?¥Í∏∞");sep(help);
  item(help,HelpPage,L"?ÑÏ?Îß??òÏù¥ÏßÄ ?¥Í∏∞");item(help,CheckUpdates,L"?ÖÎç∞?¥Ìä∏ ?ïÏù∏...");item(help,IssuesPage,L"Î¨∏Ïùò / ?§Î•ò ?†Í≥†");item(help,Website,L"?àÌéò?¥Ï? ?¥Í∏∞");return bar;
}
void updateMenus() {
  if(recentMenu){while(GetMenuItemCount(recentMenu)>0)DeleteMenu(recentMenu,0,MF_BYPOSITION);
    for(size_t i=0;i<recentFiles.size();++i){auto label=recentFiles[i].wstring();for(size_t p=0;(p=label.find(L'&',p))!=std::wstring::npos;p+=2)label.insert(p,1,L'&');AppendMenuW(recentMenu,MF_STRING|(busy?MF_GRAYED:0),RecentFirst+i,label.c_str());}
    AppendMenuW(recentMenu,MF_STRING|(busy||recentFiles.empty()?MF_GRAYED:0),RecentClear,L"ÏµúÍ∑º ?åÏùº Î™©Î°ù ÏßÄ?∞Í∏∞");
  }
  HMENU m=GetMenu(window);
  for(int id=Open;id<=LastCommand;++id)EnableMenuItem(m,id,MF_BYCOMMAND|(busy&&id!=Cancel?MF_GRAYED:MF_ENABLED));
  for(int id:{SavePassword,LoadPassword,DeletePassword,RecoverPassword,ScanArchive,CopyFiles,PreviewEntry,DeleteArchive,DeleteArchivePermanent,CloseArchive,SaveAs,OpenFolder,Test,Extract,ExtractSelected,ExtractSmart,FindFile,SelectAll,CopyNames,Refresh})
    EnableMenuItem(m,id,MF_BYCOMMAND|(!busy&&!archive.empty()?MF_ENABLED:MF_GRAYED));
  bool editable=!busy&&(archive.extension()==L".zip"||archive.extension()==L".7z"||archive.extension()==L".ZIP"||archive.extension()==L".7Z");
  for(int id:{AddEntries,DeleteEntries,RenameEntry,PasteFiles,ExternalEdit})EnableMenuItem(m,id,MF_BYCOMMAND|(editable?MF_ENABLED:MF_GRAYED));
  EnableMenuItem(m,EditComment,MF_BYCOMMAND|(!busy&&_wcsicmp(archive.extension().c_str(),L".zip")==0?MF_ENABLED:MF_GRAYED));
  CheckMenuItem(m,ToggleTree,MF_BYCOMMAND|(treeVisible?MF_CHECKED:MF_UNCHECKED));
  CheckMenuItem(m,Home,MF_BYCOMMAND|(home?MF_CHECKED:MF_UNCHECKED));
  CheckMenuItem(m,Topmost,MF_BYCOMMAND|(onTop?MF_CHECKED:MF_UNCHECKED));
  CheckMenuItem(m,ToggleStatus,MF_BYCOMMAND|(statusVisible?MF_CHECKED:MF_UNCHECKED));
  CheckMenuItem(m,ToggleToolbar,MF_BYCOMMAND|(preferences.showToolbar?MF_CHECKED:MF_UNCHECKED));
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
  r.top+=px(82);DrawTextW(dc,d->CtlID==Open?L"?ïÏ∂ï ?åÏùº ?¥Í∏∞":L"?àÎ°ú ?ïÏ∂ï?òÍ∏∞",-1,&r,DT_CENTER|DT_TOP|DT_SINGLELINE);
  if(d->itemState&ODS_FOCUS){r=d->rcItem;InflateRect(&r,-px(5),-px(5));DrawFocusRect(dc,&r);}
}
LRESULT CALLBACK procedure(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp) {
  switch(msg) {
    case WM_CREATE:{
      window=hwnd;DragAcceptFiles(hwnd,TRUE);
      for(int i=0;i<10;++i){wchar_t value[32768]{};DWORD size=sizeof(value);auto name=std::to_wstring(i);if(RegGetValueW(HKEY_CURRENT_USER,L"Software\\EoriniZip\\Recent",name.c_str(),RRF_RT_REG_SZ,nullptr,value,&size)==ERROR_SUCCESS)recentFiles.emplace_back(value);}
      loadPreferences();treeVisible=preferences.showTree;statusVisible=preferences.showStatus;HDC dc=GetDC(hwnd);dpi=GetDeviceCaps(dc,LOGPIXELSX);ReleaseDC(hwnd,dc);
      SetWindowTextW(hwnd,L"?¥Î¶∞?¥zip");
      font=CreateFontW(-px(14),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
      footerBrush=CreateSolidBrush(RGB(174,125,13));SetMenu(hwnd,menus());
      logo=control(L"STATIC",L"",SS_ICON);SendMessageW(logo,STM_SETICON,reinterpret_cast<WPARAM>(LoadImageW(GetModuleHandleW(nullptr),MAKEINTRESOURCEW(1),IMAGE_ICON,px(60),px(60),LR_SHARED)),0);
      homeOpen=control(L"BUTTON",L"?ïÏ∂ï ?åÏùº ?¥Í∏∞",BS_OWNERDRAW|WS_TABSTOP,Open);
      homeCreate=control(L"BUTTON",L"?àÎ°ú ?ïÏ∂ï?òÍ∏∞",BS_OWNERDRAW|WS_TABSTOP,CreateFiles);
      // Workspace IDs differ for duplicate home buttons; commands route to the same actions.
      const wchar_t* labels[]={L"?¥Í∏∞",L"???ïÏ∂ï",L"?¥Îçî ?ïÏ∂ï",L"?ïÏ∂ï ?ÄÍ∏?,L"Í≤Ä??,L"Ï∑®ÏÜå",L"?§Ïãú ?ΩÍ∏∞",L"Í∏∞Î≥∏ ?îÎ©¥"};
      const int ids[]={Open,CreateFiles,CreateFolder,Extract,Test,Cancel,Refresh,Home};
      for(int i=0;i<8;++i){auto b=control(L"BUTTON",labels[i],BS_OWNERDRAW|WS_TABSTOP,ids[i]+1000);workspace.push_back(b);controls.push_back(b);}
      pathText=control(L"STATIC",L"",SS_PATHELLIPSIS);
      passwordLabel=control(L"STATIC",L"ÎπÑÎ?Î≤àÌò∏",0);
      password=control(L"EDIT",L"",WS_BORDER|ES_PASSWORD|ES_AUTOHSCROLL|WS_TABSTOP);
      SendMessageW(password,EM_SETCUEBANNER,TRUE,reinterpret_cast<LPARAM>(L"?ÖÎ†• ???§Ïãú ?ΩÍ∏∞"));
      tree=control(WC_TREEVIEWW,L"",TVS_HASBUTTONS|TVS_HASLINES|TVS_LINESATROOT|TVS_SHOWSELALWAYS|WS_BORDER|WS_TABSTOP);
      list=control(WC_LISTVIEWW,L"",LVS_REPORT|LVS_SHOWSELALWAYS|LVS_SHAREIMAGELISTS|WS_BORDER|WS_TABSTOP);
      entryImages=ImageList_Create(px(16),px(16),ILC_COLOR32|ILC_MASK,2,0);
      if(entryImages){
        const SHSTOCKICONID stockIcons[]={SIID_FOLDER,SIID_DOCNOASSOC};int* indexes[]={&folderIcon,&fileIcon};
        for(int i=0;i<2;++i){SHSTOCKICONINFO info{};info.cbSize=sizeof(info);
          if(SUCCEEDED(SHGetStockIconInfo(stockIcons[i],SHGSI_ICON|SHGSI_SMALLICON,&info))){*indexes[i]=ImageList_AddIcon(entryImages,info.hIcon);DestroyIcon(info.hIcon);}
        }
        ListView_SetImageList(list,entryImages,LVSIL_SMALL);TreeView_SetImageList(tree,entryImages,TVSIL_NORMAL);
      }
      applyModernTheme(list);applyModernTheme(tree);
      ListView_SetExtendedListViewStyle(list,LVS_EX_DOUBLEBUFFER|(preferences.fullRow?LVS_EX_FULLROWSELECT:0)|(preferences.grid?LVS_EX_GRIDLINES:0));
      const wchar_t* columns[]={L"?¥Î¶Ñ",L"?êÎ≥∏ ?¨Í∏∞ (B)",L"?ïÏ∂ï ?¨Í∏∞ (B)",L"?òÏ†ï ?†Ïßú",L"?îÌò∏"};int widths[]={280,120,120,170,60};
      for(int i=0;i<5;++i){auto label=kz::ui::label(columns[i]);LVCOLUMNW c{};c.mask=LVCF_TEXT|LVCF_WIDTH;c.pszText=label.data();c.cx=px(widths[i]);ListView_InsertColumn(list,i,&c);}
      workspace.insert(workspace.end(),{pathText,passwordLabel,password,tree,list});controls.insert(controls.end(),{homeOpen,homeCreate,password});
      statusText=control(L"STATIC",L"?¥Î¶∞?¥zip ¬∑ ?§Ï†ï Î©îÎâ¥?êÏÑú ?ïÏ∂ï ?îÏßÑ???∞Í≤∞?òÏÑ∏??",SS_LEFT|SS_PATHELLIPSIS);
      applyViewPreferences();
      wchar_t executablePath[32768];GetModuleFileNameW(nullptr,executablePath,32768);auto appDir=kz::fs::path(executablePath).parent_path();
      for(const auto& candidate:{appDir/L"7zip/7z.exe",appDir.parent_path()/L"vendor/7zip/7z.exe",kz::fs::path(L"C:/Program Files/7-Zip/7z.exe")})
        if(kz::fs::exists(candidate)){engine=std::make_unique<kz::Engine>(candidate);SetWindowTextW(statusText,L"?¥Î¶∞?¥zip ¬∑ ?ïÏ∂ï?åÏùº???¥Í±∞???àÎ°ú ?ïÏ∂ï??Î≥¥ÏÑ∏??");break;}
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
          if(easyCryptFile(input)&&(mode==L"--open"||mode==L"--extract-here"||mode==L"--extract-folder"))decryptFile(input);
          else if(mode==L"--compress")createArchive(inputs);
          else if(mode==L"--compress-each"){start([inputs]{auto results=engine->createEach(inputs);return Result{{},false,std::to_string(results.size())+"Í∞?Í∞úÎ≥Ñ ?ïÏ∂ï ?ÑÎ£å"};},1);}
          else if(mode==L"--extract-each"){batchExtractTask(inputs);}
          else if(mode==L"--compress-quick"){
            start([inputs] {return Result{{},false,"?ïÏ∂ï ?ÑÎ£å: "+engine->createQuick(inputs).u8string()};},1);
          }
          else if(mode==L"--open"||mode==L"--extract-folder"||mode==L"--extract-here"||mode==L"--extract-smart"){
            archive=input;SetWindowTextW(pathText,archive.c_str());showHome(false);
            if(mode==L"--open")reload();
            else archiveTask(input,mode==L"--extract-here"?1:mode==L"--extract-smart"?2:3);


          }
        }catch(const std::exception& e){SetWindowTextW(statusText,kz::widen(e.what()).c_str());}
      }
      if(preferences.checkUpdates&&argc<2)try{launchUpdateCheck(true);}catch(...){}
      if(argv)LocalFree(argv);return 0;
    }
    case WM_DROPFILES:{
      auto drop=reinterpret_cast<HDROP>(wp);std::vector<kz::fs::path> files;
      for(UINT i=0,count=DragQueryFileW(drop,0xffffffff,nullptr,0);i<count;++i){auto length=DragQueryFileW(drop,i,nullptr,0);std::wstring path(length+1,L'\0');DragQueryFileW(drop,i,path.data(),length+1);path.resize(length);files.emplace_back(path);}
      DragFinish(drop);if(busy||files.empty())return 0;
      try{auto ext=files[0].extension().wstring();std::transform(ext.begin(),ext.end(),ext.begin(),::towlower);
        const auto& supported=kz::win::extensions();
        if(files.size()==1&&std::find(supported.begin(),supported.end(),ext)!=supported.end()){
          if(easyCryptFile(files[0]))decryptFile(files[0]);else{archive=files[0];SetWindowTextW(password,L"");SetWindowTextW(pathText,archive.c_str());reload();}
        }else createArchive(files);
      }catch(const std::exception& e){MessageBoxW(window,kz::widen(e.what()).c_str(),L"?¥Î¶∞?¥zip",MB_OK|MB_ICONERROR);}return 0;
    }
    case WM_NOTIFY:{
      auto header=reinterpret_cast<NMHDR*>(lp);
      if(header->hwndFrom==list&&header->code==LVN_COLUMNCLICK){auto column=reinterpret_cast<NMLISTVIEW*>(lp)->iSubItem;if(column==sortColumn)sortAscending=!sortAscending;else{sortColumn=column;sortAscending=true;}displayFolder(currentFolder);return 0;}
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
      if((HIWORD(wp)==0||HIWORD(wp)==1)&&((id>=Open&&id<=LastCommand)||(id>=RecentFirst&&id<=RecentClear)))command(id);return 0;
    }
    case Finished:{
      std::unique_ptr<Result> result(reinterpret_cast<Result*>(lp));if(worker.joinable())worker.join();busy=false;
      for(auto c:controls)EnableWindow(c,TRUE);if(result->listing){rememberArchive();showHome(false);display(result->entries);}
      if(!result->recoveredPassword.empty()){
        if(progressWindow){KillTimer(progressWindow,1);DestroyWindow(progressWindow);progressWindow=nullptr;}EnableWindow(window,TRUE);
        SetWindowTextW(password,kz::widen(result->recoveredPassword).c_str());SecureZeroMemory(result->recoveredPassword.data(),result->recoveredPassword.size());reload();return 0;
      }
      if(!result->clipboardRoot.empty()||!result->editFile.empty()){
        if(progressWindow){KillTimer(progressWindow,1);DestroyWindow(progressWindow);progressWindow=nullptr;}EnableWindow(window,TRUE);
        try{
          if(!result->clipboardRoot.empty()){
            std::vector<kz::fs::path> files;for(const auto& entry:kz::fs::directory_iterator(result->clipboardRoot))files.push_back(entry.path());
            kz::win::copyFilesClipboard(window,files);clipboardRoots.push_back(result->clipboardRoot);
            SetWindowTextW(statusText,kz::widen(result->message).c_str());return 0;
          }
          wchar_t system[MAX_PATH];GetSystemDirectoryW(system,MAX_PATH);auto editor=kz::fs::path(system)/L"notepad.exe";
          auto arguments=kz::quoteWindows(result->editFile.wstring());
          if(reinterpret_cast<INT_PTR>(ShellExecuteW(window,L"open",editor.c_str(),arguments.c_str(),nullptr,SW_SHOWNORMAL))<=32)throw std::runtime_error("Î©îÎ™®?•ÏùÑ ?úÏûë?òÏ? Î™ªÌñà?µÎãà??");
          if(MessageBoxW(window,L"Î©îÎ™®?•Ïóê???∏Ïßë?òÍ≥† ?Ä?•Ìïú ???åÏùº???´Ïúº?∏Ïöî.\n\n?Ä?•Îêú Î≥ÄÍ≤??¥Ïö©???ïÏ∂ï ?åÏùº??Î∞òÏòÅ?†Íπå?? ?ÑÎãà?îÎ? ?ÑÎ•¥Î©??êÎ≥∏???†Ï??©Îãà??",L"?∏Î? ?∏Ïßë Î∞òÏòÅ",MB_YESNO|MB_ICONQUESTION)==IDYES){
            auto data=std::make_shared<Result>(std::move(*result));
            start([data]{
              try{
                if(kz::fs::file_size(data->editArchive)!=data->editSize||kz::fs::last_write_time(data->editArchive)!=data->editStamp)throw std::runtime_error("?∏Ïßë Ï§??ïÏ∂ï ?åÏùº??Î∞îÎÄåÏóà?µÎãà?? ?êÎ≥∏???§Ïãú ?¥Ïñ¥ Ï£ºÏÑ∏??");
                engine->edit(data->editArchive,"replace",{data->editName,data->editFile.u8string()},data->editPassword);
                Result done{engine->list(data->editArchive,data->editPassword),true,"?∏Î? ?∏Ïßë ?¥Ïö© Î∞òÏòÅ Î∞?Î¨¥Í≤∞??Í≤Ä???ÑÎ£å"};std::error_code ec;kz::fs::remove_all(data->editRoot,ec);return done;
              }catch(...){std::error_code ec;kz::fs::remove_all(data->editRoot,ec);throw;}
            },1);return 0;
          }
          std::error_code ec;kz::fs::remove_all(result->editRoot,ec);SetWindowTextW(statusText,L"?∏Î? ?∏Ïßë Î∞òÏòÅ??Ï∑®ÏÜå?àÏäµ?àÎã§.");
        }catch(const std::exception& e){std::error_code ec;if(!result->clipboardRoot.empty())kz::fs::remove_all(result->clipboardRoot,ec);if(!result->editRoot.empty())kz::fs::remove_all(result->editRoot,ec);MessageBoxW(window,kz::widen(e.what()).c_str(),L"?¥Î¶∞?¥zip",MB_OK|MB_ICONERROR);}
        return 0;
      }
      if(!result->previewFile.empty()){
        if(progressWindow){KillTimer(progressWindow,1);DestroyWindow(progressWindow);progressWindow=nullptr;}EnableWindow(window,TRUE);
        kz::win::showPreview(window,result->previewFile);std::error_code ec;kz::fs::remove_all(result->previewRoot,ec);SetWindowTextW(statusText,L"ÎØ∏Î¶¨Î≥¥Í∏∞ ?ÑÎ£å");return 0;
      }
      if(result->passwordNeeded&&passwordRetry){
        if(progressWindow){KillTimer(progressWindow,1);DestroyWindow(progressWindow);progressWindow=nullptr;}
        EnableWindow(window,TRUE);SetActiveWindow(window);
        auto retry=std::move(passwordRetry);std::wstring secret;
        if(askArchivePassword(secret)){
          auto value=kz::narrow(secret);SecureZeroMemory(secret.data(),secret.size()*sizeof(wchar_t));secret.clear();retry(std::move(value));
        }else SetWindowTextW(statusText,L"?îÌò∏ ?ÖÎ†•??Ï∑®ÏÜå?àÏäµ?àÎã§.");
        return 0;
      }
      SetWindowTextW(statusText,kz::widen(result->message).c_str());
      if(progressWindow){
        KillTimer(progressWindow,1);SetWindowTextW(progressWindow,result->success?L"?ëÏóÖ ?ÑÎ£å - ?¥Î¶∞?¥zip":L"?ëÏóÖ Ï§ëÎã® - ?¥Î¶∞?¥zip");
        SetDlgItemTextW(progressWindow,IDC_PROGRESS_STATUS,kz::widen(result->message).c_str());
        SendDlgItemMessageW(progressWindow,IDC_PROGRESS_BAR,PBM_SETMARQUEE,FALSE,0);
        auto bar=GetDlgItem(progressWindow,IDC_PROGRESS_BAR);
        SetWindowLongPtrW(bar,GWL_STYLE,GetWindowLongPtrW(bar,GWL_STYLE)&~PBS_MARQUEE);
        if(result->success)SendMessageW(bar,PBM_SETPOS,100,0);
        else SendMessageW(bar,PBM_SETSTATE,PBST_ERROR,0);
        auto elapsed=std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now()-jobStarted).count();
        SetWindowTextW(progressWindow,((result->success?L"?ÑÎ£å":L"Ï§ëÎã®")+std::wstring(L" ¬∑ ?åÏöî ")+durationText(elapsed)+L" - ?¥Î¶∞?¥zip").c_str());
        SetDlgItemTextW(progressWindow,IDCANCEL,L"?´Í∏∞");EnableWindow(GetDlgItem(progressWindow,IDCANCEL),TRUE);
        if(result->success&&(result->listing||(jobKind==1&&preferences.closeAfterCreate)||(jobKind==2&&preferences.closeAfterExtract)))SendMessageW(progressWindow,WM_CLOSE,0,0);
      }return 0;
    }
    case WM_GETMINMAXINFO:{auto info=reinterpret_cast<MINMAXINFO*>(lp);info->ptMinTrackSize={px(730),px(460)};return 0;}
    case WM_CLOSE:if(busy){MessageBoxW(hwnd,L"?ëÏóÖ??Ï∑®ÏÜå????Ï∞ΩÏùÑ ?´ÏïÑ Ï£ºÏÑ∏??",L"?¥Î¶∞?¥zip",MB_OK);return 0;}DestroyWindow(hwnd);return 0;
    case WM_DESTROY:if(worker.joinable())worker.join();if(GetClipboardOwner()==window&&OpenClipboard(window)){EmptyClipboard();CloseClipboard();}for(const auto& root:clipboardRoots){std::error_code ec;kz::fs::remove_all(root,ec);}if(entryImages){ListView_SetImageList(list,nullptr,LVSIL_SMALL);TreeView_SetImageList(tree,nullptr,TVSIL_NORMAL);ImageList_Destroy(entryImages);entryImages=nullptr;}DeleteObject(font);DeleteObject(footerBrush);if(bgBrush)DeleteObject(bgBrush);if(titleFont)DeleteObject(titleFont);if(subFont)DeleteObject(subFont);PostQuitMessage(0);return 0;
  }
  return DefWindowProcW(hwnd,msg,wp,lp);
}
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR,int show) {
  Gdiplus::GdiplusStartupInput graphicsInput;ULONG_PTR graphicsToken=0;if(Gdiplus::GdiplusStartup(&graphicsToken,&graphicsInput,nullptr)!=Gdiplus::Ok)return 1;
  CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);INITCOMMONCONTROLSEX cc{sizeof(cc),ICC_LISTVIEW_CLASSES|ICC_PROGRESS_CLASS|ICC_TREEVIEW_CLASSES};InitCommonControlsEx(&cc);
  WNDCLASSW cls{};cls.lpfnWndProc=procedure;cls.hInstance=instance;cls.lpszClassName=L"EoriniZipNative";
  cls.hCursor=LoadCursorW(nullptr,IDC_ARROW);cls.hIcon=LoadIconW(instance,MAKEINTRESOURCEW(1));RegisterClassW(&cls);
  HWND main=CreateWindowW(cls.lpszClassName,L"?¥Î¶∞?¥zip",WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN,CW_USEDEFAULT,CW_USEDEFAULT,840,620,nullptr,nullptr,instance,nullptr);
  ShowWindow(main,show);UpdateWindow(main);PostMessageW(main,Startup,0,0);
  ACCEL keys[]={{FVIRTKEY|FCONTROL,'I',ScanArchive},{FVIRTKEY|FCONTROL|FSHIFT,'M',EditComment},{FVIRTKEY,VK_F8,EditComment},{FVIRTKEY|FCONTROL,'C',CopyFiles},{FVIRTKEY|FCONTROL,'V',PasteFiles},{FVIRTKEY,VK_F2,RenameEntry},{FVIRTKEY,VK_INSERT,AddEntries},{FVIRTKEY,VK_DELETE,DeleteEntries},{FVIRTKEY|FCONTROL,'O',Open},{FVIRTKEY|FCONTROL,'N',CreateFiles},{FVIRTKEY|FCONTROL,'A',SelectAll},{FVIRTKEY|FCONTROL,'F',FindFile},{FVIRTKEY|FCONTROL,'T',Test},{FVIRTKEY|FCONTROL,'E',Extract},{FVIRTKEY|FCONTROL,'W',Exit},{FVIRTKEY|FCONTROL|FSHIFT,'N',NewWindow},{FVIRTKEY,VK_F4,CloseArchive},{FVIRTKEY,VK_F6,Home},{FVIRTKEY,VK_F1,About},{FVIRTKEY,VK_F5,Preferences},{FVIRTKEY,VK_F7,ToggleTree}};
  HACCEL accelerators=CreateAcceleratorTableW(keys,static_cast<int>(std::size(keys)));
  MSG msg;while(GetMessageW(&msg,nullptr,0,0)>0){
    // Preserve ordinary editing shortcuts in the password field.
    if(progressWindow&&IsDialogMessageW(progressWindow,&msg))continue;
    bool editing=GetFocus()==password;
    if(msg.message==WM_KEYDOWN&&msg.wParam==VK_ESCAPE&&preferences.escape&&!editing){SendMessageW(main,WM_CLOSE,0,0);continue;}
    if((editing||!TranslateAcceleratorW(main,accelerators,&msg))&&!IsDialogMessageW(main,&msg)){TranslateMessage(&msg);DispatchMessageW(&msg);}
  }
  DestroyAcceleratorTable(accelerators);CoUninitialize();Gdiplus::GdiplusShutdown(graphicsToken);return 0;
}
