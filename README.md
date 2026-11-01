<p align="center"><img src="assets/logo.png" width="160" alt="어린이zip 캐릭터"></p>

# 어린이zip · EoriniZip 0.5.1

Windows Win32 / macOS AppKit와 C++17 공통 코어로 만드는 무료 압축 프로그램입니다. 

**0.5.1 개발 릴리즈입니다. Windows 11 첫 메뉴와 macOS 한글 파일명 처리를 보완했습니다. 로컬 테스트 인증서로 서명했으며 공인 서명·Mac 공증을 마친 일반 배포 버전은 아닙니다.**

## 다운로드 · 0.5.1 개발 릴리즈

| 운영체제 | 설치 파일 |
| --- | --- |
| Windows x64 | [설치 프로그램](https://github.com/sjhdeux0114/EoriniZip/releases/download/v0.5.1/EoriniZip-0.5.1-windows-x64-setup.exe) |
| Mac Apple Silicon (M1 이상) | [arm64 DMG](https://github.com/sjhdeux0114/EoriniZip/releases/download/v0.5.1/EoriniZip-0.5.1-macos-arm64.dmg) |
| Mac Intel | [x86_64 DMG](https://github.com/sjhdeux0114/EoriniZip/releases/download/v0.5.1/EoriniZip-0.5.1-macos-x86_64.dmg) |

Mac은 macOS 13 이상을 배포 대상으로 빌드했습니다. DMG에서 앱을 Applications로 옮겨 설치합니다. [전체 파일·ZIP·체크섬·검증 기록](https://github.com/sjhdeux0114/EoriniZip/releases/tag/v0.5.1)을 확인하세요. Mac은 ad-hoc 서명이며 Apple 공증은 없습니다.

## 압축과 파일명

- ZIP / 7Z / TAR / ZPAQ 생성·목록·해제·검사. ZIP AES-256, 7Z 파일명 암호화, ZIP/7Z 분할 생성.
- GZIP / BZIP2 / XZ 단일 파일 압축. 폴더는 먼저 TAR로 저장합니다. WIM 비압축 저장, Windows용 7Z SFX 자체 해제 EXE 생성(비분할 7Z)을 지원합니다.
- 폴더 하나의 내용으로 비부팅 ISO 생성. Windows는 IMAPI2FS로 ISO9660/Joliet/UDF를 기록하며 macOS는 시스템 hdiutil을 사용합니다.
- RAR / ALZ / EGG 읽기·해제, EasyCrypt 2.4 복호화. 전용 Rust ALZ/EGG 코덱과 ZPAQ 엔진 포함. RAR·ALZ·EGG 생성과 AESLocker는 지원하지 않습니다.
- **macOS의 NFD 한글 이름을 해제 결과에서 NFC로 조합합니다.** 원래 내부 이름은 엔진 선택·수정에 사용하고 Windows 목록·트리는 조합한 이름을 표시합니다. 악센트 문자도 정규화하며 정규화·대소문자 비교 후 충돌하는 이름은 거부합니다.
- 기존 파일은 보존합니다. 같은 파일은 `(1)`, `(2)`를 붙이고 폴더 충돌은 폴더 전체를 새 이름으로 저장합니다. 선택 해제, 알아서 풀기, 각각 압축/해제를 지원합니다.

## Windows 기능

- Windows 11 첫 우클릭의 **어린이zip** 하위 메뉴: 열기, 압축하기, ZIP 바로 압축, 각각 압축, 여기에 풀기, 알아서 풀기, 새 폴더에 풀기, 각각 풀기, ZIP 목록 미리보기. 선택에 맞는 명령만 표시하며 클래식 메뉴도 제공합니다.
- 단일 ZIP/7Z 내부 파일 추가·삭제·이름 변경. 사본에서 수정·검사한 뒤 교체하며 원본이 다른 프로그램에서 변경되면 중단합니다.
- 이미지·텍스트 내장 미리보기. 실행 파일은 실행하지 않습니다. 텍스트 하나(32 MiB 이하)를 메모장으로 편집한 뒤 저장·닫고 명시적으로 반영할 수 있습니다. 내부 경로와 암호화를 유지합니다.
- 선택 파일/폴더를 클립보드로 복사하여 탐색기에 붙여넣기. 임시 파일은 **어린이zip을 열어 둔 동안** 유효하며 종료 시 정리합니다. 탐색기에서 복사한 일반 파일을 ZIP/7Z 최상위에 붙여넣을 수 있습니다. 폴더 붙여넣기는 지원하지 않습니다.
- 단일 ZIP 설명 읽기·편집: UTF-8 65,535바이트까지. **설명은 암호화되지 않습니다.**
- 현재 압축 파일의 암호를 Windows 자격 증명 관리자에 명시적으로 저장·불러오기·삭제. 현재 사용자에게 보관하며 INI 설정·로그에는 포함하지 않습니다.
- UTF-8 암호 후보 파일 검사(한 줄에 하나, 최대 10,000개 / 4 MiB). 무결성 검사를 통과한 후보를 찾습니다. 후보에 없는 암호는 복구하지 못합니다.
- ZIP 중앙 디렉터리 재구성: 크기를 가진 완전한 비암호화 store/deflate 항목만 지원합니다. ZIP64·데이터 디스크립터·암호화 ZIP은 제외하며 CRC가 틀리면 결과를 저장하지 않습니다. 원본은 유지합니다.
- Microsoft Defender 사용자 지정 검사. 자동 치료를 요청하지 않으며 검사 오류를 안전 판정으로 표시하지 않습니다. 암호화된 내부 파일은 해제 후 별도 검사가 필요합니다.
- 시작 화면, 폴더 트리, 정렬, 찾기, 최근 파일 10개, 드래그 앤 드롭, 다른 이름으로 복사, 휴지통/영구 삭제, 새 창, 진행 창과 취소.
- 파일 연결 전체 선택/해제, ZIP 코드페이지, UTF-8 이름, 날짜 저장, Thumbs.db 제외, 멀티코어, 다운로드 출처 보존. 글꼴 3종·크기 5단계, 칼럼, 툴바/트리/상태줄, 테마, 임시 폴더, INI 가져오기/내보내기.
- 한국어 / 영어 기본 UI 선택(재시작 후 적용). 일부 상세 안내와 엔진 오류는 한국어로 남을 수 있습니다.
- 수동 및 선택적 시작 시 업데이트 확인. 공개 안정 릴리즈를 조회하며 **유효한 Authenticode 서명과 기존 앱과 동일한 서명자**를 확인한 설치 파일만 실행합니다. 개발 릴리즈는 GitHub에서 직접 확인합니다.

macOS는 공통 코어의 압축·해제·검사와 이름 정규화를 사용합니다. Windows 전용 기능이 Mac UI에도 모두 구현되었다고 표시하지 않습니다.

## 빌드와 검증

Windows: Visual Studio 2022 C++ 도구, Windows SDK, Rust stable. 공식 전체 7-Zip 26.05의 `7z.exe`, `7z.dll`, `7z.sfx`, `License.txt`, `readme.txt`를 `vendor/7zip`에 둡니다. `7za`/`7zr`는 대체하지 못합니다.

```bat
tools\build-windows.cmd
build\core_tests.exe "%CD%\vendor\7zip\7z.exe" third_party
build\split_tests.exe "%CD%\vendor\7zip\7z.exe" third_party tests\fixtures\rar
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tests\update_security_tests.ps1
```

CMake 3.21 이상도 지원합니다.

```sh
cmake -S . -B build-cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build-cmake --config Release
ctest --test-dir build-cmake -C Release --output-on-failure
```

macOS에는 Xcode Command Line Tools, Rust와 전체 7zz가 필요합니다. GitHub Actions의 빌드·코어 검사와 별개로 대화형 Mac UI·공증 및 Windows ARM64/PE는 실기기 검증이 필요합니다. [0.5.1 검증 기록](docs/release-0.5.1.md)을 확인하세요.

## 설치 파일과 Windows 11 메뉴

첫 메뉴에는 DLL/레지스트리 외에 패키지 ID가 필요합니다. 설치 프로그램은 탐색기 메뉴 옵션을 선택하면 서명된 sparse MSIX를 등록하고 제거 시 등록도 해제합니다. 확장 DLL 이름은 버전별로 구분합니다. 기본 앱 연결은 강제로 바꾸지 않습니다.

```powershell
./tools/package-windows.ps1 -BuildDirectory build -Destination dist/EoriniZip-0.5.1
./tools/build-context-menu-package.ps1 -Directory dist/EoriniZip-0.5.1 -CertificateThumbprint <인증서 지문>
& '<Inno Setup 6 경로>/ISCC.exe' /DPackageDirectory=C:\work\eorinizip\dist\EoriniZip-0.5.1 packaging/installer/EoriniZip.iss
```

인증서 Subject는 Publisher인 `CN=EoriniZip`과 일치해야 합니다. 일반 배포에는 신뢰된 코드 서명 인증서가 필요합니다. 이미 서명한 외부 MSIX는 패키징의 `-SignedContextMenuPackage`로 포함할 수 있습니다. **개인 키는 설치 파일·저장소에 포함하지 않습니다.**

현재 빌드는 `EoriniZip-test-signing.cer`의 로컬 인증서를 사용합니다. 이 PC에서는 사용자 승인으로 이 인증서만 컴퓨터의 TrustedPeople에 등록하여 첫 메뉴를 검증했습니다. 다른 PC는 기본적으로 신뢰하지 않습니다. 테스트 인증서를 신뢰하기로 명시적으로 결정한 경우에만 관리자 PowerShell에서 아래 명령을 실행한 뒤 설치하세요. 신뢰를 추가하지 않으면 클래식 메뉴를 사용할 수 있으며 첫 메뉴 등록 실패를 안내합니다.

`THIRD_PARTY_NOTICES.txt`와 각 `third_party` 하위 라이선스를 확인하세요. ALZ/EGG는 MIT 구현, EasyCrypt는 MIT easycrab 기반, ZPAQ는 public-domain/Unlicense와 MIT 부분을 사용합니다. 배포 패키지에 Rust 의존성 고지와 7-Zip 라이선스를 포함합니다. 프로젝트 자체 재사용 라이선스는 아직 지정하지 않았습니다.


## Mac 앱 패키지

macOS 13 이상용 Apple Silicon(arm64)과 Intel(x86_64) 패키지를 각각 빌드합니다. `EoriniZip.app` 안에 공식 7zz와 ALZ/EGG/ZPAQ/EasyCrypt 코덱·라이선스를 포함하므로 Homebrew나 엔진 수동 연결이 필요하지 않습니다. DMG를 열고 앱을 Applications로 드래그해 설치합니다.

Mac에서 재현:

```sh
export MACOSX_DEPLOYMENT_TARGET=13.0
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_DEPLOYMENT_TARGET=13.0
cmake --build build --parallel 3
ctest --test-dir build --output-on-failure
python3 tools/package-macos.py --arch "$(uname -m)"
```

먼저 `vendor/7zip`에 공식 Mac용 7zz, License.txt, readme.txt, 7z2605-src.tar.xz를 준비합니다. `.github/workflows/macos-package.yml`에서 두 CPU의 빌드·통합 검사·패키징·앱 서명 검증을 실행합니다. 패키징은 포함한 엔진과 코덱으로 통합 검사를 다시 실행하고 DMG 무결성도 확인합니다.

현재 Mac 배포물은 ad-hoc 서명된 개발 빌드입니다. Apple Developer ID 서명·공증을 완료한 일반 배포판은 아니며, 실제 Mac 대화형 UI 전체 검증은 별도입니다.
