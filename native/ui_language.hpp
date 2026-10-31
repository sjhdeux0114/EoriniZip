#pragma once
#include <string>
#include <map>
#ifdef _WIN32
#include <windows.h>
#endif
namespace kz::ui {
inline bool english=false;
inline std::wstring label(const std::wstring& text){
  if(!english)return text;
  static const std::map<std::wstring,std::wstring> words={
    {L"어린이zip",L"EoriniZip"},{L"파일(&F)",L"File (&F)"},{L"편집(&E)",L"Edit (&E)"},{L"찾기(&I)",L"Find (&I)"},{L"설정(&S)",L"Settings (&S)"},{L"보기(&V)",L"View (&V)"},{L"도구(&T)",L"Tools (&T)"},{L"도움말(&A)",L"Help (&A)"},
    {L"열기",L"Open"},{L"새 압축",L"Create"},{L"폴더 압축",L"Folder"},{L"압축 풀기",L"Extract"},{L"검사",L"Test"},{L"취소",L"Cancel"},{L"다시 읽기",L"Refresh"},{L"기본 화면",L"Home"},{L"압축 파일 열기",L"Open archive"},{L"새로 압축하기",L"Create archive"},{L"새로 압축",L"Create archive"},
    {L"최근 파일",L"Recent archives"},{L"압축 파일 닫기",L"Close archive"},{L"다른 이름으로 저장...",L"Save a copy..."},{L"압축 파일 삭제",L"Delete archive"},{L"압축 파일 완전 삭제",L"Permanently delete archive"},{L"압축 파일이 있는 폴더 열기",L"Open containing folder"},{L"압축 파일 테스트",L"Test archive"},{L"선택한 항목 풀기...",L"Extract selected..."},{L"알아서 풀기...",L"Smart extract..."},
    {L"Microsoft Defender로 압축 파일 검사",L"Scan archive with Microsoft Defender"},{L"새창 띄우기",L"New window"},{L"새창 띄우기(관리자 권한)",L"New window as administrator"},{L"종료",L"Exit"},{L"이름 바꾸기",L"Rename"},{L"파일 추가",L"Add files"},{L"파일 삭제",L"Delete entries"},{L"ZIP 압축 설명 편집",L"Edit ZIP comment"},{L"클립보드로 파일 복사하기",L"Copy files to clipboard"},{L"클립보드에서 파일 붙여 넣기",L"Paste files from clipboard"},{L"텍스트 파일 외부 편집 후 반영...",L"Edit text in Notepad and update archive..."},
    {L"파일의 경로명을 클립보드로 복사",L"Copy entry names"},{L"모두 선택",L"Select all"},{L"파일 찾기",L"Find file"},{L"환경 설정",L"Preferences"},{L"압축 엔진 연결...",L"Connect archive engine..."},{L"현재 파일 암호 저장...",L"Save password for this archive..."},{L"저장한 암호 불러오기",L"Load saved password"},{L"현재 파일 저장 암호 삭제...",L"Delete saved password for this archive..."},
    {L"시작 화면",L"Home screen"},{L"트리 컨트롤",L"Folder tree"},{L"ZIP 압축 설명",L"ZIP comment"},{L"선택 파일 미리보기",L"Preview selected file"},{L"상태 표시줄",L"Status bar"},{L"툴바",L"Toolbar"},{L"파일 목록 글꼴/칼럼 설정...",L"List font and columns..."},{L"항상 위에 표시",L"Always on top"},{L"색상 설정...",L"Theme color..."},{L"압축 파일 다시 읽기",L"Reload archive"},
    {L"ZIP 중앙 디렉터리 복구 (비암호화 store/deflate)...",L"Rebuild ZIP directory (unencrypted store/deflate)..."},{L"암호 후보 목록으로 복구 (UTF-8, 한 줄에 하나)...",L"Recover using password candidates (UTF-8, one per line)..."},{L"EasyCrypt 2.4 파일 해제...",L"Decrypt EasyCrypt 2.4 file..."},{L"작업 취소",L"Cancel operation"},{L"정보",L"About"},{L"프로그램 설치 폴더 열기",L"Open installation folder"},{L"도움말 페이지 열기",L"Open help page"},{L"업데이트 확인...",L"Check for updates..."},{L"문의 / 오류 신고",L"Report an issue"},{L"홈페이지 열기",L"Open project website"},
    {L"일반 설정",L"General"},{L"파일 연결",L"File associations"},{L"탐색기 메뉴",L"Explorer menus"},{L"압축하기",L"Compression"},{L"보기/편집",L"View / edit"},{L"보안 설정",L"Security"},{L"고급 설정",L"Advanced"},{L"언어 설정",L"Language"},
    {L"새로 압축하기 - 어린이zip",L"Create archive - EoriniZip"},{L"환경 설정 - 어린이zip",L"Preferences - EoriniZip"},{L"비밀번호 입력 - 어린이zip",L"Password - EoriniZip"},{L"이름 바꾸기 - 어린이zip",L"Rename - EoriniZip"},{L"ZIP 압축 설명 - 어린이zip",L"ZIP comment - EoriniZip"},{L"작업 준비 중 - 어린이zip",L"Preparing - EoriniZip"},
    {L"압축할 파일 / 폴더",L"Files / folders to archive"},{L"파일 추가...",L"Add files..."},{L"폴더 추가...",L"Add folder..."},{L"목록 비우기",L"Clear list"},{L"파일 이름",L"File name"},{L"압축 형식",L"Format"},{L"분할 압축",L"Split volumes"},{L"압축 방법",L"Compression level"},{L"비밀번호",L"Password"},{L"파일명 암호화 (7Z)",L"Encrypt names (7Z)"},{L"압축 후 테스트",L"Test after creating"},{L"압축 시작",L"Create"},
    {L"지정한 위치에 저장됩니다. 같은 이름은 번호를 붙입니다.",L"Save to the chosen location; existing names get a number."},{L"파일 이름에 포함된 글자",L"Part of the file name"},{L"다음 찾기",L"Find next"},{L"닫기",L"Close"},{L"초기화",L"Reset"},{L"확인",L"OK"},{L"비밀번호를 입력하세요.",L"Enter the password."},{L"취소하면 파일을 변경하지 않고 작업을 중단합니다.",L"Cancel stops the operation without changing the archive."},{L"해제",L"Extract"},{L"새 이름",L"New name"},{L"저장",L"Save"},{L"설명은 암호화되지 않습니다. UTF-8로 저장합니다.",L"Comments are not encrypted. They are saved as UTF-8."},
    {L"ESC 키로 프로그램 종료",L"Exit with Escape"},{L"파일 목록에 눈금선 보이기",L"Show grid lines"},{L"파일 목록 창에 전체 행 선택 기능 사용",L"Select full rows"},{L"프로그램 시작 시 시작 화면 보여주기",L"Show home screen on startup"},{L"압축 후 압축 파일 테스트",L"Test new archives"},{L"ZIP 파일 이름을 UTF-8로 저장",L"Store ZIP names as UTF-8"},{L"만든 날짜와 접근 날짜 저장 (ZIP / 7Z)",L"Store creation and access times (ZIP / 7Z)"},{L"압축 완료 후 작업 창 자동으로 닫기",L"Close progress after creating"},{L"압축 해제 완료 후 작업 창 자동으로 닫기",L"Close progress after extracting"},{L"Thumbs.db 제외",L"Exclude Thumbs.db"},{L"엔진 멀티코어 처리 사용",L"Allow engine multithreading"},{L"다운로드 출처 정보를 해제한 파일에도 보존",L"Preserve downloaded-file origin information"},
    {L"환경 설정 내보내기...",L"Export preferences..."},{L"환경 설정 가져오기...",L"Import preferences..."},{L"연결 프로그램 등록",L"Register associations"},{L"Windows 기본 앱 설정",L"Windows default apps"},{L"전체 선택",L"Select all"},{L"전체 해제",L"Clear selection"},{L"탐색기 메뉴 등록",L"Register classic menus"},{L"탐색기 메뉴 제거",L"Remove classic menus"},{L"폴더 트리 표시",L"Show folder tree"},{L"상태 표시줄 표시",L"Show status bar"},{L"테마 색상 선택...",L"Theme color..."},{L"칼럼 표시 선택...",L"Visible columns..."},{L"임시 폴더 선택...",L"Choose temporary folder..."},{L"OS 임시 폴더 사용",L"Use OS temporary folder"},{L"시작할 때 업데이트 확인 (GitHub 접속)",L"Check for updates on startup (connects to GitHub)"},
    {L"압축하지 않음",L"Store"},{L"빠르게",L"Fast"},{L"보통",L"Normal"},{L"최대로",L"Maximum"},{L"ZIP 코드페이지: 자동",L"ZIP code page: automatic"},{L"한국어 (949)",L"Korean (949)"},{L"일본어 (932)",L"Japanese (932)"},{L"중국어 간체 (936)",L"Simplified Chinese (936)"},{L"중국어 번체 (950)",L"Traditional Chinese (950)"},{L"서유럽어 (1252)",L"Western European (1252)"},{L"이름",L"Name"},{L"원본 크기 (B)",L"Size (B)"},{L"압축 크기 (B)",L"Packed (B)"},{L"원본 크기",L"Original size"},{L"압축 크기",L"Packed size"},{L"수정 날짜",L"Modified"},{L"암호",L"Encrypted"},
    {L"언어 설정은 다음 실행부터 적용됩니다.",L"The language setting takes effect after restarting."},
    {L"GZIP (파일 하나)",L"GZIP (one file)"},{L"BZIP2 (파일 하나)",L"BZIP2 (one file)"},{L"XZ (파일 하나)",L"XZ (one file)"},{L"SFX (7Z 자체 해제 EXE)",L"SFX (7Z self-extracting EXE)"},{L"WIM (비압축)",L"WIM (uncompressed)"},{L"ISO (폴더 내용)",L"ISO (folder contents)"},
    {L"글꼴 크기: 12",L"Font size: 12"},{L"글꼴 크기: 14",L"Font size: 14"},{L"글꼴 크기: 16",L"Font size: 16"},{L"글꼴 크기: 18",L"Font size: 18"},{L"글꼴 크기: 20",L"Font size: 20"},{L"맑은 고딕",L"Malgun Gothic"},{L"굴림",L"Gulim"},
    {L"입력 후 다시 읽기",L"Enter password, then reload"},{L"일치하는 파일이 없습니다.",L"No matching file."},{L"최근 파일 목록 지우기",L"Clear recent files"}
  };
  auto tab=text.find(L'\t');auto key=text.substr(0,tab);auto found=words.find(key);
  return found==words.end()?text:found->second+(tab==std::wstring::npos?L"":text.substr(tab));
}
#ifdef _WIN32
inline bool editable(HWND window){wchar_t type[32]{};GetClassNameW(window,type,32);return _wcsicmp(type,L"Edit")==0;}
inline void dialog(HWND window){
  if(!english)return;
  wchar_t caption[1024]{};GetWindowTextW(window,caption,1024);auto title=label(caption);SetWindowTextW(window,title.c_str());
  EnumChildWindows(window,[](HWND child,LPARAM)->BOOL{wchar_t type[32]{};GetClassNameW(child,type,32);if(_wcsicmp(type,L"Button")&&_wcsicmp(type,L"Static"))return TRUE;wchar_t text[4096]{};GetWindowTextW(child,text,4096);auto value=label(text);SetWindowTextW(child,value.c_str());return TRUE;},0);
}
#endif
}
