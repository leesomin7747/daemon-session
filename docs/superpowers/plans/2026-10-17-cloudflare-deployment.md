# Task8 Cloudflare 배포 보충 계획

2026-10-17 사용자는 무료 상시 운영을 위해 Workers + SQLite Durable Objects 이식을 선택했다. 기존 Qt 제품 UI·wire v1·OS Keychain·승인 정책은 유지하고 서버 호스트만 확장한다. 기존 C++ 서버는 로컬 검증/프로토콜 기준으로 유지한다. Linux 실기는 계속 유예한다.

1. Node 내장 검사로 strict 봉투/명령·Unicode/Qt trim·busy/queue/이동/크기 제한·서버 문구를 이식한다. 같은 `core/copy/ko.json`을 사용한다. 실패를 먼저 확인한다.
2. 개인용 office Durable Object에 SQLite 영속 기기/초대/페어 권한과 일시적 세션 상태를 구현한다. SQL에는 기기/초대 해시만 쓰며 메모/쪽지/장면은 메모리에만 둔다. 승인 트랜잭션, 현재 소켓 권한, 요청 순서·중복/충돌·한도·퇴장/재접속을 원래 규칙대로 검증한다.
3. WebSocket 표준 API를 사용해 연결 중의 비저장 메모리가 idle hibernation으로 사라지지 않게 한다. 개인 office 하나가 coordination atom이다. 범용 다중 tenant/sharding을 추가하지 않는다. 무료 duration·요청·저장 한도는 문서화하며 Paid 업그레이드는 하지 않는다.
4. Miniflare 실제 SQLite/WebSocket으로 초기 등록·승인·재인증·재시도/중복·권한·만료·재시작을 검증하고, 기존 별도 QML 두 프로세스 전체 흐름을 로컬 Worker에도 실행한다. 로그는 요청/비밀/본문을 수집하지 않도록 끈다.
5. Wrangler dry-run/types/typecheck·의존성 점검·독립 리뷰 후 로그인된 무료 계정에 배포한다. 외부 WSS에도 같은 두 클라이언트 흐름을 실행한다. 실제 서로 다른 두 컴퓨터는 추가 Mac/사용자 없이는 검증 완료로 주장하지 않는다.
6. macOS Qt/QtKeychain 번들·설치 이미지와 내부 의존성 검증·고지를 제공한다. Developer ID가 없어 정식 서명·공증은 미완료로 남기며 보호 우회는 제공하지 않는다. C++ 기준 서버의30분 native idle/busy/interaction 실행은 별도 기록하고 Worker 외부 검증과 구별한다.

배포나 계정 선택이 막혀도 독립적인 구현/산출물/검사는 완료하고 정확히 남은 항목을 기록한다. 사용자 새 비밀 값을 채팅으로 받지 않는다.
