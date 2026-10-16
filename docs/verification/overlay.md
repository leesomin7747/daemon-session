# Native overlay validation

Task 1 진행 중. 양쪽 실제 GUI 검증을 마치기 전 Qt 기반 지원을 확정하거나 Task 2로 넘어가지 않는다. 이 probe는 로컬 창 처리만 제공한다. 두 컴퓨터의 동기화나 제품 UI는 구현하지 않았다.

## 구현

- `OverlayWindow`: 투명 Qt Quick 창, 일반 창 위 표시, 하단 작업 영역 배치. 표시 및 ping은 활성화를 요청하지 않는다.
- 클릭 영역: 유효한 논리 좌표의 사각형을 창 경계로 자른 뒤 `QWindow::setMask`로 갱신한다. 영역이 없으면 `WindowTransparentForInput`을 켠다. Qt가 디바이스 배율을 변환한다.
- 편집: 직접 WIP를 열 때만 포커스 허용·활성화 요청. 닫으면 수동 모드로 복구하고, 편집 전 앱 내부 창이 살아 있고 보이면 그 창으로 돌아간다. 외부 앱으로 자동 복귀하는 기능은 제공하지 않으며 직접 클릭해서 돌아간다.
- Linux: LayerShellQt의 Bottom/Left/Right 앵커, LayerTop, exclusive zone 0, KeyboardInteractivityNone. 편집 중 OnDemand. 작업 영역과 전체 화면의 차이를 좌우·하단 margin으로 반영한다. X11/XWayland 실행은 probe가 거부한다.
- probe의 WIP는 메모리에만 보존한다. 실제 작업 화면·외부 키 입력을 수집하지 않는다.

`setMask`는 플랫폼 hint이므로 자동 테스트의 마스크 일치만으로 클릭 통과를 보장할 수 없다. [Qt QWindow 문서](https://doc.qt.io/qt-6/qwindow.html#setMask), [LayerShellQt 6.7.5 인터페이스](https://github.com/KDE/layer-shell-qt/blob/v6.7.5/src/interfaces/window.h)를 기준으로 구현했다.

## 확인한 환경과 결과

2026-10-16: macOS 26.4 arm64, Qt 6.11.2, Cocoa, 실제 창 DPR 2, CMake 4.4.4, Ninja 1.13.2, Apple clang. 공식 Homebrew의 qtbase/qtdeclarative 설치. 최초 의존성 설치 중 OpenSSL opt 링크 누락으로 실행이 중단되어 `brew reinstall openssl@4`로 복구했다.

| 항목 | macOS 실제 결과 | KDE Wayland 실제 결과 |
| --- | --- | --- |
| 빌드와 실행 | 성공; 네이티브 두 창 표시 | 미실행 |
| 펫 두 개 및 편집창 렌더링 | 창 캡처로 확인 | 미실행 |
| 빈 공간 클릭 통과 | 클릭 후 뒤 창 카운터 0→1 확인 | 미실행 |
| 펫 클릭 / 이동한 옛 위치 통과 | 카운터를 포함한 재검증 필요 | 미실행 |
| 자동 ping 중 뒤 작업창 포커스 | ping 증가 중 `focus-before-after` 입력 유지 | 미실행 |
| WIP 한글·이모지 | `알고리즘 과제 ☕` 입력 및 저장 확인 | 미실행 |
| WIP 닫기 후 앱 내부 포커스 복귀 | 수정 후 AX에서 작업창 입력 포커스 확인; Cocoa 회귀 테스트 통과 | 미실행 |
| 다른 앱 위 표시 | 미검증 | 미실행 |
| 다른 앱 타이핑 유지 | 자동 ping을 켠 상태에서 TextEdit의 `external-before` 뒤에 `-after` 입력, 입력 영역 포커스 유지 확인. 앱 전환 전후 광범위한 검사는 남음 | 미실행 |
| 실제 배율에서 가장자리 클릭 | DPR 2 기본 동작 확인; 체계적인 가장자리 검사 남음 | 125% 미실행 |
| Dock/패널 및 작업 영역 변경 | 미검증 | 미실행 |

남은 macOS 화면 검증 중 Mac이 잠겨 CUA가 진행을 거부했고 사용자가 직접 해제했다. 재개 후 다른 앱 연결에서도 CUA 시간 초과가 발생했으나 이후 TextEdit 연결은 회복됐다. 펫의 화면 캡처만으로 실제 클릭 영역 일치를 단정하지 않고 해당 항목은 보류한다.

보존한 캡처: [네이티브 펫·WIP 창](evidence/daemon-overlay-native.png), [검증 작업창](evidence/daemon-overlay-workbench.png), [외부 앱 입력](evidence/daemon-overlay-external-focus.png). 개별 창 캡처이며, 투명 영역은 캡처 도구에서 흰 배경으로 표시된다. 다른 앱과 합성된 전체 바탕화면의 투명도·위아래 순서를 증명하는 캡처는 아니다.

자동 테스트는 8개 동작을 기본 배율과 `QT_SCALE_FACTOR=1.25`로 각각 실행한다. 이동 전후 영역 교체, 소수 좌표 경계, 잘못된 영역/창 밖 영역 무시, 빈 영역 통과, 편집에 따른 포커스 허용, 정규화 위치의 경계, 편집 종료 시 이전 창 복귀, 열린 편집창으로 재진입할 때 포커스 재획득을 확인한다. 실제 Probe.qml을 로드해 WIP를 연 채 펫을 이동했을 때 편집창의 새 영역만 입력을 받는지도 검사한다. QML 로드·종료 스모크 테스트와 함께 CTest 4/4 통과.

별도로 `QT_QPA_PLATFORM=cocoa ./build/tests/tst_overlaygeometry editorReturnsFocusToPreviousWindow explicitEditCanRegainFocus`에서 실제 Cocoa 포커스 두 경우가 통과했다. OS 입력 서비스의 CFMessagePortSendRequest 경고가 있었으나 두 포커스 assertion은 통과했다. 이 결과는 네이티브 입력 영역 전체의 통과 판정이 아니다.

## 코드 리뷰와 남은 확인

독립 리뷰가 재현한 “펫 이동 후 WIP 클릭 영역이 옛 자리에 남는 문제”와 추가로 확인한 “이미 열린 편집창 재진입 시 활성화되지 않는 문제”는 각각 실패하는 테스트를 먼저 실행한 뒤 수정했다. 중요한 미해결 코드 리뷰 항목은 없다.

경미한 보류 항목: KWin은 exclusive zone 0인 surface를 이미 패널의 예약 공간을 뺀 영역에 배치한다. Qt가 `availableGeometry()`에도 패널 여백을 보고하는 환경에서는 현재 margin 처리가 여백을 중복 적용해 펫이 추가로 떠 보일 수 있다. 실제 Qt/KWin 환경의 geometry 값과 배치를 확인한 뒤 조정한다. [KWin 6.7.5 배치 구현](https://github.com/KDE/kwin/blob/v6.7.5/src/layershellv1integration.cpp#L71)을 근거로 한 조건부 지적이며, 실제 Linux 재현 결과는 아니다.

입력 목표 영역은 캐릭터 외곽을 감싸는 사각형이다. 둥근 모서리의 투명 픽셀까지 통과하는 모양별 입력 영역은 구현하지 않았다. 2단계 이후 제품 구현에 들어가기 전에 실제 가장자리 검사에서 이 목표 영역의 적절성을 판단한다. 전체 화면 위 표시와 여러 디스플레이 사이 이동은 승인된 초기 범위 밖이다.

## 실제 컴퓨터에서 확인하는 순서

README의 빌드·실행 명령을 사용한다. Linux 패키지는 공식 Arch [layer-shell-qt](https://archlinux.org/packages/extra/x86_64/layer-shell-qt/)와 [qt6-wayland](https://archlinux.org/packages/extra/x86_64/qt6-wayland/)를 사용한다. Linux의 빌드 성공 여부부터 기록한다.

1. 화면 아래 펫 두 개가 보이고, 그 사이에 뒤 창이 그대로 보이는지 확인한다. OS/Qt/LayerShellQt 버전, 디스플레이 해상도·배율, probe의 플랫폼·DPR을 적는다. Linux 플랫폼은 `wayland`여야 한다.
2. 펫을 한 번 클릭해 **펫 클릭**만 증가하는지 확인한다. 둥근 모서리의 바깥 빈 부분도 확인한다.
3. 주변 빈 곳을 클릭해 **뒤 창 클릭**만 증가하는지 확인한다. `펫 이동`을 누른 뒤 이전 자리에서는 뒤 창, 새 자리에서는 펫 카운터가 증가하는지 확인한다. 반복 이동한다.
4. `3초마다 자동 ping`을 켜고 뒤 창 입력창에 타이핑한다. ping이 여러 번 증가해도 문자가 빠지거나 입력 위치가 바뀌지 않아야 한다.
5. WIP를 열어 한글·영문·이모지를 입력하고 Enter/Esc/닫기로 종료한다. 작업창에 이어 입력되는지 확인한다. 외부 앱에서도 타이핑하다 펫을 더블클릭해 편집하고, 닫은 뒤 외부 앱을 직접 클릭해 입력을 계속한다.
6. 자동 ping을 켠 채 일반 편집기나 터미널을 앞에 둔다. 펫은 계속 보이고 외부 앱의 입력 포커스가 유지되어야 한다. 전체 화면 앱 위 표시 여부는 범위 밖이다.
7. KWin 실제 100%와 125%에서 2~6을 반복한다. 환경변수로 배율을 중복 적용하지 않는다. 각 펫 네 변과 이동한 옛 위치를 클릭한다.
8. Dock/패널의 위치·크기 또는 자동 숨김을 바꿔 작업 영역 변화 후 펫이 화면 밖이나 패널 뒤로 밀리지 않는지 확인한다. 테스트 후 원래 설정으로 복원한다.
9. 결과 표에 통과/실패, 재현 단계, 캡처를 추가한다. 실패 시 Qt의 공통 창 설정만으로 충분한지 해당 플랫폼에서 다시 판단한다. 완료 후 `검증 종료`로 앱을 닫는다.
