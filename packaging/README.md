# 설치 산출물

## macOS 로컬 검증용

```sh
python3 packaging/package_macos.py
```

Release 빌드 후 공식 `macdeployqt`로 Qt/QML·플러그인을 포함하고 QtKeychain dylib를 함께 둔다. 전체 Mach-O 의존성이 시스템 또는 번들 내부에만 있는지 확인한다. Qt의 NFC/NFD 경로 계산을 피하려고 canonical ASCII 임시 폴더에서 배포한 뒤 최종 위치로 이동한다. 이미 만든 산출물을 덮어쓰지 않는다.

`out/packages/daemon-session-<version>-macos-<arch>-local-<UTC>/` 아래 `.app`, `.dmg`, `.zip`, `SHA256SUMS`, `manifest.json`을 만든다. DMG에는 Applications 바로가기를 넣었다. 현재 산출물은 **arm64 / macOS 26.0 이상**을 대상으로 빌드했고 실제 실행은 macOS26.4에서 확인했다. manifest의 실제 deployment target을 확인한다. Intel·다른 macOS·Qt 미설치 실제 다른 컴퓨터에서의 실행은 검증하지 않았다.

**현재는 ad-hoc 서명한 로컬 검증용이며 Developer ID 서명·공증된 공개 배포본이 아니다.** 보호 기능을 끄거나 quarantine을 제거하는 절차를 제공하지 않는다. 정식 배포 전에 Developer ID Application 인증서로 중첩 코드부터 서명하고, [Apple의 공증 절차](https://developer.apple.com/documentation/security/notarizing-macos-software-before-distribution)에 따른 제출·수락 확인·staple·Gatekeeper 평가를 수행해야 한다. 현재 Mac에는 유효한 codesigning identity가 없다.

```sh
python3 packaging/verify_macos.py /절대경로/daemon-session.app
codesign --verify --deep --strict /절대경로/daemon-session.app
```

번들 안에 의존성 고지·공식 라이선스 원문·Qt SPDX 및 동봉된 Homebrew 라이브러리 고지가 있다. 공개 binary release 전 최종 의존성 및 대응 소스 제공을 다시 검토한다. 현재 산출물을 자동으로 GitHub Release에 게시하지 않는다.

## KDE Wayland 준비 자료 — 실기 유예

정확한 EndeavourOS 컴퓨터에서 Qt6 계열 패키지 버전을 일관되게 유지해 빌드한다. `qt6-base`, `qt6-declarative`, `qt6-websockets`, `qt6-wayland`, [`qtkeychain-qt6`](https://archlinux.org/packages/extra/x86_64/qtkeychain-qt6/), [`layer-shell-qt`](https://archlinux.org/packages/extra/x86_64/layer-shell-qt/), CMake/Ninja가 필요하다. 기본 시스템의 KDE Wallet/Secret Service가 정상 작동해야 하며 평문 fallback은 없다. 실제 OS 검증과 Linux 실행 바이너리 생성은 사용자의 완성본 공개 이후다.

`linux/daemon-session.desktop`은 PATH에 설치한 `daemon-session`을 native Wayland로 실행하는 entry다. 아직 해당 컴퓨터에서 desktop-file-validate나 실행하지 않았다. AppImage/Flatpak/자동 업데이트를 추가하지 않았다.

## 로컬 C++ 기준 서버만 빌드

```sh
cmake -S . -B out/server -G Ninja -DDAEMON_BUILD_DESKTOP=OFF -DBUILD_TESTING=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build out/server
cmake --install out/server --prefix /원하는/설치/경로
```

서버는 loopback에만 bind하고 지속 SQLite 파일을 사용한다. 외부 무료 호스트는 승인한 [Cloudflare 이식](../cloud/README.md)을 사용한다.
