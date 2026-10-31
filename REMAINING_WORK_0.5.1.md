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

## 배포 진행 순서

1. 커밋·푸시 후 Windows/macOS GitHub Actions 확인. 실패 시 수정 후 재실행.
2. 휴대용 ZIP·MSIX·공개 CER·체크섬과 setup을 v0.5.1 prerelease에 게시.
3. 원격 CI 및 릴리즈 결과를 이 문서에 갱신.

## 후속 검증 및 제한

- 설정의 모든 저장/재시작 조합, 클립보드 실제 복사/붙여넣기, 외부 편집 전체 UI 흐름.
- Mac 실제 UI, ARM64/Windows PE 실기기, 일반 배포용 공인 인증서와 Mac 공증.
- 영어 상세 메시지 완전 번역, 폴더 붙여넣기와 대형/바이너리 외부 편집.
- ZIP 복구는 비암호화 store/deflate의 완전한 로컬 레코드만 지원. 암호 복구는 제공 후보 목록만 시도.
- ISO는 비부팅 데이터 이미지, WIM은 비압축. 업데이트 새 안정 버전 다운로드·설치 전체는 별도 검증 필요.
- 암호의 기존 코덱 CLI 인수 전달을 전용 API/IPC로 변경하는 후속 작업.

## 서명

CN=EoriniZip 테스트 인증서: 3587837FAF6C582184FD02A0E3E97A3E4BCF998F.
이 PC의 신뢰 등록은 사용자가 승인했습니다. 다른 PC에서는 기본적으로 신뢰되지 않으며 자동 신뢰 등록하지 않습니다. 개인 키는 업로드하지 않습니다.
