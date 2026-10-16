# Cloudflare WSS 서버

사용자가 선택한 Workers + SQLite Durable Objects 이식이다. Qt 클라이언트 wire v1과 공유 한국어 copy catalogue를 유지한다. C++ 서버는 로컬 기준 구현으로 남는다. 서버의 응답은 같은 두 기기에 같은 event/caption/bubble/최종 위치를 보낸다.

## 로컬 검증

Node24 이상에서:

```sh
npm ci --ignore-scripts
npm run types
npm run check
npm test
npm audit
npm run dev
```

프로젝트 루트에서 `DAEMON_TEST_SERVER_URL=ws://127.0.0.1:8788/ ./build/tests/tst_twopeers fullDesktopScenario`로 실제 Qt/QML 두 프로세스를 Worker에 연결한다. Miniflare는 Wrangler와 같은 정확한 버전으로 lockfile에 고정한다. 현재 Miniflare5는 공식 v4 옵션 converter를 사용하며 SQLite inspection은 테스트 인스턴스에만 켠다.

## 저장과 권한

개인 office Durable Object가 승인/멤버십/실시간 상태의 coordination atom이다. SQLite에는 기기 토큰과 초대의 SHA-256 해시 및 페어 권한만 저장한다. 32바이트 cryptographic token·일회용10분 초대·발급자 승인·SQL 트랜잭션을 사용한다. 명령 actor는 현재 인증된 소켓에서만 가져온다. 같은 기기 새 소켓은 이전 권한을 먼저 폐기한다.

메모/쪽지/선물·장면·중복 캐시는 메모리에만 둔다. 배포/런타임 재시작으로 일시 상태가 초기화될 수 있고 영속 인증과 페어는 유지한다. 모든 peer가 떠난 후15초 유예가 지나면 메모리 세션을 정리한다. offline 우편·자동 재발송·지속 채팅 기록은 없다.

메모리를 idle hibernation이 지우지 않도록 표준 WebSocket API를 사용한다. 연결 중 DO duration을 소비한다. 외부 WebSocket의 RFC ping/pong은 플랫폼이 처리하며 Qt Client가 heartbeat를 검증한다. 메시지16KiB, 소켓64, 대기 요청(소켓32/office256), 재시도 캐시256, 기기 등록4096, IP 예산1024 및 기기별 요청 한도를 둔다. 플랫폼 WebSocket 송신 버퍼를 사용하는 것이며 C++의64KiB 버퍼 검사를 그대로 제공한다고 주장하지 않는다.

수신 메모·token·초대 코드의 console 로그를 쓰지 않는다. Worker observability/preview URL은 껐다. 일반 HTTP에는 `/healthz`의 생존 응답만 제공하고, root WebSocket은 GET/정확한 path/빈 query/Origin 없음만 허용한다. `/healthz`는 Worker 접근 확인이며 SQLite/승인 전체의 검증은 실제 연결 시나리오로 한다.

## 무료 플랜과 배포

[Cloudflare 공식 가격](https://developers.cloudflare.com/durable-objects/platform/pricing/) 기준 Free의 SQLite DO는 일100,000 요청,13,000GB-s duration,100,000 rows written,5million rows read 및 계정 저장 한도를 제공한다. 같은 DO 하나의 연결을24시간 유지하면 공식128MB 환산으로 약11,060GB-s다. 같은 계정의 다른 DO 사용도 함께 계산하며 무료 한도를 초과하면 오류가 날 수 있다. Paid 업그레이드·유료 Containers·R2·추가 도메인을 생성하지 않는다. 무제한 무료나 SLA를 보장하지 않는다.

```sh
npx wrangler login
npx wrangler whoami
npm run build
npm run deploy
```

배포 전 선택 계정이 의도한 무료 계정인지, 같은 이름의 기존 Worker를 덮어쓰지 않는지 확인한다. `new_sqlite_classes` migration으로 namespace를 생성하며 토큰·초대·메일 본문을 설정/명령 인수로 전달하지 않는다. `OFFICE_ID`는 비밀이 아닌 개인 office 이름이고 바꾸면 별도 저장 공간이 된다. 생성한 workers.dev hostname의 root를 `wss://.../`로 앱에 입력한다. 계정 login 완료 전에는 외부 배포 완료로 표시하지 않는다.

외부검증:

```sh
DAEMON_TEST_SERVER_URL=wss://실제-hostname/ ./build/tests/tst_twopeers fullDesktopScenario remoteHeartbeatKeepsTheSameConnection
```

실제 두 물리 컴퓨터 사용, Linux 실기는 별도 증거다. 로컬 workerd 검사로 대체하지 않는다. [통합 검증 기록](../docs/verification/two-computers.md)을 함께 확인한다.
