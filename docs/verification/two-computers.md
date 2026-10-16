# Task8 설치·Cloudflare·두 클라이언트 통합

2026-10-17, macOS26.4 arm64 / Qt6.11.2 / QtKeychain0.17 / CMake4.4.4. Linux 실기는 사용자 요청대로 유예한다.

## 구분

| 항목 | 현재 증거 |
| --- | --- |
| C++ 기준 서버 + 별도 Qt/QML 클라이언트2프로세스 | offscreen·실제 Cocoa 전체 흐름 통과 |
| 로컬 Cloudflare workerd + 같은 Qt 클라이언트2프로세스 | 실제 Cocoa 전체 흐름 통과 |
| Cloudflare 단위/저장소/소켓 | 현재12/12, 타입 검사·dry-run·npm audit0 취약점 |
| macOS 설치 산출물 | Qt/QtKeychain 포함 .app/.zip/.dmg 생성, DMG read-only mount→별도 위치 복사→내부 의존성/서명/native smoke 통과 |
| 외부 Cloudflare WSS | 계정 login 필요, 미배포 |
| 실제 서로 다른 두 컴퓨터 / KDE Wayland | 미실행, Linux는 명시적 유예 |
| Developer ID·공증 | 자격증명 없음, 미완료 |
| native30분 idle/busy/상호작용 | reduced-motion30분 통과, 기본 애니메이션30분은 실행 중 |

## 통합 검사

`tests/integration/tst_twopeers.cpp`가 서버와 두 별도 프로세스를 실행한다. 제품 SceneModel/SessionClient/QML/OverlayWindow를 사용하며 제어용 stdin/stdout은 테스트 fixture에만 있다. production binary에 테스트용 제어 기능을 넣지 않았다. 테스트 peer만 memory secret backend를 쓰고 실제 Keychain 증거는 [Task7](client.md)의 native검사다.

초대와 명시적 승인, 한글/이모지/줄바꿈/태그 WIP 저장, busy 장난 거절, 커피/에너지/쪽지 대기와 명시적 확인, 양쪽 동일 말풍선과 서버 이벤트, 밧줄/간식 이동 후 위치 유지, offline 발송 거절과 재접속 후 중복 없음, 동일 identity 복귀 및 권한 폐기를 한 흐름에서 확인한다. 두 프로세스는 두 물리 컴퓨터 증거가 아니다. UI 경로로 WIP 저장과 받은 항목 확인을 실행하고 나머지 행동은 동일 모델 의도를 사용한다.

Cloudflare 이식은 같은 wire v1과 `core/copy/ko.json`을 사용한다. Node/Miniflare 검사에서 malformed 봉투·Unicode/Qt trim·권한·busy·이동/크기·재시도 충돌·현재 소켓 교체·SQLite 재시작·평문 token/쪽지 미저장·요청 한도 재접속 우회·초대 만료를 확인한다. 테스트의 storage inspection은 로컬 인스턴스에만 켰으며 서버에 테스트 API를 만들지 않았다.

실제 Cocoa의 Worker 소켓을35초 유지하며 연결 교체 없이 같은 identity/heartbeat가 유지되는 것을 확인했다. 별도 자체 작업창을 활성화해 `a` 입력 후 Worker 커피 이벤트를 전달하고, 작업창 active 상태와 `b` 입력으로 `ab`가 유지되는 것을 확인했다. 수신 peer overlay는 active가 되지 않았다. 이는 해당 정상 수신 시나리오의 증거이며 모든 native 창 순서/OS 입력기 검증을 대신하지 않는다.

전체 CTest20/20 및 GUI 의존성을 끈 기준 서버 별도 빌드4/4가 통과했다.

## 설치 검증

`packaging/package_macos.py`는 Release 및 canonical ASCII 경로에서 공식 macdeployqt를 실행한다. 실제 Mach-O fixture로 외부 dylib/누락된 dylib/외부 rpath를 실패 재현하고 내부 상대 경로는 허용했다. 실제 번들에서 Qt 배포 도구가 남긴 외부 상대 rpath·Frameworks 검색 경로 누락과 한글 경로 NFC/NFD 계산 오류도 검출해 수정했다. 전체 비시스템 의존성이 번들 내부로 해석되는지 검증한다.

로컬 산출물에는 Qt/QML/플러그인·QtKeychain·공식 라이선스·SPDX/고지·SHA256이 들어 있다. 현재 arm64 / macOS26.0 deployment target이며 실제26.4에서만 검사했다. ad-hoc 서명 local build이며 정식 서명·공증/공개 release로 표시하지 않는다. Qt 미설치 별도 Mac과 Intel 검증은 없다. [설치 절차](../../packaging/README.md).

## 실행 측정과 남은 범위

처음30분 검사는 Qt Test의 기본5분 function timeout으로 중단됐다. 이는 제품 crash 증거가 아니며, `QTEST_FUNCTION_TIMEOUT=1900000`을 해당 선택 검사에만 지정해 다시 실행했다. 자체 server/two-client pid의 ps CPU/RSS만10초 간격으로 기록하며 화면·키보드·다른 앱 데이터를 수집하지 않는다. CPU값은 ps 시점 값이며 사용자 상호작용 지연/모든 native focus·배율 증거로 확대하지 않는다.

기존 Dock/work-area/실제 display scale·일부 가장자리와 OS IME 미확인 항목은 [오버레이 기록](overlay.md)을 유지한다. 외부 서버의 인증서·실제 WSS와 무료 계정 선택은 login 후 검증한다. Linux 빌드/설치/IMEs/비밀 저장소/실기 및 실제 두 컴퓨터는 완료된 것으로 주장하지 않는다.

reduced-motion 선택 검사는1800.768초 동안 정상 완료했다. 10초마다180회/프로세스,총540행을 기록했다. steady 구간 CPU는 각 프로세스 대체로0.0–0.1%였고 시작/전환 순간 peak를 별도로 포함한다. 기본 애니메이션은 더 많은 렌더링 CPU를 사용하므로 이 수치를 기본 설정의 성능으로 표시하지 않는다. 기본 모드 측정 및 최종 수치 기록은 완료 후 갱신한다.
