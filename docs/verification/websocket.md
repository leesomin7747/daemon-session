# WebSocket 서버 구현과 검증

macOS 26.4 arm64, Qt 6.11.2의 Qt WebSockets/Qt SQL, CMake/Ninja 환경에서 수행한다. 별도 브로커·큐·네트워크 패키지 없이 기존 공통 reducer와 PairStore를 사용한다. 제품 UI, Client, OS 비밀 저장소와 인터넷 WSS 배포는 이 단계에 포함하지 않는다.

## 실행과 전송 보호

`./build/server/daemon-session-server`는 `127.0.0.1:8787`에 바인딩한다. `--db PATH`는 이미 있는 부모 폴더 안의 영속 SQLite 파일을 사용한다. 생략하면 OS 앱 데이터 폴더에 DB를 만든다. `--port 0`은 테스트용 가용 포트를 선택하고 실제 주소만 stdout에 출력한다. 잘못된 포트·메모리 DB·저장소 실패는 시작을 거절한다.

`SessionBroker::listen`은 loopback 외 주소를 거부한다. 브라우저 Origin, URL query·userinfo 및 `/` 이외 경로도 거부한다. 아직 공개 서버가 아니며 향후 TLS 종료와 지속 DB를 갖춘 단일 서버 배포를 따로 준비해야 한다. 토큰은 query에 넣지 않는다. 등록 응답은 저장할 기기 토큰을 한 번 발급하고, 같은 연결에서 동일 요청을 재시도하면 그 응답만 다시 제공한다.

## 공통 봉투

`core/WireProtocol.h`가 Broker와 후속 Client에서 공유할 16KiB JSON 봉투를 검증한다. 필드·버전·operation·타입·UTF-8과 requestId의 Unicode code point 제한을 검사한다. binary 메시지는 거부한다.

```json
{"v":1,"op":"auth","requestId":"unique-id","body":{"token":"32-byte-base64url-token"}}
```

요청마다 새로운 requestId를 사용한다. `result` 또는 `error`가 같은 requestId로 응답한다. push인 `session.event`, `session.snapshot`, `session.notice`, `join.requested`의 requestId는 빈 문자열이다. snapshot 조회 요청에는 requestId가 있어야 한다. 행동 body는 `encodeCommand` 형식이며 안팎의 requestId가 같아야 한다. actor는 보내지 않는다.

| 요청 op | body | result body |
| --- | --- | --- |
| device.register | `{}` | peerId, token |
| auth | token | peerId |
| invite.create | `{}` | invite, expiresIn(600초) |
| invite.join | invite | joinRequestId |
| invite.approve | joinRequestId | pairId |
| pair.revoke | `{}` | `{}` 후 양쪽 연결 종료 |
| session.snapshot | `{}` | encodeSnapshot 형식 |
| session.command | encodeCommand 형식 | eventId, epoch, revision |

초대 발급자는 `join.requested`의 joinRequestId·peerId를 확인하고 직접 approve한다. 발급자가 잠시 끊겼다면 인증 시 DB에서 아직 유효한 대기 요청을 복구해 알린다. 승인 시점에도 10분 만료를 확인한다. auth는 접속 후 5초 이내에 완료하며 잘못된 자격 증명은 연결을 닫는다. 동일 기기의 새 인증 연결이 기존 소켓을 대체하며, 기존 소켓 권한부터 폐기한다.

## 순서·재시도·재접속

하나의 Qt 이벤트 루프에서 페어당 하나의 상태를 변경한다. busy 전환과 장난이 경합하면 먼저 처리한 상태가 기준이다. 두 소켓에는 동일한 eventId·epoch·revision·문구·본문·최종 위치를 보낸다. presence 변경은 revision을 증가시키고 snapshot으로 전달한다.

세션별 최근 256개의 `(epoch, actor, requestId)` 결과를 보관한다. 같은 의미의 JSON 재시도에는 성공 또는 실패 응답만 다시 보내며 지난 이벤트를 다시 방송하지 않는다. 같은 ID의 다른 본문은 request_conflict다. rate_limited·offline 등의 처리 결과도 유지한다. 256개보다 오래된 요청은 캐시 보장 밖이므로 Client는 오래된 요청이나 끊긴 쪽지를 자동 재발송하지 않아야 한다. control 응답 캐시는 해당 소켓 안에서만 유지하며 재접속 인증으로 현재 멤버십·snapshot을 복구한다.

소켓 종료·heartbeat 만료는 즉시 reachable=false다. 10초마다 ping하고 마지막 정상 pong으로부터 30초에 만료한다. 소켓 메시지 처리와 수신 대상의 전달 직전에도 만료를 검사해 타이머 지연이 권한을 연장하지 않는다. 퇴장 안내만 15초 유예하며 그 전에 재접속하면 취소한다. 양쪽 모두 offline이고 유예가 끝나면 메모리 세션과 재시도 캐시를 정리한다. 다음 접속은 새 epoch다. 장난·쪽지 발송 예약은 하지 않는다.

기기 인증·페어는 DB에 남고 위치·busy·WIP·받은 항목은 메모리에만 있다. 서버가 재시작하거나 세션이 정리되면 메모리 상태는 끝난다. 이미 받은 항목은 활성 메모리 세션에서 재접속 snapshot으로 복구하고 자기 AckInbox로 비운다. 연결 해제는 DB 멤버십·관련 초대와 메모리 세션을 먼저 제거한 뒤 결과를 내보내고 두 소켓을 닫는다.

## 자원과 오류 경계

- 기기당 초당 10개, burst 20의 새 요청. 동일 소켓에 대한 추가 방어는 초당 20프레임·burst 40이며 이를 넘는 flood는 연결을 닫는다.
- 기기 등록·초대 생성은 IP별 합쳐서 1분당 10회. 소켓을 바꿔도 기기/IP 한도는 초기화하지 않는다.
- 받은 항목 최대 100개. 16KiB snapshot 용량이 먼저 차면 session_full, 개수가 먼저 차면 inbox_full로 거절하며 기존 항목을 버리지 않는다.
- 송신 예정 메시지를 포함해 64KiB를 넘는 연결은 종료한다. 느린 수신자는 즉시 offline이 되어 추가 전달 대상에서 빠진다.
- 활성 소켓 64개, pending 연결 16개, handshake 5초. rate map도 상한과 만료를 갖는다. 범위는 작은 단일 서버다.
- 정상 거절은 secret·본문 없는 오류 code다. 저장소 장애는 storage_unavailable로 응답 후 연결을 닫고 메모리 인증으로 우회하지 않는다. 토큰·초대·WIP·쪽지 본문을 서버 로그에 출력하지 않는다.

## 검증 방법과 한계

`tst_broker`는 loopback의 실제 QWebSocket, 임시 SQLite, 주입한 monotonic/UTC clock을 사용한다. 인증·쌍방 승인, 동일 이벤트, busy 순서, 끊긴 뒤 쪽지 거부, 재시도, snapshot 복구, 권한 위조·셋째 기기 거부, 연결 해제, 큰 메시지·flood, heartbeat·퇴장 유예, 100개 항목·256개 캐시·송신 버퍼 경계와 실행 파일 재시작을 검사한다. 초기 미구현 테스트의 실패를 확인했으며, 버퍼·항목 제한·캐시 정리는 보호를 잠시 제거하면 테스트가 실패하는 것도 확인했다. 실제 버퍼 테스트는 다수의 큰 응답을 한 번에 요청하여 서버의 대기 바이트 경계를 넘긴다.

이는 로컬 실시간 서버의 검증이다. 실제 인터넷 장애·TLS 호스트 설정·앱 UI·Linux 오버레이 또는 두 컴퓨터 사용을 검증한 결과로 대신하지 않는다.

API는 [Qt QWebSocket](https://doc.qt.io/qt-6/qwebsocket.html), [Qt QWebSocketServer](https://doc.qt.io/qt-6/qwebsocketserver.html)를 기준으로 한다.
