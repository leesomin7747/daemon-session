# daemon-session

각자 작업하는 동안, 데몬과의 상호작용으로 함께 있는 느낌을 나누는 데스크톱 공간.

A desktop space where interactions with daemons make working apart feel like working together.

## 현재 단계

네이티브 오버레이 검증용 앱, 공통 상태 규칙·JSON codec, SQLite 기반 비공개 페어링·기기 인증과 로컬 WebSocket 서버를 구현했습니다. 실제 소켓으로 두 기기의 인증·승인·실시간 동기화·재접속을 검증합니다. QML 펫 장면은 로컬 미리보기로 실행할 수 있으며, 제품 UI와 네트워크의 연결 및 KDE Wayland 실기 검증은 남아 있습니다.

첫 MVP는 두 사람의 비공개 세션을 대상으로 하며, 더 많은 데몬을 위한 확장은 이후에 다룹니다. 첫 지원 목표는 macOS와 KDE Plasma의 Wayland 환경입니다.

## 함께 하는 일

- 작업창 아래에 머무는 펫과 같은 장면의 장난
- 커피와 에너지음료, 짧은 쪽지
- 지금 하는 일을 공유하는 WIP
- 집중을 존중하고 선물과 쪽지를 조용히 쌓는 busy
- 상황에 맞게 바뀌는 짧고 다정한 문구

## 기획과 목업

- [승인 기획서](docs/superpowers/specs/2026-10-16-shared-pet-office-design.md)
- [구현 계획](docs/superpowers/plans/2026-10-16-shared-pet-office-implementation.md)
- [승인 인터랙션 목업](docs/mockups/shared-session.html)

목업 HTML을 내려받아 브라우저에서 열면 두 사용자의 화면을 한 탭에서 체험할 수 있습니다. 실제 온라인 연결이나 OS 오버레이를 제공하는 파일은 아닙니다.

## 개발 방향

Qt 6, QML/C++, CMake와 KDE LayerShellQt 연동을 초기 기술 후보로 검토합니다. 사용자 결정에 따라 macOS에서 먼저 제품을 완성하고, 완성본을 보여준 뒤 KDE Wayland 실기 검증을 진행합니다. Linux 대응 코드는 유지하되 실제 검증 전에는 Linux 지원을 확정하지 않습니다. 남은 macOS 창 처리 검사는 제품 구현 중 이어갑니다.

## 오버레이 probe 실행

macOS에서 확인한 환경은 macOS 26.4 arm64, Qt 6.11.2, CMake 4.4.4, Ninja 1.13.2입니다. Apple Command Line Tools와 Homebrew가 설치된 상태에서:

```sh
brew install qtbase qtdeclarative qtwebsockets cmake ninja
cmake --preset dev -DCMAKE_PREFIX_PATH="$(brew --prefix)"
cmake --build --preset dev
ctest --preset dev
open build/probes/overlay/overlay_probe.app
```

EndeavourOS/KDE Wayland용 구성은 아래와 같습니다. **아직 해당 컴퓨터에서 빌드·실행하지 않은 절차**입니다. 시스템의 패키지 버전을 일관되게 유지한 상태에서 실행하세요.

```sh
sudo pacman -S --needed base-devel cmake ninja qt6-base qt6-declarative qt6-websockets qt6-wayland layer-shell-qt
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
QT_QPA_PLATFORM=wayland ./build/probes/overlay/overlay_probe
```

probe에는 이동 가능한 두 테스트 캐릭터, 뒤 창 클릭 카운터, 3초마다 자동 ping, WIP 입력창이 있습니다. 네트워크 연결 없이 OS 창 동작만 검증합니다. 상세 절차와 항목별 실제 결과는 [오버레이 검증 기록](docs/verification/overlay.md)에 있습니다. 테스트의 `QT_SCALE_FACTOR=1.25` 결과는 실제 KWin 125% 검증을 대체하지 않습니다.

작업 규칙은 [AGENTS.md](AGENTS.md)에 있습니다. lifecycle hook은 정확한 정의에 대한 사용자 검토·승인 없이 사용하지 않습니다.

## 공통 세션 규칙

`daemon_core`는 Qt Core만 사용하는 라이브러리입니다. `core/Session.h`의 applyCommand가 인증된 actor와 명령을 받아 새 상태·확정 이벤트 또는 오류를 반환합니다. busy 중 장난은 거부하고 커피·에너지음료·쪽지는 조용히 쌓습니다. busy를 풀어도 자동 개봉하지 않으며, 받은 항목은 자기 자신만 확인할 수 있습니다. Rope/Lure 목적지는 세션에 유지하고 이후 행동이 원위치로 되돌리지 않습니다.

`core/Protocol.h`는 명령·이벤트·snapshot의 JSON 본문 codec입니다. 잘못된 타입·행동·좌표·식별자를 거부하고 WIP 80 / 쪽지 120 Unicode code point 제한을 적용합니다. text는 일반 문자열로 보존하며 향후 UI에서도 PlainText로 표시해야 합니다. 공통 전송 봉투는 `core/WireProtocol.h`, 인증된 연결과 행동 순서는 `server/src/SessionBroker.h`가 담당합니다.

승인 문구는 `core/copy/ko.json`에 포함되며 `CopyCatalog`가 상황별 직전 문구를 피해서 고릅니다. 결과를 이벤트에 담으므로 클라이언트가 따로 문구를 추첨할 필요가 없습니다. 받은 쪽지는 서버 DB나 로그에 쓰지 않고 세션 메모리에만 둡니다. 전송 크기를 넘길 만큼 쌓이면 새 요청을 명시적으로 거부하며 기존 항목을 버리지 않습니다.

공통 규칙만 검증하려면 `ctest --test-dir build -R 'protocol|session' --output-on-failure`, 오버레이 회귀 검사를 포함하려면 `ctest --preset dev`를 실행하세요.

## 비공개 페어링과 기기 인증

`server/src/PairStore.h`는 Qt SQL/SQLite를 사용하는 서버 측 저장소입니다. 기기 등록 시 32바이트의 시스템 난수 토큰을 최초 한 번 반환하며, DB에는 SHA-256 해시만 저장합니다. 초대도 32바이트 난수를 URL-safe 코드로 표현하고 해시만 보관합니다. 코드는 10분 유효하며 참여 요청이 접수되면 재사용할 수 없습니다. 실제 페어는 발급자가 승인해야 만들어지고, 승인 시점에도 만료를 다시 확인합니다.

모든 쓰기는 prepared statement와 `BEGIN IMMEDIATE` 트랜잭션으로 처리합니다. DB 제약이 기기당 한 페어·페어당 두 슬롯을 보장하며, 실패한 승인은 부분 연결을 남기지 않습니다. 기기 인증과 페어 정보는 DB를 다시 열어도 유지됩니다. 저장소 열기·스키마·SQL 오류는 예외로 처리하며 임시 메모리 인증으로 우회하지 않습니다. 파일 DB는 소유자만 읽고 쓸 수 있게 만듭니다.

연결 해제는 양쪽의 페어 멤버십과 관련 초대를 폐기합니다. 기기 자체의 인증 자격은 유지되지만 예전 세션에 접근할 권한은 사라지며, 다시 연결하려면 새 초대와 승인이 필요합니다. Broker는 인증된 소켓의 PeerId를 사용하고 해제 시 두 소켓의 권한과 세션 캐시를 즉시 폐기합니다. 클라이언트 비밀 저장소와 승인 UI는 후속 단계입니다.

정상적인 거절은 빈 문자열/optional로, 저장소 장애는 비밀 값을 포함하지 않는 `std::runtime_error`로 전달합니다. `approveJoin`의 시간 인자는 테스트/서버 clock을 전달할 수 있고 생략하면 현재 UTC 시각을 사용합니다. `inviterForRequest`는 후속 Broker가 참여 요청을 정확한 발급자에게 전달할 때 사용하는 만료 검사 포함 조회입니다.

`ctest --test-dir build -R pairing --output-on-failure`로 실제 임시 SQLite DB를 이용한 인증·승인·만료·경합·트랜잭션 실패 복구를 검증합니다. 난수와 트랜잭션 선택은 [Qt 시스템 난수 문서](https://doc.qt.io/qt-6/qrandomgenerator.html#system), [SQLite 트랜잭션 문서](https://www.sqlite.org/lang_transaction.html)를 기준으로 합니다.

## 로컬 WebSocket 서버 실행

```sh
cmake --build --preset dev
./build/server/daemon-session-server
```

기본 주소는 `ws://127.0.0.1:8787/`입니다. `--db /절대경로/pairing.sqlite --port 8788`로 저장 위치와 포트를 지정할 수 있습니다. 기본 DB는 OS의 앱 데이터 폴더에 만듭니다. 개발용 서버는 loopback만 허용하며 인터넷에 배포하지 않았습니다. 외부 연결은 이후 TLS 종료·호스트 설정을 준비한 뒤 WSS로 제공합니다.

인증은 연결 후 5초 이내에 보내며, 초대자 승인 이후 같은 세션의 두 소켓으로 서버가 확정한 이벤트를 보냅니다. 재시도는 최근 256개 결과를 재사용하고 연출을 다시 방송하지 않습니다. 끊긴 상대에게 보내는 요청은 실패하며 다음 접속 때 발송하지 않습니다. 재접속에는 현재 snapshot을 제공합니다. heartbeat·퇴장 유예·요청 한도·버퍼 한도와 봉투 예시는 [WebSocket 구현·검증 기록](docs/verification/websocket.md)에 있습니다.

```sh
ctest --test-dir build -R broker --output-on-failure
ctest --preset dev
```

서버 실행 파일의 재시작과 실제 QWebSocket 두 개를 사용합니다. 제품 UI의 실제 서버 연결, OS 비밀 저장소, 인터넷 WSS 및 실제 두 컴퓨터 검증은 아직 후속 작업입니다.


## QML 펫 장면 미리보기

```sh
cmake --build --preset dev
open build/desktop/daemon-session.app
```

Linux에서는 `QT_QPA_PLATFORM=wayland ./build/desktop/daemon-session`로 실행하는 구성이며 아직 실기 검증 전입니다. 지금은 고정된 두 기기의 **로컬 미리보기**입니다. 실제 친구에게 접속하거나 데이터를 전송하지 않습니다. 펫을 누르면 WIP가 열립니다. 내 WIP는 80자까지 수정·삭제하고, 친구 WIP는 읽으며 장난·선물 메뉴나 120자 쪽지로 이어집니다. 내 WIP에서 busy를 바꾸고 받은 것을 확인합니다. 움직임 줄이기와 종료는 ‘움직임·앱 메뉴’에 있습니다. 실제 Client 연결은 다음 단계입니다.

`--menu`로 키보드에서 내 메뉴를 직접 열고, `--reduced-motion`으로 움직임을 끈 채 시작할 수 있습니다. macOS에서 옵션을 전달하려면 `open build/desktop/daemon-session.app --args --menu`를 사용하세요. 메뉴는 Tab/Shift+Tab으로 이동하고 Space로 실행하며 Escape로 닫습니다. 애니메이션은 최종 위치를 바꾸지 않으며 서버 이벤트의 같은 문구를 표시합니다.

`ctest --test-dir build -R pet --output-on-failure`는 상태 모델, 실제 QML 장면, 네이티브 입력 mask와 실행 smoke를 검사합니다. 장면·배율·포커스의 실제 결과는 [QML 펫 장면 검증 기록](docs/verification/pet-scene.md)에 있습니다.

메뉴바/트레이의 데몬 아이콘에서도 내 펫·친구 펫·받은 것을 열 수 있습니다. Tab/Shift+Tab으로 이동하고 Space로 버튼을 실행하며 Escape로 닫습니다. 편집기의 Enter는 줄바꿈이며, 한글 조합 중에도 유효한 초안이면 버튼을 누를 수 있고, 조합을 확정한 뒤 저장·발송합니다. 긴 받은 목록은 Page Up/Down으로 읽고 ‘확인하고 정리’를 누를 때만 비웁니다. busy 수신과 busy 해제는 받은 창을 자동으로 열지 않습니다.

글자 수는 공통 C++ 검증으로 trim 후 Unicode code point를 셉니다. 글자 수를 넘으면 버튼을 비활성화하며 이모지의 절반을 잘라내지 않습니다. 저장·발송·정리는 확정 이벤트를 받은 뒤 반영하고, 거부된 쪽지의 초안은 열린 패널에 유지합니다. 닫은 초안과 로컬 미리보기 상태는 종료 후 보관하지 않습니다. 패널 동작·포커스·실제 한글 확인은 [패널 검증 기록](docs/verification/panels.md)에 있습니다.

```sh
ctest --test-dir build -R panels --output-on-failure
```
