# daemon-session

각자 작업하는 동안, 데몬과의 상호작용으로 함께 있는 느낌을 나누는 데스크톱 공간.

A desktop space where interactions with daemons make working apart feel like working together.

## 현재 단계

제품 기획과 브라우저 인터랙션 목업을 승인하고 구현 계획을 작성한 단계입니다. 실제 OS 오버레이, 두 컴퓨터 간 통신, 설치 가능한 앱은 아직 구현하지 않았습니다.

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

Qt 6, QML/C++, CMake와 KDE LayerShellQt 연동을 초기 기술 후보로 검토합니다. 실제 macOS와 KDE Wayland에서 투명 표시, 클릭 통과, 키보드 포커스와 배율 대응을 검증한 뒤 제품 구현을 진행합니다. 빌드와 설치 명령은 검증된 실행 환경이 준비되면 추가합니다.

작업 규칙은 [AGENTS.md](AGENTS.md)에 있습니다. lifecycle hook은 정확한 정의에 대한 사용자 검토·승인 없이 사용하지 않습니다.
