# Shared Pet Office Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** macOS와 KDE Plasma Wayland의 두 컴퓨터에서 승인한 펫 오버레이, WIP, busy, 장난, 선물과 쪽지를 함께 사용하는 MVP를 만든다.

**Architecture:** macOS 오버레이를 기준으로 먼저 제품을 구현한다. Qt 공통 라이브러리의 상태 규칙과 JSON 프로토콜을 데스크톱 앱과 작은 WebSocket 서버가 공유한다. 서버가 행동 순서·최종 위치·랜덤 문구를 확정하고 두 클라이언트는 같은 결과를 표시한다. KDE Wayland 대응 코드는 유지하며 실제 Linux 검증은 macOS 완성본을 남자친구에게 보여준 뒤 수행한다.

**Tech Stack:** 검증 후보는 Qt 6.8 이상, QML/Qt Quick Controls, C++17, CMake 3.24 이상, Ninja, Qt WebSockets, Qt SQL/SQLite, Qt Test/Qt Quick Test다. Qt Widgets는 메뉴바·트레이의 키보드 진입점을 위한 QSystemTrayIcon에만 사용한다. Linux에서 LayerShellQt, 기기 자격증명 저장에 QtKeychain을 사용한다. 이는 작성자의 추천이며 OS 검증 전 확정된 동작을 보장하는 선택이 아니다.

**Spec:** `docs/superpowers/specs/2026-10-16-shared-pet-office-design.md` — 2026-10-16 사용자 승인.

## Global Constraints

- 첫 지원 목표는 사용자의 macOS와 남자친구의 EndeavourOS x86_64, KDE Plasma 6.7.5, KWin Wayland다. 전달받은 Linux 배율은 125%다.
- 컴퓨터마다 한 디스플레이의 하단 오버레이를 제공한다. 모든 전체 화면 앱 위 표시와 다중 디스플레이 이동은 초기 범위 밖이다.
- 한 번 친구와 연결하면, 이후 둘 다 앱을 켰을 때 자동으로 같은 세션에 접속한다.
- 한 세션의 참여자는 두 명이다. offline 발송 예약이나 다음 접속 때 전달하는 우편함은 만들지 않는다.
- busy 중 장난은 실행하지 않는다. 커피·에너지음료·쪽지는 옆에 조용히 쌓으며, busy 해제 후 자동 개봉하지 않는다.
- 밧줄 끌기와 간식으로 꼬시기는 도착한 자리를 새 위치로 삼는다. 현재 세션에서 유지하고 다른 행동으로 원위치 복귀시키지 않는다.
- 한 사건의 동작, 말풍선과 최종 위치는 두 화면에서 같다. 같은 상황의 직전 문구는 피한다.
- WIP, ping 보내기, busy, 세션을 유지한다. UI 전체를 함수 호출로 바꾸지 않는다.
- 작업창 포커스·키보드 접근성·움직임 줄이기·입력 검증·비공개 연결을 보존한다. 화면 내용이나 키 입력을 수집하지 않는다.
- lifecycle hook은 사용자가 정확한 정의를 검토하고 승인하기 전 설치·실행·신뢰하지 않는다. 초기 세팅에 Husky, lint-staged, agent hook을 넣지 않는다.
- 이 계획은 제품 구현 전 문서다. 리포는 leesomin7747/daemon-session으로 정하고 기획·목업·기본 개발 설정부터 보존한다. 의존성 설치와 배포는 아직 수행하지 않는다.

## Review Focus

1. 펫이 움직여 비워진 자리와 그 주변: 클릭이 뒤의 작업창으로 통과해야 한다. Task 1의 입력 영역 테스트와 실제 포커스 검사로 확인한다.
2. busy 전환과 장난 요청의 경합: 서버가 먼저 처리한 상태를 따르며, 거부한 장난이 재접속 후 재생되지 않아야 한다. Task 2·4에서 순서별 테스트한다.
3. 클릭 후 전달 전에 상대가 끊긴 경우: 보내기 실패를 표시하고 offline 우편으로 바꾸지 않아야 한다. Task 4·7의 연결 경합 테스트로 확인한다.
4. 서로 다른 배율과 작은 작업 영역: 같은 논리 위치를 표시하면서 펫·소품·메뉴가 화면 밖으로 사라지지 않아야 한다. Task 1·5에서 100%/125% 및 작업 영역 변경을 확인한다.
5. 한글·이모지·줄바꿈과 태그처럼 보이는 메모: 양쪽에서 같은 글자 수로 검증하고 일반 텍스트로 표시해야 한다. Task 2·6의 Unicode·plain-text 테스트로 확인한다.

## 리포명과 초기 설정 제안

사용자가 선택한 리포명은 `daemon-session`이며 원격 주소는 https://github.com/leesomin7747/daemon-session.git 이다. 리포명은 인원수를 제한하지 않지만, 첫 MVP는 승인 기획대로 두 사람을 대상으로 한다.

| 항목 | 추천 |
| --- | --- |
| 공개 범위 | 초기에는 Private |
| 기본 브랜치 | main |
| 구성 | 한 리포에 core/, desktop/, server/, docs/ |
| 개발 도구 | Qt Creator 또는 익숙한 편집기 + CMake/Ninja |
| 기본 설정 | UTF-8, LF, 들여쓰기 4칸, C++17, C++ format 파일 |
| 테스트 | Qt Test, Qt Quick Test, CTest; Task 1부터 빌드와 함께 구성 |
| 자동화 | 우선 수동 테스트 명령. 핵심 빌드가 안정된 뒤 최소 CI 추가 |
| Git 제외 | .superpowers/, .agents/skills/, build/, out/, 개인 CMake 설정, .env, 기기 토큰, 로컬 DB 및 인증서 개인키 |
| 문서 | README.md, 기존 AGENTS.md, 승인 기획서·계획서, 공개 가능한 승인 HTML 목업 |

프로젝트 라이선스 선택과 GitHub 소유자·리포 실제 생성은 이 추천과 별개다. 배포할 때 Qt 및 포함한 의존성의 라이선스 고지 파일을 함께 준비한다.

## 첫 검증과 완료 기준

2026-10-16 사용자 변경 승인: macOS 완성본을 먼저 만들어 남자친구에게 보여주고, KDE Wayland 실기 검증은 그 뒤로 미룬다. 사용자는 macOS에서 클릭이 잘 된다고 보고했다. 기존의 “양쪽 실제 GUI 통과 전 Task 2 금지” 조건은 해제한다. Task 1의 미확인 항목은 기록에 유지하고 제품 구현을 진행할 수 있다. macOS에서 남은 창 순서·Dock·작업 영역 검사는 제품 구현 중 계속 확인한다. 공개 전에는 Mac에서 두 클라이언트로 연결·동기화를 검증하고, Linux 지원 확정은 실제 KDE Wayland 검증 뒤로 둔다.

테스트용으로 X11/XWayland로 바꾼 결과를 KDE Wayland 네이티브 검증으로 기록하지 않는다. 남자친구에게 미리 실행이나 검증을 요청하지 않는다. Linux에서 차이가 드러나면 해당 플랫폼 창 처리와 패키징을 보완한다.

통과 기준은 투명 표시, 일반 작업창 위 유지, 펫만 클릭 가능, 빈 곳 클릭 통과, 수신 시 작업창 포커스 유지, 필요할 때만 글 입력 가능, 125% 배율에서 클릭 영역 일치다. 실패하면 실패 항목과 실제 화면 증거를 남겨 해당 플랫폼 창 처리만 수정한다. 해결되지 않으면 프레임워크 결정을 다시 검토하고 검증되지 않은 지원을 선언하지 않는다.

## 파일과 책임

현재 README, Git 제외 규칙, 편집·포맷 설정, 기획·계획 문서와 승인 목업을 보존한다. 아래의 실제 앱·서버·테스트 파일은 실행 단계에서 만든다.

| 위치 | 책임 |
| --- | --- |
| CMakeLists.txt, CMakePresets.json, .editorconfig, .clang-format, .gitignore, README.md | 빌드·개발·실행 기준 |
| probes/overlay/{CMakeLists.txt,main.cpp,Probe.qml} | 실제 두 OS에서 오버레이 가능성 검증 |
| core/{CMakeLists.txt,Protocol.h,Protocol.cpp,Session.h,Session.cpp,CopyCatalog.h,CopyCatalog.cpp} | 공통 명령·이벤트·상태 규칙·문구 선택 |
| core/copy/ko.json | 승인한 한국어 문구와 펫 말풍선 후보 |
| desktop/src/{main.cpp,OverlayWindow.h,OverlayWindow.cpp,SceneModel.h,SceneModel.cpp} | 창 처리와 QML에 노출할 장면 상태 |
| desktop/src/{SessionClient.h,SessionClient.cpp,CredentialStore.h,CredentialStore.cpp} | 서버 연결·자동 재접속·OS 비밀 저장소 |
| desktop/qml/{Main.qml,Pet.qml,ActionMenu.qml,WipPanel.qml,InboxPanel.qml,NoteComposer.qml,ConnectPanel.qml} | 승인한 펫과 메뉴·메모·쪽지 UI |
| server/src/{main.cpp,PairStore.h,PairStore.cpp,SessionBroker.h,SessionBroker.cpp} | 비공개 연결·인증·영속 페어·메모리 내 실시간 세션 |
| tests/{CMakeLists.txt,tst_overlaygeometry.cpp,tst_protocol.cpp,tst_session.cpp,tst_pairing.cpp,tst_broker.cpp,tst_credentials.cpp,tst_client.cpp} | C++ 동작 검증 |
| tests/qml/{tst_pet.qml,tst_panels.qml}, tests/integration/tst_twopeers.cpp | QML 및 두 클라이언트 통합 검증 |
| docs/mockups/shared-session.html, docs/verification/overlay.md, docs/verification/two-computers.md | 승인 기준과 실제 환경 검증 기록 |
| packaging/{CMakeLists.txt,README.md,daemon-session.desktop}, THIRD_PARTY_NOTICES.md | 설치 산출물과 배포 절차 |

## 공통 인터페이스와 초기 기본값

아래 이름은 모든 Task에서 동일하게 사용한다. public 타입은 Protocol.h와 Session.h에 정의한다.

- `PeerId = QString`; 역할명 a/b는 목업용이다. 서버가 발급한 식별자를 실제 앱에 사용한다.
- `PairId = QString`; `DeviceCredential { PeerId id; QByteArray token; }`. token은 32바이트이며 전송 시 base64url로 인코딩한다.
- `Action { Ping, Rope, Lure, Blanket, Plane, Coffee, Energy, Note, SetBusy, SetWip, AckInbox }`.
- `Command { QString requestId; Action action; PeerId targetId; QString text; bool busy; }`. actor는 인증된 연결에서 가져오고 클라이언트가 보낸 actor를 신뢰하지 않는다.
- `PeerState { PeerId id; bool reachable; bool busy; QString wip; double x; bool pinned; QVector<InboxItem> inbox; }`.
- `SessionState { QString epoch; quint64 revision; QMap<PeerId,PeerState> peers; }`.
- `SessionEvent { QString eventId; QString requestId; QString epoch; quint64 revision; Action action; PeerId actorId; PeerId targetId; QString caption; QString bubble; double finalX; bool queued; QString text; bool busy; }`. text는 쪽지/WIP 변경 내용 또는 busy 중 쌓인 선물의 실제 inbox 설명, busy는 대상의 확정된 상태이며 클라이언트가 이벤트만으로 변경을 재현할 수 있게 함께 전달한다. queued 선물·쪽지의 InboxItem은 eventId/action/text로 구성하면 snapshot과 같은 내용이 된다.
- `Transition { SessionState state; std::optional<SessionEvent> event; QString error; }`.
- `InboxItem { QString eventId; Action kind; QString text; }`; 쪽지와 busy 중 선물은 표시 가능한 항목으로 보존한다.
- `encodeCommand(const Command&) -> QByteArray`, `decodeCommand(const QByteArray&) -> std::optional<Command>`, `encodeEvent(const SessionEvent&) -> QByteArray`, `decodeEvent(const QByteArray&) -> std::optional<SessionEvent>`, `encodeSnapshot(const SessionState&) -> QByteArray`, `decodeSnapshot(const QByteArray&) -> std::optional<SessionState>`.
- `applyCommand(const SessionState&, const PeerId& actor, const Command&, CopyCatalog&, const std::function<int(int)>& choose) -> Transition`.
- `applyPresence(const SessionState&, const PeerId&, bool reachable) -> SessionState`; Broker만 호출하며 revision을 증가시킨다.
- `CopyCatalog::pick(const QString& key, const std::function<int(int)>& choose) -> QString`; 상태 안내와 말풍선에 별도 key를 쓰고 key별 직전 선택을 기억한다.
- `projectX(double x, double width, double petWidth, double margin) -> double`; x는 0~1의 사용 가능한 이동 구간 비율이다. 양끝의 margin과 petWidth를 제외한 구간에 투영한다.

작성자 선택: WIP는 80 Unicode code point, 쪽지는 120 code point다. 양쪽에서 trim 후 검증하며 빈 쪽지는 거부한다. WIP는 빈 문자열로 지울 수 있다. 문자열은 QML에서 Text.PlainText로 표시한다. 이동 연출은 약 2초로 하고 최종 x를 사건 수신 시 즉시 상태에 확정한다. 연출이 생략·중단되어도 최종 위치를 보존한다.

작성자 선택: 기기당 자동 접속 자격증명 하나, 페어당 최대 두 기기다. 초대는 32바이트 난수의 일회용 코드로 10분 유효하며, 발급자가 상대 참여를 승인한 뒤 연결한다. 기기 토큰도 32바이트 난수이며 서버는 해시만, 클라이언트는 OS 비밀 저장소에만 보관한다. 연결 해제 시 해당 페어 권한과 초대를 폐기한다.

작성자 선택: 네트워크 heartbeat는 10초, 마지막 응답 후 30초에 도달하면 연결 불능으로 처리한다. 소켓 종료는 즉시 reachable=false로 반영하며 15초 동안 퇴장 농담을 유예한다. 실제 전송은 reachable일 때만 허용한다. 양쪽이 모두 끊기고 유예가 끝나면 메모리 세션을 정리한다. 중간 재접속은 snapshot으로 복구하고 지난 수신 연출을 재생하지 않는다. 이미 받은 쪽지는 앱 실행 중 유지하고 확인하면 정리한다. 쪽지·장난 본문을 서버 DB나 로그에 보관하지 않는다.

서버는 pair 정보와 토큰 해시만 SQLite에 저장한다. 캐릭터 위치, busy와 미확인 항목은 세션 메모리에 있다. 웹 로그인·OAuth·Redis·큐 서비스는 초기 구성에 필요하지 않다. 외부 배포 연결은 WSS이며 개발용 평문 WS는 loopback 주소에 한정한다.

전송 봉투는 `{ "v": 1, "op": "...", "requestId": "...", "body": {} }`다. control 요청은 device.register, auth, invite.create, invite.join, invite.approve, pair.revoke, session.snapshot이고, 행동은 session.command로 보낸다. device.register만 미인증 새 기기에 허용하며 반환한 자격증명으로 소켓을 인증한다. 기존 기기는 연결 후 5초 이내 auth를 보내야 한다. 초대·페어 요청은 인증된 actor만 수행한다. 응답은 동일 requestId의 result 또는 error, 실시간 갱신은 session.event 또는 session.snapshot이다. 초대 발급자에게 join.requested를 보내 승인받는다. 접속 안내는 서버에서 고른 단일 문자열을 session.notice로 양쪽에 보낸다. codec은 body를 직렬화하고 봉투의 버전·operation·크기 검증은 Broker/Client가 함께 사용한다.

Task 2 구현 결정: revision의 JSON 표현은 정밀도 손실을 막는 10진 문자열이며 public C++ 타입은 quint64를 유지한다. 식별자는 비어 있지 않은 최대 128 code point의 제어문자 없는 문자열이다. 모든 codec은 알려진 필드·타입만 허용하며 actor는 command 본문에서 받지 않는다. snapshot은 peers 배열을 사용하고 중복 멤버/받은 항목 ID를 거부한다. 쪽지·WIP는 trim 후 Unicode code point 제한을 적용하고 HTML처럼 보이는 내용도 문자열 그대로 유지한다.

Task 2 구현 결정: 상태가 가득 차면 기존 항목을 버리지 않고 새 요청을 session_full 오류로 거부한다. 16KiB 한도 안에 바깥 통신 봉투용 1KiB와 상태 플래그·revision 증가용 64바이트를 남긴다. 받은 항목 확인은 자기 inbox 전체를 직접 비우는 동작이다. 상대가 offline이어도 online인 사용자의 자기 WIP·busy·확인은 허용하며, 상대에게 보내는 행동은 두 멤버 모두 online이어야 한다. Rope/Lure의 정규화 간격은 각각 0.12/0.16으로 시작하고 화면 투영은 공통 projectX를 사용한다.

---

### Task 1: 실제 OS 오버레이 검증과 실행 가능한 개발 기준

**Files:** Create root 개발 설정, probes/overlay/*, desktop/src/OverlayWindow.{h,cpp}, tests/tst_overlaygeometry.cpp, docs/verification/overlay.md, docs/mockups/shared-session.html. 기존 AGENTS.md와 docs/superpowers/*를 보존한다.

**Interfaces:** `OverlayWindow(QScreen*, QObject* parent=nullptr)`; `setInteractiveRegions(const QVector<QRectF>&) -> void`; `setEditorActive(bool) -> void`; `projectX(...) -> double`. 후속 Task는 창을 직접 다시 구현하지 않고 이 API를 사용한다.

- [ ] **1. 실패할 기하 테스트를 쓴다.** `tst_overlaygeometry::clickRegionFollowsPet`는 이동 전 영역이 빠지고 새 영역만 포함되는지, `boundsAt125Percent`는 1.25 배율에서도 논리 좌표의 클릭 영역과 시각 위치가 일치하는지, `projectXStaysInBounds`는 x=0/1에서 margin 밖으로 나가지 않는지 검증한다.
- [ ] **2. CMake/CTest 최소 구성을 추가하고 테스트를 실행한다.** `cmake -S . -B build -G Ninja -DBUILD_TESTING=ON`, `cmake --build build`, `ctest --test-dir build -R overlaygeometry --output-on-failure`. 최초에는 함수 미구현으로 실패해야 한다. Qt가 없으면 이를 먼저 보고하고 공식 패키지로 설치하며 임의 다운로드 스크립트를 실행하지 않는다.
- [ ] **3. 최소 probe와 창 구현을 만든다.** 투명 QQuickWindow와 동적으로 갱신되는 QWindow::setMask 영역을 사용한다. macOS는 비활성 표시 창에서 시작하고, Linux는 QWindow 생성 후 표시 전에 LayerShellQt::Window::get으로 Bottom/Left/Right 앵커·LayerTop·exclusive zone 0·KeyboardInteractivityNone을 설정한다. 편집을 직접 연 경우에만 OnDemand 등 입력 가능한 모드로 전환한다. useLayerShell()은 현재 소스에서 deprecated이므로 새 코드에 넣지 않는다. setMask는 플랫폼 hint이므로 실제 클릭 통과를 반드시 확인한다.
- [ ] **4. 실제 GUI 검증 결과를 기록한다.** 펫 클릭, 빈 곳 클릭, 자동 수신 중 타 앱 타이핑 유지, WIP 입력 후 타 앱으로 복귀, 100%/125%, Dock·패널 주변, 작업 영역 변경을 검사한다. 실제 OS/Qt/LayerShellQt 버전과 캡처를 overlay.md에 기록하고 실패 항목을 남긴다. 사용자 승인에 따라 Linux 실기 부분은 완성본을 보여준 뒤 Task 8에서 수행하며, 이 미실행 상태는 다음 Task를 막지 않는다.
- [ ] **5. 개발 기준을 기존 리포에 보존한다.** 기존 .gitignore와 docs/mockups/shared-session.html을 유지하고 README에 실제로 검증된 설치·빌드·실행 명령을 추가한다. 로컬 저장소와 origin이 이미 있으므로 Git을 다시 초기화하지 않는다. `git add README.md CMakeLists.txt CMakePresets.json probes desktop tests docs/verification` 후 `git commit -m "chore: establish verified desktop overlay baseline"`한다. lifecycle hook은 실행하지 않는다.

### Task 2: 공통 상태 규칙과 문구 동기화

**Files:** Create core/*, core/copy/ko.json, tests/tst_protocol.cpp, tests/tst_session.cpp. Task 1의 projectX 선언·구현은 OverlayWindow에서 중복하지 않고 core/Session 쪽으로 이동해 기존 테스트를 유지한다.

**Interfaces:** 앞 절의 모든 공통 타입과 codec, applyCommand, CopyCatalog를 제공한다. UI나 서버 저장소에 의존하지 않는 라이브러리로 만든다.

- [x] **1. 실패 테스트를 작성한다.** `busyRejectsPrankWithoutQueue`, `busyQueuesGiftAndNoteSilently`, `unbusyDoesNotAutoOpen`, `offlineRejectsNote`, `onlyOwnerChangesWip`, `ropeAndLurePinDestination`, `otherActionKeepsPinnedPosition`, `copyAvoidsImmediateRepeat`, `sameEventHasSameBubble`, `unicodeLimitsAndPlainText`, `invalidFrameIsRejected`를 만든다. 핵심 단언은 error/queued 값, inbox 크기, 두 번 적용한 위치 불변, WIP 80/81 및 쪽지 120/121 code point의 경계다.
- [x] **2. `cmake --build build`와 `ctest --test-dir build -R 'protocol|session' --output-on-failure`를 실행해 미구현으로 실패하는지 확인한다.**
- [x] **3. 타입·codec·reducer를 구현한다.** actor와 두 멤버의 reachable을 검증하고, 상태 변경은 revision 순서로 한다. 이동 목적지는 상대와 화면 경계 안의 가까운 자리로 확정하고 pinned=true로 남긴다. busy 이후 들어온 장난은 event 없이 거부한다. 랜덤 선택 결과인 caption과 bubble은 이벤트에 담아 송신한다. ko.json에는 승인한 커피·에너지음료 말풍선 각각 5개를 그대로 포함한다. 최대 프레임 16KiB, 알려진 action만 허용하고 NaN·범위 밖 좌표·잘못된 타입을 거부한다.
- [x] **4. 같은 테스트 명령으로 전부 통과시키고, busy 전후 두 명령의 순서를 바꿔 실행한 테스트도 통과시킨다.**
- [x] **5. `git add core tests CMakeLists.txt desktop/src/OverlayWindow.*` 후 `git commit -m "feat: define shared session rules and dialogue events"`한다.**

### Task 3: 서로 동의한 일대일 페어와 영속 인증

**Files:** Create server/src/PairStore.{h,cpp}, tests/tst_pairing.cpp; add server/CMakeLists.txt.

**Interfaces:** `PairStore(const QString& dbPath)`; `createDevice() -> DeviceCredential`; `createInvite(const PeerId&, QDateTime now) -> QString`; `requestJoin(const QString& invite, const PeerId&, QDateTime now) -> QString`; `approveJoin(const PeerId& inviter, const QString& requestId, QDateTime now=QDateTime::currentDateTimeUtc()) -> PairId`; `authenticate(const QByteArray& token) -> std::optional<PeerId>`; `pairForPeer(const PeerId&) -> std::optional<PairId>`; `members(const PairId&) -> QVector<PeerId>`; `inviterForRequest(const QString& requestId, QDateTime now) -> std::optional<PeerId>`; `revokePair(const PeerId&) -> void`. DeviceCredential은 id와 최초 한 번 반환할 token을 담는다. 저장소 장애는 비밀 값 없는 예외로, 정상 거절은 빈 결과로 구별한다.

Task 3 구현 결정: 승인에도 동일한 10분 만료를 적용하도록 호환 가능한 기본 시간 인자를 추가한다. Broker의 clock을 주입할 때 생성·요청·승인 모두 같은 clock 값을 전달한다. inviterForRequest는 Broker가 join.requested를 발급자에게 보낼 수 있게 하는 조회다. 발급자당 하나의 현재 초대·신청자당 하나의 대기 요청만 두며 새 초대는 이전 코드와 대기를 취소한다. 연결 해제는 페어 접근 권한과 관련 초대를 제거하되 기기 식별 토큰은 유지한다. Broker는 해제 직전에 멤버를 조회하고 연결 해제 후 두 소켓의 세션 권한·캐시를 즉시 폐기해야 한다.

- [x] **1. `tst_pairing`에 영속 재접속, 10분 만료, 사용한 코드 재사용 거부, 발급자 승인 전 페어 미성립, 셋째 기기 거부, 다른 기기의 승인 거부, 연결 해제 후 권한 폐기, DB에 원문 token/invite 없음 테스트를 작성한다.**
- [x] **2. `ctest --test-dir build -R pairing --output-on-failure`를 실행해 실패를 확인한다.**
- [x] **3. Qt SQL의 prepared statement와 트랜잭션으로 PairStore를 구현한다.** 토큰·초대는 시스템 난수로 32바이트 생성하고 해시를 저장한다. 요청·승인의 인증과 페어 최대 인원 제약을 DB 트랜잭션으로 보장한다. DB 열기 실패 시 인증을 우회하지 않고 시작 실패를 반환한다.
- [x] **4. 테스트 DB를 닫고 다시 열어도 authenticate와 pair 조회가 유지되는지 포함해 같은 테스트를 통과시킨다.**
- [x] **5. `git add server tests CMakeLists.txt` 후 `git commit -m "feat: persist private two-device pairing"`한다.**

### Task 4: WebSocket 서버와 단일 순서의 세션 이벤트

**Files:** Create server/src/{main.cpp,SessionBroker.h,SessionBroker.cpp}, core/WireProtocol.{h,cpp}, tests/tst_broker.cpp, docs/verification/websocket.md. Modify PairStore for a pending request lookup and the common event codec's busy combination validation.

**Interfaces:** `SessionBroker(PairStore&, QObject* parent=nullptr)`; `SessionBroker(PairStore&, BrokerClock, QObject* parent=nullptr)`; `listen(const QHostAddress&, quint16) -> bool`; `serverPort() -> quint16`; `close() -> void`. BrokerClock은 monotonicMs/utcNow 함수를 주입한다. `WireFrame { QString op; QString requestId; QJsonObject body; }`, `encodeFrame/decodeFrame`으로 봉투를 공통 검증한다. `PairStore::pendingJoinForInviter(const PeerId&, QDateTime) -> std::optional<PendingJoin>`은 아직 유효한 requestId/applicantId를 반환한다. 인증된 WebSocket에 공통 Command를 받아 SessionEvent와 Snapshot을 송신한다. 시작 시 localhost:8787을 기본값으로 한다.

Task 4 구현 결정: 평문 WS는 loopback에만 바인딩하며 브라우저 Origin·query credential을 거부한다. 동일 기기의 새 인증 소켓은 기존 권한을 먼저 폐기하고 대체한다. 기기당 초당 10/burst 20은 control을 포함한 새 요청에 적용하고, 별도의 연결 프레임 한도(초당 20/burst 40)는 재시도 flood도 제한한다. 정상 거절 결과도 최근 256개 캐시에 포함하며 같은 ID의 다른 본문은 request_conflict다. 연결 해제·저장소 오류는 권한을 먼저 폐기한 뒤 응답과 close를 내보내고 최대 1초 후 강제 종료한다. 대기 승인은 발급자 재인증 시 DB에서 복구한다. 외부 TLS 배포는 Task 8, 제품 UI·Client는 후속 Task 범위로 유지한다.

- [x] **1. loopback의 두 실제 QWebSocket으로 실패 테스트를 작성한다.** `controlFramesEstablishMutuallyApprovedPair`, `authTimeoutClosesSocket`, `bothPeersReceiveIdenticalEvent`, `busyRaceRespectsServerOrder`, `disconnectBeforeNoteRejectsWithoutReplay`, `duplicateRequestDoesNotRepeat`, `reconnectGetsSnapshotNotOldAnimation`, `unknownPeerCannotJoin`, `oversizedFrameAndFloodAreBounded`를 검증한다. 동일 이벤트의 eventId/revision/bubble/finalX가 같아야 한다.
- [x] **2. `ctest --test-dir build -R broker --output-on-failure`에서 실패를 확인한다.**
- [x] **3. Broker를 구현한다.** 위의 control 봉투를 PairStore에 연결하고 actor는 인증 연결에 묶는다. 등록·초대 생성도 IP별 분당 10회로 제한한다. 페어당 하나의 reducer 순서를 사용하며 마지막 256개의 `(epoch, actor, requestId)` 결과를 캐시해 재시도에 같은 응답을 반환한다. applyPresence로 끊긴 소켓의 reachable은 즉시 false, 퇴장 문구는 15초 유예한다. 재접속에 snapshot만 제공하며 offline 명령은 보관하지 않는다. 기기별 초당 10명령 burst 20, inbox 최대 100개에서 명확한 거절 응답, 송신 버퍼 64KiB 초과 시 연결 종료로 메모리를 제한한다. inbox 정리는 AckInbox로 하고 본문을 로그에 남기지 않는다.
- [x] **4. heartbeat·유예는 주입 가능한 clock으로 테스트하고 같은 명령으로 전부 통과시킨다.**
- [x] **5. `git add server tests` 후 `git commit -m "feat: synchronize two peers through ordered websocket events"`한다.**

### Task 5: 승인한 펫 장면과 짧은 행동 연출

**Files:** Create desktop/src/{main.cpp,SceneModel.h,SceneModel.cpp}, desktop/qml/{Main.qml,Pet.qml,Prop.qml,ActionMenu.qml}, tests/qml/tst_pet.qml, tests/{tst_scenemodel.cpp,tst_pet.cpp,tst_petwindow.cpp}, desktop/CMakeLists.txt, docs/verification/pet-scene.md. Modify desktop/src/OverlayWindow.*.

**Interfaces:** `SceneModel::replaceSnapshot(const SessionState&) -> void`; `applyEvent(const SessionEvent&) -> void`; QML용 peers와 lastEvent properties; `actionRequested(Action, PeerId, QString text, bool busy)` signal. 이 Task에서는 고정 fixture로 로컬 체험한다. SceneModel은 eventAboutToApply/sceneReset으로 연출을 취소·복구하고, snapshotRequired로 잘못된 순서의 복구를 요청한다. requestAction은 상태 변경 없이 검증한 의도만 actionRequested로 내보낸다. OverlayWindow::setContentHeight(int)는 기존 probe의 180px 기본값을 유지하면서 제품 장면 높이를 380px로 설정한다. 입력 mask는 실제 움직이는 아바타·책상·선물·메뉴의 논리 좌표를 따른다. 움직임 줄이기는 메뉴와 CLI에서 명시적으로 설정하며 OS 설정 자동 연동은 아직 없다.

- [ ] **1. `tst_pet.qml`에 두 장면의 동일 이벤트 표시, 각각의 최종 위치 유지, 다른 행동 뒤에도 위치 불변, 연출 중 snapshot 복구, 줄인 움직임에서도 최종 위치 표시, 화면 경계와 소품 클릭 영역 테스트를 작성한다.**
- [ ] **2. `ctest --test-dir build -R pet --output-on-failure`로 실패를 확인한다.**
- [ ] **3. 목업을 QML 장면으로 옮긴다.** 펫의 색·크기·분위기와 짧은 연출을 보존하고 product UI에 코드 편집기 배경이나 목업 송신자 선택기를 넣지 않는다. 이벤트가 정한 말풍선을 표시한다. pinned인 펫은 새 이동 이벤트까지 자리를 지킨다. Pet의 시각 영역 변화와 OverlayWindow의 입력 mask를 함께 갱신한다. busy 펫은 현재 자리에서 안경·책상 모습이 된다. 로프·간식·비행기의 시작과 끝은 현재 펫 위치를 따른다.
- [ ] **4. 테스트를 통과시키고 실제 OS에서 수신 연출 중 타 앱 타이핑이 유지되는지 확인한다.**
- [ ] **5. `git add desktop tests` 후 `git commit -m "feat: render synchronized pet actions in the desktop overlay"`한다.**

### Task 6: WIP, 쪽지, busy와 받은 항목 패널

**Files:** Create desktop/qml/{WipPanel.qml,InboxPanel.qml,NoteComposer.qml}, tests/qml/tst_panels.qml. Modify SceneModel.*, Main.qml, ActionMenu.qml.

**Interfaces:** Task 5의 actionRequested를 사용한다. 자기 WIP 저장은 SetWip, busy는 SetBusy, 쪽지는 Note, 확인은 AckInbox를 요청한다. 서버 이벤트 전 UI를 수신 성공 상태로 확정하지 않는다.

- [ ] **1. `tst_panels.qml`에 자기 WIP만 편집 가능, 빈 WIP 삭제, 쪽지 빈 문자열 거부, 80/120 code point 경계, `<b>과제</b>`가 태그 아닌 글자로 표시, busy 중 새 항목이 패널을 자동으로 열지 않음, busy 해제도 자동 개봉하지 않음, offline 전송 비활성, 키보드로 열기·닫기·포커스 복귀를 테스트한다.**
- [ ] **2. `ctest --test-dir build -R panels --output-on-failure`로 실패를 확인한다.**
- [ ] **3. 패널과 입력을 구현한다.** 기존 개발 감성을 유지하고 WIP·busy·ping 표현을 쓴다. 문자열은 plain text이며 trim과 code point 계산을 공통 Protocol 검증에 맞춘다. QApplication과 QSystemTrayIcon의 ‘펫 메뉴 열기’를 키보드 진입점으로 제공한다. 실제 메뉴·입력을 열었을 때만 편집 포커스를 받고 Escape/닫기로 이전 상태에 복귀한다. 선물·쪽지 개수와 확인 동작을 제공하고 기본 소리는 넣지 않는다. 서버가 보낸 상황 문구를 표시한다.
- [ ] **4. 테스트와 실제 한글 입력기 조합 입력을 확인한다.** 입력 도중 저장이 조기 실행되거나 한글이 잘리지 않는지 macOS에서 검사한다. Linux 실제 입력기 검사는 완성본을 보여준 뒤 Task 8에서 반복한다.
- [ ] **5. `git add desktop tests` 후 `git commit -m "feat: add shared WIP notes and quiet busy inbox"`한다.**

### Task 7: 기기 연결 UI, 안전한 자동 접속과 복구

**Files:** Create desktop/src/{SessionClient.h,SessionClient.cpp,CredentialStore.h,CredentialStore.cpp}, desktop/qml/ConnectPanel.qml, tests/{tst_credentials.cpp,tst_client.cpp}. Modify main.cpp, SceneModel.*, Main.qml, desktop/CMakeLists.txt.

**Interfaces:** `CredentialStore::load() -> void`, `save(const DeviceCredential&) -> void`, `clear() -> void`와 완료/오류 signals; `SessionClient::connectTo(const QUrl&) -> void`, `send(const Command&) -> void`, `requestControl(const QString& operation, const QJsonObject& body) -> QString`, `disconnectAndForget() -> void`, snapshotReceived/eventReceived/noticeReceived/connectionChanged/controlResult/error signals. requestControl은 requestId를 반환하고 controlResult는 그 id와 response body를 전달한다.

- [ ] **1. 가짜 secret backend와 loopback 서버로 실패 테스트를 작성한다.** `credentialRoundTrip`, `secretStoreFailureHasNoPlaintextFallback`, `relaunchReusesPairedIdentity`, `staleSocketCannotSend`, `duplicateEventAppliedOnce`, `revisionGapRequestsSnapshot`, `badTlsCertificateIsNotIgnored`, `unpairStopsReconnect`, `localPetRemainsOptionalOffline`를 검증한다.
- [ ] **2. `ctest --test-dir build -R 'credentials|client' --output-on-failure`로 실패를 확인한다.**
- [ ] **3. QtKeychain 기반 CredentialStore와 SessionClient를 구현한다.** 비밀 저장소에 실패하면 재시도 가능한 오류를 표시하고 QSettings·파일에 토큰을 평문으로 저장하지 않는다. non-secret 설정만 QSettings를 사용한다. 첫 연결은 코드 만들기·입력·발급자의 승인으로 구성한다. 재접속은 1/2/4/8/15초 상한과 jitter로 하며, epoch와 revision으로 오래되거나 중복된 이벤트를 버린다. URL query·로그에 토큰을 넣지 않는다. 프로덕션은 WSS만 허용하고 TLS 검증을 우회하지 않는다.
- [ ] **4. 공통 codec→Broker→Client→SceneModel을 연결해 두 클라이언트 테스트를 통과시킨다.** 수신 전에 끊긴 쪽지는 실패로 표시하고 자동 재발송하지 않는다. offline 내 펫 표시 설정은 기기에 저장한다.
- [ ] **5. `git add desktop tests` 후 `git commit -m "feat: connect paired desktops with secure automatic reconnect"`한다.**

### Task 8: 실제 두 컴퓨터 사용과 설치 산출물

**Files:** Create tests/integration/tst_twopeers.cpp, packaging/*, THIRD_PARTY_NOTICES.md, docs/verification/two-computers.md. Modify root CMakeLists.txt, README.md, docs/superpowers/specs의 목업 참조를 docs/mockups/shared-session.html로 갱신한다.

**Interfaces:** 기존 desktop/server executable과 모든 공통 API를 사용한다. `daemon-session` 앱과 `daemon-session-server` 실행 산출물을 만든다.

- [ ] **1. 두 클라이언트의 전체 시나리오 테스트를 작성한다.** 첫 연결·동시 접속·WIP·busy·선물과 쪽지 대기·같은 랜덤 말풍선·양방향 이동 후 위치 유지·offline 거부·재접속 중복 없음·연결 해제를 한 흐름에서 검증한다.
- [ ] **2. `ctest --test-dir build -R twopeers --output-on-failure`를 실행하고 통합에서 드러난 실패만 수정한다.**
- [ ] **3. Mac 완성본을 검증하고 공개 후 Linux 실기를 수행한다.** `cmake --build build`, `ctest --test-dir build --output-on-failure` 후 macOS에서 두 클라이언트로 같은 시나리오를 수행한다. 30분 동안 idle/busy/상호작용 상태의 CPU·메모리 변화, 포커스 방해와 배율 클릭 불일치를 기록한다. Mac 완성본을 남자친구에게 보여준 뒤 실제 KDE Wayland 125%에서 오버레이·한글 조합 입력·같은 시나리오를 반복하고 필요한 플랫폼 수정을 한다. 컴파일 성공을 실제 Wayland 동작 통과로 대신하지 않는다.
- [ ] **4. 설치 산출물과 실행 문서를 준비한다.** macOS .app에 필요한 Qt 라이브러리를 포함하고 Linux는 우선 정확한 EndeavourOS 환경에서 실행할 바이너리·의존성 목록·desktop entry를 제공한다. 자동 업데이트는 후속 작업으로 둔다. Mac 서명·공증 자격증명과 서버 호스트가 없으면 미완료 항목으로 명시하며 우회 설정을 권하지 않는다. 외부 서버는 TLS 종료와 지속 SQLite 저장소가 있는 단일 프로세스로 배포할 수 있게 문서화하되 호스트·비용 결정 없이 배포하지 않는다.
- [ ] **5. 최종 사용 증거와 의존성 고지를 검토하고 `git add packaging THIRD_PARTY_NOTICES.md README.md docs tests CMakeLists.txt` 후 `git commit -m "feat: validate and package the first paired desktop release"`한다.**

## 계획 자체 검토

기획의 플랫폼·세션·하단 표시·장난·선물·busy·WIP·쪽지·랜덤 문구·이동 위치 유지·비목표를 위 Task에 대응시켰다. Review Focus 다섯 항목은 각각 해당 Task의 테스트와 실제 환경 검사에 포함했다. 공통 타입·public API 이름은 앞 절을 단일 기준으로 사용한다. GUI가 없는 테스트 환경에서는 Qt Quick Test의 offscreen 결과와 실제 오버레이 증거를 구별한다.

기술 후보와 상세 초기값은 이 계획에서 제안한 구현 선택이다. 제품 경험을 바꾸는 결정이 필요하면 기획서에 되돌아가 확인하고, 라이브러리·플랫폼의 작은 차이는 검증 결과로 해결한다.

## 공식 참고 자료

- [Qt QWindow의 mask와 창 옵션](https://doc.qt.io/qt-6/qwindow.html): 입력 mask는 플랫폼 hint이므로 실제 확인이 필요하다.
- [KDE LayerShellQt](https://github.com/KDE/layer-shell-qt): Qt 창에서 Wayland Layer Shell을 사용하는 연동 후보다.
- [LayerShellQt Window API](https://github.com/KDE/layer-shell-qt/blob/master/src/interfaces/window.h): 앵커·레이어·입력 모드·exclusive zone 설정.
- [LayerShellQt Shell API](https://github.com/KDE/layer-shell-qt/blob/master/src/interfaces/shell.h): useLayerShell()의 deprecated 상태.
- [Qt WebSockets](https://doc.qt.io/qt-6/qwebsocket.html): Qt 클라이언트·서버의 WebSocket 통신.
- [QtKeychain](https://github.com/frankosterfeld/qtkeychain): macOS Keychain 및 Linux 비밀 저장소 지원.
