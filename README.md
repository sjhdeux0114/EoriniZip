<p align="center"><img src="assets/logo.png" width="160" alt="유치원 모자를 쓴 어린이zip 캐릭터"></p>

# 어린이zip · EoriniZip

Windows와 macOS용 **무료 네이티브 압축 프로그램**을 개발하는 프로젝트입니다.
C++17 공통 코어, Windows Win32 UI, macOS AppKit UI를 사용합니다.
Electron, WebView, Node.js 런타임은 사용하지 않습니다.

**현재는 0.1 개발용 프로토타입입니다. 반디집·알집의 모든 기능을 구현한 제품이 아닙니다.**
반디집과 무관한 독립 프로젝트이며 해당 제품의 소스·아이콘·이미지를 포함하지 않습니다.

## 현재 구현과 남은 작업

| 기능 | 상태 |
| --- | --- |
| ZIP / 7Z / TAR 생성, 목록, 전체 해제, 검사 | 외부 전체 7-Zip 엔진으로 구현 |
| ZIP AES-256 / 7Z 암호 및 파일명 암호화 | 구현 |
| ZIP / 7Z 분할 생성, 첫 볼륨 열기 | 구현, 7Z 분할 통합 테스트 |
| 한글 경로, 작업 취소, 기존 파일 덮어쓰기 방지 | 구현 |
| RAR / RAR5 / GZ / BZ2 / XZ 읽기 | 전체 7-Zip에 위임, 광범위한 변형 검증 필요 |
| ALZ / EGG 열기·해제 | **미구현**, 모듈·배포 조건 확인 필요 |
| ZPAQ 읽기·쓰기 | **미구현** |
| AESLocker 최신판 .lock 복호화 | **미구현**, 호환 파일 구조와 검증 fixture 확보 필요 |
| EasyCrypt 2.4 .ezc 복호화 | **미구현**, 호환 파일 구조와 검증 fixture 확보 필요 |
| Windows 우클릭: 열기 / 새 폴더에 풀기 / ZIP 압축 | 명령 연결·현재 사용자용 등록/제거 스크립트 구현 |
| Windows 11 첫 우클릭 메뉴 | 미구현, 현재는 ‘더 많은 옵션’ 메뉴 |
| 여기에 풀기 / 알아서 풀기 / 우클릭 내부 파일 미리 보기 | 미구현 |
| 선택 해제·내부 편집·추가/삭제·드래그 앤 드롭 | 미구현 |
| 암호 관리자·ZIP 복구·이미지 미리보기·악성코드 검사 | 미구현 |
| 설치/자동 업데이트·코드 서명·macOS 공증 | 미구현 |

RAR·ALZ·EGG는 생성하지 않고 읽기·해제만 지원하는 방향입니다.
AESLocker·EasyCrypt는 **복호화만** 범위에 포함하며 암호화는 구현하지 않습니다.
프로그램이 다른 같은 확장자의 파일까지 호환된다고 가정하지 않습니다.

## 빌드

Windows: Visual Studio 2022 C++ 도구와 Windows 10/11 SDK가 필요합니다.

```bat
tools\build-windows.cmd
build\EoriniZip.exe
build\core_tests.exe "C:\Program Files\7-Zip\7z.exe"
```

CMake 3.21 이상을 사용하는 Windows/macOS 빌드:

```sh
cmake -S . -B build-cmake
cmake --build build-cmake --config Release
ctest --test-dir build-cmake -C Release --output-on-failure
```

macOS에는 Xcode Command Line Tools가 필요합니다.
macOS UI 소스와 CI 빌드가 포함되며, 실제 Mac에서 UI·배포 검증은 별도로 필요합니다.
코어 테스트는 엔진 경로를 인자로 주면 실제 압축 왕복 테스트도 수행합니다.
인자가 없으면 통합 테스트를 건너뛰었다고 명시합니다.

## 압축 엔진

[공식 7-Zip](https://www.7-zip.org/download.html)의 전체 엔진을 설치하고 앱에서 **엔진 연결**로 선택합니다.
Windows는 `7z.exe`와 `7z.dll`이 함께 필요하고 macOS는 `7zz`를 사용합니다.
기능이 제한된 `7za` / `7zr`는 전체 엔진의 대체품이 아닙니다.

Windows 자동 검색: 실행 파일 옆 `7zip/7z.exe`, 상위 폴더의 `vendor/7zip/7z.exe`,
또는 `C:/Program Files/7-Zip/7z.exe`.
macOS: `/opt/homebrew/bin/7zz` 또는 `/usr/local/bin/7zz`.
엔진 바이너리는 저장소에 포함하지 않습니다.

암호가 있는 파일은 비밀번호를 입력한 뒤 열거나 **다시 읽기**를 사용합니다.
암호 입력란은 새 압축의 암호 설정에도 사용합니다.
생성·해제 결과는 선택한 상위 폴더 아래 새 `EoriniZip-…` 폴더에 저장됩니다.

## Windows 탐색기 메뉴

앱을 이동하지 않을 위치에 빌드한 뒤 PowerShell에서:

```powershell
.\tools\explorer-menu.ps1 -Action Install -Executable "D:\Apps\EoriniZip\EoriniZip.exe"
.\tools\explorer-menu.ps1 -Action Uninstall
```

관리자 권한 없이 현재 사용자의 메뉴만 등록합니다.
기본 파일 연결은 변경하지 않습니다. 현재 단일 선택만 지원합니다.
Windows 11에서는 **더 많은 옵션**에 나타납니다.
암호 파일의 우클릭 해제가 실패하면 앱에서 암호를 입력하고 압축 풀기를 다시 실행하세요.

## 검증과 안전 한계

- C++ 단위 검사: 인수 구성, Windows 인용 처리, 목록 파싱, 경로 탈출·예약 이름·링크·충돌 검사.
- 실제 엔진 검사: ZIP/7Z/TAR 왕복, 한글 파일명, 올바른/틀린 암호, 분할 볼륨, 기존 결과 보호, 취소 상태.
- 안전 한도: 100,000개 항목, 선언된 총 해제 크기 20 GiB.
- 사전 경로 검사는 OS 샌드박스를 대신하지 않습니다. 적대적 파일·Unicode 충돌·목록 파싱 모호성·리소스 한도 및 퍼징 검증이 추가로 필요합니다.
- 비밀번호는 설정·로그에 저장하지 않지만 CLI 인수로 전달되어 로컬 프로세스 검사에서 보일 수 있습니다. 배포 전 전용 native API/IPC로 개선할 항목입니다.
- 실제 퍼센트 진행률, 설정 저장, 대용량 목록 가상화, 완전한 고해상도/DPI 대응은 남아 있습니다.

## 라이선스와 브랜드

무료 배포가 목표입니다. 공개 저장소와 오픈소스 재사용 허가는 별개이므로,
프로젝트 자체의 재사용 라이선스는 소유자가 결정하기 전까지 지정하지 않았습니다.
외부 엔진은 각 라이선스와 소스 제공/고지 의무를 검토한 뒤 배포해야 합니다.

- [7-Zip 라이선스](https://www.7-zip.org/license.txt): LGPL 및 일부 BSD/unRAR 조건.
- [EGG 공식 라이선스](https://secure.altools.co.kr/support/License.aspx): 채택할 모듈의 조건을 검토한 뒤 통합.
- [ZPAQ](https://www.mattmahoney.net/dc/zpaq.html): 별도 엔진 통합 예정.
- [AESLocker](https://www.aselocker.com/) / [EasyCrypt 2.4](https://copynull.tistory.com/96): 현재 복호화 호환성 미구현.

로고는 이 프로젝트용으로 이미지 생성 도구에서 새로 만든 캐릭터입니다.
PNG 원본과 Windows ICO / macOS ICNS를 `assets/`에 포함했습니다.
