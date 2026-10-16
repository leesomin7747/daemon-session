# daemon-session

각자 작업하는 동안, 데몬과의 상호작용으로 함께 있는 느낌을 나누는 데스크톱 공간.

A desktop space where interactions with daemons make working apart feel like working together.

## 현재 단계

네이티브 오버레이 검증용 앱과 UI·서버가 함께 사용할 상태 규칙 및 JSON codec을 구현했습니다. macOS에서 빌드·일부 실제 입력 검증을 마쳤으며, KDE Wayland 실기 검증은 남아 있습니다. 실제 네트워크 연결과 제품 UI에는 아직 공통 규칙을 연결하지 않았습니다.

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
brew install qtbase qtdeclarative cmake ninja
cmake --preset dev -DCMAKE_PREFIX_PATH="$(brew --prefix)"
cmake --build --preset dev
ctest --preset dev
open build/probes/overlay/overlay_probe.app
```

EndeavourOS/KDE Wayland용 구성은 아래와 같습니다. **아직 해당 컴퓨터에서 빌드·실행하지 않은 절차**입니다. 시스템의 패키지 버전을 일관되게 유지한 상태에서 실행하세요.

```sh
sudo pacman -S --needed base-devel cmake ninja qt6-base qt6-declarative qt6-wayland layer-shell-qt
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
QT_QPA_PLATFORM=wayland ./build/probes/overlay/overlay_probe
```

probe에는 이동 가능한 두 테스트 캐릭터, 뒤 창 클릭 카운터, 3초마다 자동 ping, WIP 입력창이 있습니다. 네트워크 연결 없이 OS 창 동작만 검증합니다. 상세 절차와 항목별 실제 결과는 [오버레이 검증 기록](docs/verification/overlay.md)에 있습니다. 테스트의 `QT_SCALE_FACTOR=1.25` 결과는 실제 KWin 125% 검증을 대체하지 않습니다.

작업 규칙은 [AGENTS.md](AGENTS.md)에 있습니다. lifecycle hook은 정확한 정의에 대한 사용자 검토·승인 없이 사용하지 않습니다.

## 공통 세션 규칙

`daemon_core`는 Qt Core만 사용하는 라이브러리입니다. `core/Session.h`의 applyCommand가 인증된 actor와 명령을 받아 새 상태·확정 이벤트 또는 오류를 반환합니다. busy 중 장난은 거부하고 커피·에너지음료·쪽지는 조용히 쌓습니다. busy를 풀어도 자동 개봉하지 않으며, 받은 항목은 자기 자신만 확인할 수 있습니다. Rope/Lure 목적지는 세션에 유지하고 이후 행동이 원위치로 되돌리지 않습니다.

`core/Protocol.h`는 명령·이벤트·snapshot의 JSON 본문 codec입니다. 잘못된 타입·행동·좌표·식별자를 거부하고 WIP 80 / 쪽지 120 Unicode code point 제한을 적용합니다. text는 일반 문자열로 보존하며 향후 UI에서도 PlainText로 표시해야 합니다. 서버의 전송 봉투·인증은 후속 단계에서 연결합니다.

승인 문구는 `core/copy/ko.json`에 포함되며 `CopyCatalog`가 상황별 직전 문구를 피해서 고릅니다. 결과를 이벤트에 담으므로 클라이언트가 따로 문구를 추첨할 필요가 없습니다. 받은 쪽지는 서버 DB나 로그에 쓰지 않고 세션 메모리에만 둡니다. 전송 크기를 넘길 만큼 쌓이면 새 요청을 명시적으로 거부하며 기존 항목을 버리지 않습니다.

공통 규칙만 검증하려면 `ctest --test-dir build -R 'protocol|session' --output-on-failure`, 오버레이 회귀 검사를 포함하려면 `ctest --preset dev`를 실행하세요.
