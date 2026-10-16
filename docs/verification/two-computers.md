# Task8 설치·Cloudflare·두 클라이언트 통합

2026-10-17, macOS26.4 arm64 / Qt6.11.2 / QtKeychain0.17 / CMake4.4.4. Linux 실기는 사용자 요청대로 유예한다.

## 구분

| 항목 | 현재 증거 |
| --- | --- |
| C++ 기준 서버 + 별도 Qt/QML 클라이언트2프로세스 | offscreen·실제 Cocoa 전체 흐름 통과 |
| 로컬 Cloudflare workerd + 같은 Qt 클라이언트2프로세스 | 실제 Cocoa 전체 흐름 통과 |
| Cloudflare 단위/저장소/소켓 | 17/17, 타입 검사·dry-run·npm audit0 취약점 |
| macOS 설치 산출물 | Qt/QtKeychain 포함 .app/.zip/.dmg 생성, DMG read-only mount→별도 위치 복사→내부 의존성/서명/native smoke 통과 |
| 외부 Cloudflare WSS | 계정 login 필요, 미배포 |
| 실제 서로 다른 두 컴퓨터 / KDE Wayland | 미실행, Linux는 명시적 유예 |
| Developer ID·공증 | 자격증명 없음, 미완료 |
| native30분 idle/busy/상호작용 | reduced-motion·기본 애니메이션 각각30분 통과, heartbeat 확장 전 소스 기준 |

## 통합 검사

`tests/integration/tst_twopeers.cpp`가 서버와 두 별도 프로세스를 실행한다. 제품 SceneModel/SessionClient/QML/OverlayWindow를 사용하며 제어용 stdin/stdout은 테스트 fixture에만 있다. production binary에 테스트용 제어 기능을 넣지 않았다. 테스트 peer만 memory secret backend를 쓰고 실제 Keychain 증거는 [Task7](client.md)의 native검사다.

초대와 명시적 승인, 한글/이모지/줄바꿈/태그 WIP 저장, busy 장난 거절, 커피/에너지/쪽지 대기와 명시적 확인, 양쪽 동일 말풍선과 서버 이벤트, 밧줄/간식 이동 후 위치 유지, offline 발송 거절과 재접속 후 중복 없음, 동일 identity 복귀 및 권한 폐기를 한 흐름에서 확인한다. 두 프로세스는 두 물리 컴퓨터 증거가 아니다. UI 경로로 WIP 저장과 받은 항목 확인을 실행하고 나머지 행동은 동일 모델 의도를 사용한다.

Cloudflare 이식은 같은 wire v1과 `core/copy/ko.json`을 사용한다. Node/Miniflare 검사에서 malformed 봉투·Unicode/Qt trim·권한·busy·이동/크기·재시도 충돌·현재 소켓 교체·SQLite 재시작·평문 token/쪽지 미저장·요청 한도 재접속 우회·초대 만료를 확인한다. 테스트의 storage inspection은 로컬 인스턴스에만 켰으며 서버에 테스트 API를 만들지 않았다.

실제 Cocoa의 Worker 소켓을35초 유지하며 연결 교체 없이 같은 identity/heartbeat가 유지되는 것을 확인했다. 별도 자체 작업창을 활성화해 `a` 입력 후 Worker 커피 이벤트를 전달하고, 작업창 active 상태와 `b` 입력으로 `ab`가 유지되는 것을 확인했다. 수신 peer overlay는 active가 되지 않았다. 이는 해당 정상 수신 시나리오의 증거이며 모든 native 창 순서/OS 입력기 검증을 대신하지 않는다.

최종 전체 CTest20/20(18.56초), Worker17/17(44.19초)과 타입 검사·audit0이 통과했다. GUI 의존성을 끈 기준 서버 별도 빌드4/4도 통과했다.

독립 리뷰의 두 Important를 회귀 RED→GREEN으로 수정했다. 실제76개 쪽지의 큰 snapshot을 반복 요청하면 캐시 예산 소진 시 `capacity` 후 해당 소켓만 종료하고, 재접속 후 이미 수락한 명령에는 같은 응답만 반환한다.64개 캐시는512KiB/개·office 전체16MiB 예산을 공유한다. 별도 실제 소켓 검사에서31초 동안 조용한 상대에게 쪽지를 보내면 offline 실패이고, 재접속해도 받은 항목과 재생 이벤트가 추가되지 않는다. `session.heartbeat`의 인증·빈 body, Qt의 요청 발신 및 RFC pong이 정상이어도 앱 응답이 없으면 재접속하는 경계를 확인했다. 최종 Qt 코드로 Worker 전체 흐름·35초 정상 heartbeat·수신 중 작업창 포커스를 실제 Cocoa에서 다시 통과했다.

## 설치 검증

`packaging/package_macos.py`는 Release 및 canonical ASCII 경로에서 공식 macdeployqt를 실행한다. 실제 Mach-O fixture로 외부 dylib/누락된 dylib/외부 rpath를 실패 재현하고 내부 상대 경로는 허용했다. 실제 번들에서 Qt 배포 도구가 남긴 외부 상대 rpath·Frameworks 검색 경로 누락과 한글 경로 NFC/NFD 계산 오류도 검출해 수정했다. 전체 비시스템 의존성이 번들 내부로 해석되는지 검증한다.

로컬 산출물에는 Qt/QML/플러그인·QtKeychain·공식 라이선스·SPDX/고지·SHA256이 들어 있다. 현재 arm64 / macOS26.0 deployment target이며 실제26.4에서만 검사했다. ad-hoc 서명 local build이며 정식 서명·공증/공개 release로 표시하지 않는다. Qt 미설치 별도 Mac과 Intel 검증은 없다. [설치 절차](../../packaging/README.md).

## 실행 측정과 남은 범위

처음30분 검사는 Qt Test의 기본5분 function timeout으로 중단됐다. 이는 제품 crash 증거가 아니며, `QTEST_FUNCTION_TIMEOUT=1900000`을 해당 선택 검사에만 지정해 다시 실행했다. 자체 server/two-client pid의 ps CPU/RSS만10초 간격으로 기록하며 화면·키보드·다른 앱 데이터를 수집하지 않는다. CPU값은 ps 시점 값이며 사용자 상호작용 지연/모든 native focus·배율 증거로 확대하지 않는다.

기존 Dock/work-area/실제 display scale·일부 가장자리와 OS IME 미확인 항목은 [오버레이 기록](overlay.md)을 유지한다. 외부 서버의 인증서·실제 WSS와 무료 계정 선택은 login 후 검증한다. Linux 빌드/설치/IMEs/비밀 저장소/실기 및 실제 두 컴퓨터는 완료된 것으로 주장하지 않는다.

reduced-motion 선택 검사는1800.768초, 기본 애니메이션 검사는30분 동안 정상 완료했다. 각 검사에서10초마다180회/프로세스,총540행을 기록했다. 아래는 시작/전환 순간을 포함한 구간별 `ps` 평균이며100%는 논리 CPU 하나 기준이다. RSS는 구간 내 최솟값–최댓값이다.

| 모드/구간 | 서버 CPU | 클라이언트 A/B CPU | 클라이언트 A/B RSS(MiB) |
| --- | --- | --- | --- |
| 기본 / idle | 0.25% | 10.84% / 9.63% | 86.9–103.1 / 81.8–96.8 |
| 기본 / busy | 0.09% | 6.46% / 5.97% | 88.0–95.6 / 83.3–94.8 |
| 기본 / 상호작용 | 0.09% | 5.86% / 5.89% | 88.9–98.1 / 86.4–95.0 |
| 움직임 줄이기 / idle | 0.33% | 0.39% / 0.42% | 75.7–96.5 / 80.5–101.6 |
| 움직임 줄이기 / busy | 0.10% | 0.56% / 0.52% | 86.6–88.8 / 91.2–93.2 |
| 움직임 줄이기 / 상호작용 | 0.08% | 0.39% / 0.38% | 87.6–91.0 / 92.2–95.2 |

원자료: [기본 애니메이션](evidence/soak-normal-motion.csv), [움직임 줄이기](evidence/soak-reduced-motion.csv). C++ 기준 서버와 Cocoa 두 프로세스, 소스5e70a48 기반이며 최종 앱 heartbeat 추가 전이다. 최종 heartbeat 경로는 별도35초 native 검사로 확인했다. 기본 렌더링의 CPU 사용은 개선 여지가 있으며, 움직임 줄이기 수치를 기본 성능이나 배터리 보장으로 표시하지 않는다. 다른 앱/빌드 검사도 같은 Mac에서 실행돼 비교 실험으로 해석하지 않는다. 두 번째 busy 구간은 양쪽 모델 상태와 창 렌더링을 포함하며 CPU가0이 되는 것을 요구하지 않는다.
