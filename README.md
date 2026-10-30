<p align="center"><img src="assets/logo.png" width="160" alt="어린이zip 캐릭터"></p>

# 어린이zip · EoriniZip 0.5

Windows Win32 / macOS AppKit와 C++17 공통 코어로 만드는 무료 압축 프로그램입니다. 

**0.5 개발 릴리즈입니다. 아래 미완료 항목이 있어 모든 기능이 완성된 버전으로 표시하지 않습니다.**

## 구현된 기능

- ZIP / 7Z / TAR / ZPAQ 생성·목록·해제·검사. ZIP AES-256, 7Z 파일명 암호화, ZIP/7Z 분할 생성.
- RAR / ALZ / EGG 읽기·해제. ALZ/EGG 전용 Rust 코덱, ZPAQ 네이티브 엔진 포함.
- EasyCrypt 2.4 복호화. 사용자가 제공한 실제 파일로 원본 바이트 일치, 오암호, 변조, 잘림, 중복 이름을 검증했습니다. 개인 검증 파일과 암호는 저장소에 포함하지 않습니다.
- 압축 작업의 임시 폴더는 OS TEMP에 생성하고 종료 시 정리합니다. 지정한 폴더에 직접 해제, 알아서 풀기, 선택한 파일·폴더 해제. 기존 파일은 보존하고 새 결과에 `(1)`, `(2)`를 붙입니다. 폴더 충돌은 폴더 전체를 번호가 붙은 새 이름으로 저장합니다.
- 단일 ZIP/7Z 내부 파일 추가·삭제·이름 변경. 별도 사본에서 수정하고 무결성 검사 후 원본을 교체합니다. 파일 추가는 최상위에 적용되며 같은 내부 이름을 교체하기 전에 확인합니다.
- 이미지(PNG/JPG/BMP/GIF)·텍스트 내장 미리보기. 선택한 파일만 임시 해제하고 창 종료 시 정리합니다. 실행 파일은 실행하지 않습니다. 실제 화면 배치 검증은 남아 있습니다.
- 노란색 시작 화면, 폴더 트리, 열별 목록 정렬, 파일 찾기, 최근 파일 10개, 파일 끌어놓기, 다른 이름으로 복사, 휴지통/영구 삭제, 새 창.
- 컴팩트 진행 창, 진행률·경과 시간·예상 남은 시간, 취소. 목록 읽기 성공 시 진행 창은 자동으로 닫힙니다.
- 암호 파일 열기·우클릭 해제의 비밀번호 입력/재시도 경로. 엔진 오류 판별과 암호 대화상자 리소스 검사가 있으며 실제 화면 조작 검증은 별도로 남아 있습니다.
- 파일 연결 전체 선택/해제, 일반 설정, 압축 기본값, UTF-8/ZIP 코드페이지, 날짜 저장, Thumbs.db 제외, 멀티코어 옵션, 다운로드 출처 보존, 테마·트리·상태줄, INI 설정 가져오기/내보내기.
- 클래식 탐색기 메뉴: 현재 폴더/새 폴더/알아서 풀기, 즉시 ZIP 압축, 각각 압축/해제. ZIP만 내부 파일 목록·아이콘 미리보기를 제공합니다.
- Windows 11 IExplorerCommand 구현과 sparse package 구성. 실제 첫 메뉴 등록·배포 검증은 미완료입니다. 일반 설치 파일은 클래식 메뉴를 등록합니다.

AESLocker는 요청에 따라 지원 범위에서 제외했습니다. RAR·ALZ·EGG 생성은 지원하지 않습니다.

## 빌드 및 설치 파일

Windows: Visual Studio 2022 C++ 도구, Windows SDK, Rust stable이 필요합니다.

```bat
tools\build-windows.cmd
build\core_tests.exe "C:\Program Files\7-Zip\7z.exe" third_party
build\split_tests.exe "C:\Program Files\7-Zip\7z.exe" third_party tests\fixtures\rar
```

공식 전체 7-Zip의 `7z.exe`와 `7z.dll`을 `vendor/7zip`에 둡니다. 제한된 `7za`/`7zr`는 대체하지 못합니다. 패키지에는 실행 파일 옆 `7zip` 폴더로 포함됩니다.

```powershell
./tools/package-windows.ps1 -BuildDirectory build
& 'C:/Program Files (x86)/Inno Setup 6/ISCC.exe' packaging/installer/EoriniZip.iss
```

설치 파일 이름은 `dist/EoriniZip-0.5-windows-x64-setup.exe`입니다. 현재 사용자 폴더에 설치하며 기본 앱 연결을 강제로 변경하지 않습니다. 코드 서명은 아직 없습니다.

CMake 3.21 이상:

```sh
cmake -S . -B build-cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build-cmake --config Release
ctest --test-dir build-cmake -C Release --output-on-failure
```

macOS에는 Xcode Command Line Tools, Rust와 전체 `7zz`가 필요합니다. Mac 빌드·실행·공증은 현재 Windows 환경에서 검증하지 않았습니다.

## 검증과 안전 범위

- ZIP/7Z/TAR/ZPAQ 왕복, 암호·한글 이름, 파일/폴더 충돌 번호 저장, 취소 정리, 선택 해제, 내부 편집 실패 시 원본 보존.
- ALZ/EGG의 store/deflate/bzip2/AZO 및 암호·분할 fixture. RAR4/RAR5와 ZIP/7Z/ALZ/EGG 분할 정상·중간 누락·손상 검사. 모든 외부 프로그램/버전의 분할 변형을 보장하지 않습니다.
- 현재 ZIP 분할 생성은 `.001/.002` 방식입니다. `.z01` 방식 생성은 미구현입니다.
- 경로 탈출, 예약 이름, 링크, 중복 경로 검사. 100,000개 항목 / 선언된 총 해제 크기 20 GiB 한도가 있습니다. 이 검사는 OS 샌드박스를 대신하지 않습니다.
- 암호는 설정·로그에 저장하지 않지만 엔진 CLI 인수로 전달되어 로컬 프로세스 검사에서 보일 수 있습니다. 전용 API/IPC 전환은 남아 있습니다.
- 멀티코어 옵션은 엔진에 전달합니다. 실제 병렬성은 포맷과 코덱에 따라 달라지며 모든 단일 압축 스트림이 병렬 해제되는 것은 아닙니다.
- 2026-10-31: 공식 7-Zip 최신 26.05 포함, RustSec 감사 세 코덱 통과, 외부 코덱 4개 최신 커밋 일치 확인. 알려지지 않은 취약점 부재를 보장하지 않습니다.
- GitHub Actions에 RustSec/업스트림 변경 감시, Dependabot을 구성했습니다. 실제 원격 실행은 푸시 후 확인해야 합니다.

## 아직 완료되지 않은 기능

- 실제 비밀번호 모달 및 최신 설정 화면 조작 검증: 화면 도구 초기화 오류로 진행 불가.
- Windows 11 첫 메뉴의 실제 등록/서명 배포: 현재 PC는 Windows 10 19045여서 OS 요구사항 검사에서 등록이 거부됐습니다. Mac UI와 ARM64/Windows PE 검증도 남았습니다.
- 외부 편집 후 재압축, 클립보드 파일 복사/붙여넣기, 압축 설명, 암호 관리자/복구, 손상 ZIP 복구, 악성코드 검사.
- 환경설정의 나머지 세부 항목(글꼴·칼럼·임시 폴더·언어·업데이트 등), SFX/ISO/WIM 및 추가 생성 포맷, 자동 업데이트/서명/공증.

## 라이선스

`THIRD_PARTY_NOTICES.txt`와 각 `third_party` 하위 라이선스를 확인하세요. ALZ/EGG는 MIT 구현, EasyCrypt는 MIT easycrab 기반, ZPAQ는 public-domain/Unlicense와 MIT 부분을 사용합니다. 배포 패키지에 Rust 의존성 고지와 7-Zip 라이선스를 포함합니다. 프로젝트 자체 재사용 라이선스는 아직 지정하지 않았습니다.
