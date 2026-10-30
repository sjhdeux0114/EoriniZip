#include "core.hpp"
#include <windows.h>
#include <commctrl.h>
#include <shobjidl.h>
#include <memory>
#include <thread>
#include <functional>
#include <stdexcept>
#include <algorithm>
#pragma comment(lib, "comctl32.lib")
namespace {
HWND window, list, statusText, pathText, password, formatBox, splitBox, hideNames;
HFONT font;
std::vector<HWND> controls;
std::unique_ptr<kz::Engine> engine;
std::thread worker;
kz::fs::path archive;
bool busy = false;
enum { Open=100, CreateFiles, CreateFolder, Extract, Test, Connect, Cancel, Refresh };
constexpr UINT Finished = WM_APP + 1;
constexpr UINT Startup = WM_APP + 2;
struct Result { std::vector<kz::Entry> entries; bool listing = false; std::string message; };
std::wstring text(HWND control) {
  int n = GetWindowTextLengthW(control); std::wstring value(n + 1, L'\0');
  GetWindowTextW(control, value.data(), n + 1); value.resize(n); return value;
}
HWND control(const wchar_t* cls, const wchar_t* title, DWORD style, int x, int y, int w, int h, int id = 0) {
  HWND item = CreateWindowExW(0, cls, title, WS_CHILD | WS_VISIBLE | style, x,y,w,h,window,reinterpret_cast<HMENU>(static_cast<intptr_t>(id)),nullptr,nullptr);
  SendMessageW(item, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE); return item;
}
std::vector<kz::fs::path> choose(bool folder, bool multiple = false, bool save = false, const std::wstring& name = L"") {
  IFileDialog* dialog = nullptr;
  HRESULT hr = CoCreateInstance(save ? CLSID_FileSaveDialog : CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog));
  if (FAILED(hr)) throw std::runtime_error("Cannot open file dialog.");
  DWORD flags; dialog->GetOptions(&flags);
  flags |= FOS_FORCEFILESYSTEM | FOS_NOCHANGEDIR;
  if (folder) flags |= FOS_PICKFOLDERS;
  if (multiple) flags |= FOS_ALLOWMULTISELECT;
  dialog->SetOptions(flags);
  if (!name.empty()) dialog->SetFileName(name.c_str());
  std::vector<kz::fs::path> results;
  if (SUCCEEDED(dialog->Show(window))) {
    auto add = [&](IShellItem* item) {
      PWSTR p = nullptr; if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &p))) { results.emplace_back(p); CoTaskMemFree(p); }
    };
    if (multiple) {
      IFileOpenDialog* open = nullptr;
      if (SUCCEEDED(dialog->QueryInterface(IID_PPV_ARGS(&open)))) {
        IShellItemArray* items = nullptr;
        if (SUCCEEDED(open->GetResults(&items))) {
          DWORD count; items->GetCount(&count);
          for (DWORD i=0; i<count; ++i) { IShellItem* item = nullptr; if (SUCCEEDED(items->GetItemAt(i,&item))) { add(item); item->Release(); } }
          items->Release();
        } open->Release();
      }
    } else {
      IShellItem* item = nullptr; if (SUCCEEDED(dialog->GetResult(&item))) { add(item); item->Release(); }
    }
  }
  dialog->Release(); return results;
}
void display(const std::vector<kz::Entry>& entries) {
  SendMessageW(list, WM_SETREDRAW, FALSE, 0); ListView_DeleteAllItems(list);
  for (size_t i=0; i<entries.size(); ++i) {
    const auto& e = entries[i];
    std::wstring values[] = {kz::widen((e.directory ? "[폴더] " : "") + e.name), std::to_wstring(e.size), std::to_wstring(e.packed), kz::widen(e.modified), e.encrypted ? L"암호" : L""};
    LVITEMW item{}; item.mask = LVIF_TEXT; item.iItem = static_cast<int>(i); item.pszText = values[0].data(); ListView_InsertItem(list,&item);
    for(int j=1;j<5;++j) ListView_SetItemText(list,static_cast<int>(i),j,values[j].data());
  }
  SendMessageW(list, WM_SETREDRAW, TRUE, 0); InvalidateRect(list,nullptr,TRUE);
}
void start(std::function<Result()> task) {
  if (busy) return;
  if (!engine) throw std::runtime_error("먼저 '엔진 연결'에서 공식 7z.exe를 선택하세요.");
  if (worker.joinable()) worker.join();
  busy = true; engine->resetCancel();
  for (auto c : controls) EnableWindow(c,FALSE);
  EnableWindow(GetDlgItem(window,Cancel),TRUE);
  SetWindowTextW(statusText,L"작업 중…");
  worker = std::thread([task=std::move(task)] {
    auto result = std::make_unique<Result>();
    try { *result = task(); } catch (const std::exception& e) { result->message = e.what(); }
    PostMessageW(window, Finished, 0, reinterpret_cast<LPARAM>(result.release()));
  });
}
void reload() {
  if (archive.empty()) return;
  const auto p = kz::narrow(text(password)); const auto input = archive;
  start([=] { auto entries = engine->list(input,p); return Result{std::move(entries),true,"목록 읽기 완료"}; });
}
void command(int id) {
  if (id == Cancel) { if(engine) engine->cancel(); return; }
  if (busy) return;
  try {
    if (id == Connect) {
      auto files = choose(false); if(files.empty()) return;
      engine = std::make_unique<kz::Engine>(files[0]); SetWindowTextW(statusText,L"7-Zip 엔진 연결됨"); return;
    }
    if (id == Open) {
      auto files = choose(false); if(files.empty()) return;
      archive = files[0]; ListView_DeleteAllItems(list); SetWindowTextW(pathText,archive.c_str()); reload(); return;
    }
    if (id == Refresh) { reload(); return; }
    if (id == CreateFiles || id == CreateFolder) {
      if (!engine) throw std::runtime_error("먼저 7-Zip 엔진을 연결하세요.");
      auto inputs = choose(id == CreateFolder, id == CreateFiles); if(inputs.empty()) return;
      kz::Options options; options.format = kz::narrow(text(formatBox)); options.password = kz::narrow(text(password));
      options.split = kz::narrow(text(splitBox)); options.hideNames = SendMessageW(hideNames,BM_GETCHECK,0,0) == BST_CHECKED;
      auto output = choose(false,false,true,L"Archive." + kz::widen(options.format)); if(output.empty()) return;
      SetWindowTextW(password,L"");
      start([=] { return Result{{},false,"압축 완료: " + engine->create(options,output[0],inputs).u8string()}; }); return;
    }
    if (archive.empty()) throw std::runtime_error("압축파일을 먼저 여세요.");
    const auto p = kz::narrow(text(password)); const auto input = archive;
    if (id == Extract) {
      auto dirs = choose(true); if(dirs.empty()) return;
      start([=] { return Result{{},false,"해제 완료: " + engine->extract(input,dirs[0],p).u8string()}; });
    } else if (id == Test) start([=] { engine->test(input,p); return Result{{},false,"압축파일 검사 완료"}; });
  } catch (const std::exception& e) { MessageBoxW(window,kz::widen(e.what()).c_str(),L"어린이zip",MB_OK | MB_ICONINFORMATION); }
}
LRESULT CALLBACK procedure(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp) {
  switch(msg) {
    case WM_CREATE: {
      window = hwnd;
      font = CreateFontW(-16,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
      auto logo = control(L"STATIC",L"",SS_ICON,16,12,48,48);
      SendMessageW(logo,STM_SETICON,reinterpret_cast<WPARAM>(LoadImageW(GetModuleHandleW(nullptr),MAKEINTRESOURCEW(1),IMAGE_ICON,48,48,LR_SHARED)),0);
      control(L"STATIC",L"어린이zip   |   Native Preview 0.1",0,78,18,650,24);
      control(L"STATIC",L"ZIP · 7Z · TAR    /    RAR 읽기    /    ALZ · EGG · ZPAQ 개발 예정",0,78,44,800,24);
      const wchar_t* labels[] = {L"열기",L"파일 압축",L"폴더 압축",L"압축 풀기",L"검사",L"엔진 연결",L"취소",L"다시 읽기"};
      for (int i=0;i<8;++i) controls.push_back(control(L"BUTTON",labels[i],WS_TABSTOP,16+i*116,82,108,32,Open+i));
      control(L"STATIC",L"형식",0,16,131,40,24);
      formatBox = control(WC_COMBOBOXW,L"",CBS_DROPDOWNLIST | WS_TABSTOP,60,126,84,150);
      for(auto s : {L"zip",L"7z",L"tar"}) SendMessageW(formatBox,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(s));
      SendMessageW(formatBox,CB_SETCURSEL,0,0);
      control(L"STATIC",L"비밀번호",0,163,131,70,24);
      password = control(L"EDIT",L"",WS_BORDER | ES_PASSWORD | ES_AUTOHSCROLL | WS_TABSTOP,235,126,180,28);
      control(L"STATIC",L"분할",0,435,131,40,24);
      splitBox = control(L"EDIT",L"",WS_BORDER | ES_AUTOHSCROLL | WS_TABSTOP,480,126,110,28);
      SendMessageW(splitBox,EM_SETCUEBANNER,TRUE,reinterpret_cast<LPARAM>(L"예: 100m"));
      hideNames = control(L"BUTTON",L"파일명 암호화 (7Z)",BS_AUTOCHECKBOX | WS_TABSTOP,615,126,240,28);
      controls.insert(controls.end(),{formatBox,password,splitBox,hideNames});
      pathText = control(L"STATIC",L"압축파일을 열거나 파일 / 폴더를 압축하세요. 결과는 새 어린이zip 폴더에 저장됩니다.",SS_PATHELLIPSIS,16,173,940,24);
      list = control(WC_LISTVIEWW,L"",LVS_REPORT | LVS_SHOWSELALWAYS | WS_BORDER | WS_TABSTOP,16,206,940,380);
      ListView_SetExtendedListViewStyle(list,LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
      const wchar_t* columns[] = {L"이름",L"원본 크기 (B)",L"압축 크기 (B)",L"수정 날짜",L"암호"};
      int widths[] = {380,140,140,190,70};
      for(int i=0;i<5;++i) { LVCOLUMNW c{}; c.mask = LVCF_TEXT | LVCF_WIDTH; c.pszText = const_cast<wchar_t*>(columns[i]); c.cx=widths[i]; ListView_InsertColumn(list,i,&c); }
      statusText = control(L"STATIC",L"준비 · 엔진 연결 필요",SS_LEFT,16,600,940,40);
      wchar_t executablePath[32768]; GetModuleFileNameW(nullptr,executablePath,32768);
      const auto appDir=kz::fs::path(executablePath).parent_path();
      for(const auto& candidate : {appDir/L"7zip/7z.exe",appDir.parent_path()/L"vendor/7zip/7z.exe",kz::fs::path(L"C:/Program Files/7-Zip/7z.exe")}) {
        if(kz::fs::exists(candidate)) { engine=std::make_unique<kz::Engine>(candidate); SetWindowTextW(statusText,L"준비 · 7-Zip 연결됨"); break; }
      }
      return 0;
    }
    case WM_SIZE: {
      int width = LOWORD(lp), height = HIWORD(lp);
      MoveWindow(list,16,206,std::max(100,width-32),std::max(80,height-262),TRUE);
      MoveWindow(statusText,16,height-45,width-32,40,TRUE);
      MoveWindow(pathText,16,173,width-32,24,TRUE); return 0;
    }
    case Startup: {
      int argc=0; auto argv=CommandLineToArgvW(GetCommandLineW(),&argc);
      if(argv && argc==3) {
        std::wstring mode=argv[1]; kz::fs::path input=argv[2];
        try {
          if(mode==L"--compress") {
            kz::Options options;
            auto output=input.parent_path()/(input.filename().wstring()+L".zip");
            start([=]{return Result{{},false,"압축 완료: "+engine->create(options,output,{input}).u8string()};});
          } else if(mode==L"--open" || mode==L"--extract-folder") {
            archive=input; SetWindowTextW(pathText,archive.c_str());
            if(mode==L"--open") reload();
            else start([=]{return Result{{},false,"해제 완료: "+engine->extract(input,input.parent_path()).u8string()};});
          }
        } catch(const std::exception& e) { SetWindowTextW(statusText,kz::widen(e.what()).c_str()); }
      }
      if(argv) LocalFree(argv); return 0;
    }
    case WM_COMMAND: if(HIWORD(wp)==BN_CLICKED && LOWORD(wp)>=Open && LOWORD(wp)<=Refresh) command(LOWORD(wp)); return 0;
    case Finished: {
      std::unique_ptr<Result> result(reinterpret_cast<Result*>(lp));
      if(worker.joinable()) worker.join(); busy=false;
      for(auto c:controls) EnableWindow(c,TRUE);
      if(result->listing) display(result->entries);
      SetWindowTextW(statusText,kz::widen(result->message).c_str()); return 0;
    }
    case WM_GETMINMAXINFO: {
      auto info=reinterpret_cast<MINMAXINFO*>(lp); info->ptMinTrackSize={990,520}; return 0;
    }
    case WM_CLOSE:
      if(busy) { MessageBoxW(hwnd,L"작업을 취소한 뒤 창을 닫아 주세요.",L"어린이zip",MB_OK); return 0; }
      DestroyWindow(hwnd); return 0;
    case WM_DESTROY:
      if(worker.joinable()) worker.join(); DeleteObject(font); PostQuitMessage(0); return 0;
  }
  return DefWindowProcW(hwnd,msg,wp,lp);
}
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR,int show) {
  CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
  INITCOMMONCONTROLSEX cc{sizeof(cc),ICC_LISTVIEW_CLASSES}; InitCommonControlsEx(&cc);
  WNDCLASSW cls{}; cls.lpfnWndProc=procedure; cls.hInstance=instance; cls.lpszClassName=L"EoriniZipNative";
  cls.hCursor=LoadCursorW(nullptr,IDC_ARROW); cls.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_WINDOW+1);
  cls.hIcon=LoadIconW(instance,MAKEINTRESOURCEW(1)); RegisterClassW(&cls);
  HWND main=CreateWindowW(cls.lpszClassName,L"어린이zip",WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,1060,740,nullptr,nullptr,instance,nullptr);
  ShowWindow(main,show); UpdateWindow(main); PostMessageW(main,Startup,0,0);
  MSG msg; while(GetMessageW(&msg,nullptr,0,0)>0) { if(!IsDialogMessageW(main,&msg)) {TranslateMessage(&msg); DispatchMessageW(&msg);} }
  CoUninitialize(); return 0;
}
