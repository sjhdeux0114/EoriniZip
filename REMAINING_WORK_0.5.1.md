# EoriniZip 0.5.1 검증 및 후속 작업

갱신: 2026-11-01 (한국 시간). 이전 중단 후 작업을 재개했습니다.

## 로컬 완료

- Windows 전체 네이티브 빌드 및 메뉴·대화상자 리소스·자격 증명 검사 통과.
- 7-Zip 26.05 코어 통합 및 ALZ/EGG/RAR4/RAR5/ZIP/7Z 분할 검사 통과.
- CF_HDROP 유니코드·긴 경로·복수 파일·빈 선택 검사 통과. 별도 클립보드 왕복은 전용 station 생성 불가(오류 183)로 명시적 skip(77). 사용자 클립보드를 건드리지 않습니다.
- 업데이트 서명 가드 및 PowerShell 구문 검사 통과.
- 실제 UI: 암호 마스킹, 오암호 재입력, 취소, 정상 암호 목록 열기. 설정 보기·글꼴·언어 페이지 표시와 취소 확인.
- dist/EoriniZip-0.5.1 최신 패키지 조립, 자체 EXE/DLL과 MSIX, setup 서명 Valid.
- dist/EoriniZip-0.5.1-windows-x64-setup.exe는 최신 설치 파일입니다.
- 별도 테스트 AppID 설치·제거 성공, 최종 사용자 설치 성공. 설치 앱 SHA-256이 패키지와 일치합니다.
- 현대 메뉴 패키지 Status Ok, EoriniZipModernShell_v051.dll 등록. 클래식 메뉴도 LocalAppData/Programs/EoriniZip의 버전 DLL 참조.
- 상세 변경과 기능 제한은 docs/release-0.5.1.md에 기록했습니다.

## 배포 완료

- 구현 커밋·푸시 완료. 릴리즈 태그 v0.5.1은 검증한 소스 9e104e6을 참조합니다.
- [Windows/macOS 빌드·테스트](https://github.com/sjhdeux0114/EoriniZip/actions/runs/41254697397), [보안 검사](https://github.com/sjhdeux0114/EoriniZip/actions/runs/41254697385) 통과.
- [macOS 15 arm64/x86_64 패키징](https://github.com/sjhdeux0114/EoriniZip/actions/runs/41254697409) 통과. 번들 엔진으로 통합 검사, 아키텍처·라이브러리·서명 검사와 DMG 무결성 확인.
- Windows의 [폴더] 접두어를 제거하고 기본 폴더 아이콘으로 표시. 왼쪽 트리와 Mac 파일 목록에도 적용.
- 아이콘 수정 후 Windows 설치 프로그램·휴대용 ZIP 재생성, 서명 Valid. 최종 설치 성공 및 설치된 파일 해시 일치.
- Mac arm64/Intel DMG·ZIP 및 체크섬을 dist에 저장. 다운로드한 파일 해시가 CI 체크섬과 일치.
- [v0.5.1 개발 릴리즈](https://github.com/sjhdeux0114/EoriniZip/releases/tag/v0.5.1) 공개 게시 완료. Windows/Mac 파일과 공개 CER·MSIX·체크섬 업로드. 업로드 자산의 SHA-256을 로컬 dist와 대조했습니다.
- Windows·Mac 설치 파일의 직접 링크는 README.md에 정리했습니다.

## 후속 검증 및 제한

- 설정의 모든 저장/재시작 조합, 클립보드 실제 복사/붙여넣기, 외부 편집 전체 UI 흐름.
- Mac 실제 대화형 UI, Windows ARM64/PE 실기기, 일반 배포용 공인 인증서와 Mac 공증.
- 영어 상세 메시지 완전 번역, 폴더 붙여넣기와 대형/바이너리 외부 편집.
- ZIP 복구는 비암호화 store/deflate의 완전한 로컬 레코드만 지원. 암호 복구는 제공 후보 목록만 시도.
- ISO는 비부팅 데이터 이미지, WIM은 비압축. 업데이트 새 안정 버전 다운로드·설치 전체는 별도 검증 필요.
- 암호의 기존 코덱 CLI 인수 전달을 전용 API/IPC로 변경하는 후속 작업.

## 서명

CN=EoriniZip 테스트 인증서: 3587837FAF6C582184FD02A0E3E97A3E4BCF998F.
이 PC의 신뢰 등록은 사용자가 승인했습니다. 다른 PC에서는 기본적으로 신뢰되지 않으며 자동 신뢰 등록하지 않습니다. 개인 키는 업로드하지 않습니다.
