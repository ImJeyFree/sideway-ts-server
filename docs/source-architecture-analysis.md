# Sideway TS Server C++ 소스 코드 심층 아키텍처 분석 및 모듈 명세서

## 1. 시스템 개요 및 1:N 팬아웃(Fan-out) 아키텍처

**Sideway TS Server**는 Windows BDA 하드웨어 TV 튜너로부터 수신한 19.75Mbps 1080i MPEG-2 Transport Stream(TS) 방송 신호를 손실 없이 수신하여, 다수의 동시 접속 클라이언트(안드로이드 TS Player, VLC, 웹 대시보드 등)에 무지연·무손실로 동시 전달하는 고성능 C++20 기반 튜너 서버입니다.

```text
[ Windows USB BDA TV Tuner ]
           │ (실시간 1080i MPEG-2 TS)
           ▼
[ SidewayTunerCore.dll (비공개 BDA 제어 코어) ]
           │ (DLL API 파이프라인)
           ▼
[ TunerClient (튜너 바인딩 & 상태 모니터) ]
           │
           ▼
[ TsBroadcaster (1:N Fan-out Hub) ]
    ├──────────────┬──────────────┬──────────────┐
    ▼              ▼              ▼              ▼
[TsSubscriber] [TsSubscriber] [TsSubscriber] [TsSubscriber]
(4MB 링버퍼)    (4MB 링버퍼)    (4MB 링버퍼)    (4MB 링버퍼)
    │              │              │              │
    ▼              ▼              ▼              ▼
[HttpStreamer] [RtspStreamer] [RtpStreamer] [UdpStreamer]
 (HTTP TS)    (RTSP/Interleave) (RTP Multicast) (UDP Multicast)
```

---

## 2. 모듈별 C++ 소스 코드 정밀 분석

### 2.1 `TsBroadcaster` & `TsSubscriber` (1:N 링버퍼 팬아웃 허브)
- **파일**: `include/TsBroadcaster.h`, `src/TsBroadcaster.cpp`
- **핵심 역할**: 수신 스레드와 각 프로토콜 송출 스레드 간의 병목 현상을 차단하는 스레드 세이프(Thread-safe) 비동기 링버퍼 허브.
- **주요 데이터 구조**:
  - `TsSubscriber`: 클라이언트별 독립된 4MB(`1024 * 1024 * 4` 바이트) 선형 원형 링버퍼 메모리.
  - `m_head`, `m_tail`, `m_count`: `std::mutex` 및 `std::condition_variable`을 활용한 세마포어 방식 데이터 푸시/팝 조율.
- **주요 메커니즘**:
  - `PushData(data, size)`: 수신 스레드에서 단 한 번의 메모리 카피로 등록된 모든 `TsSubscriber` 링버퍼에 패킷 복제.
  - `PopData(outBuf, size, timeoutMs)`: 개별 송출 스레드가 클라이언트 전송률에 맞추어 블로킹 타임아웃 방식으로 데이터 인출.
  - 버퍼 오버플로우 시 가장 오래된 이전 데이터를 자동 폐기하여 라이브 스트림 저지연 유지.

### 2.2 `TunerClient` (BDA 튜너 코어 SDK 어댑터)
- **파일**: `include/TunerClient.h`, `src/TunerClient.cpp`
- **핵심 역할**: 비공개 `SidewayTunerCore.dll`의 C-API 함수들을 동적 로딩하여 BDA 하드웨어 튜너를 제어.
- **주요 기능**:
  - `SelectChannel(channelId)`: 특정 채널 ID 선택 및 하드웨어 튜너 락 수립.
  - `StopTuner()`: BDA 튜너 장치 점유 해제 및 전력 절감.
  - `GetSignalInfo()`: 실시간 튜너 신호 강도(Signal Strength), 신호 품질(Signal Quality), 비트레이트(Mbps) 측정.

### 2.3 `HttpStreamer` (WinSock2 HTTP 서버 & REST API 라우터)
- **파일**: `include/HttpStreamer.h`, `src/HttpStreamer.cpp`
- **핵심 역할**: HTTP 1.1 스트리밍 및 관제 REST API 서버.
- **핵심 라우팅 테이블**:
  - `GET /stream`: `TsSubscriber`로부터 188바이트 TS 패킷을 읽어 HTTP Chunked / Raw 바이너리 스트리밍.
  - `GET /api/status`: 튜너 신호 강도, 비트레이트, 프로토콜별 On/Off 상태 JSON 반환.
  - `GET /api/channels`: 저장된 채널 목록 반환.
  - `GET /api/epg`: 선택 채널의 PSIP/EIT EPG 데이터 JSON 반환.
  - `GET /api/rtsp/toggle`, `GET /api/rtp/toggle`, `GET /api/udp/toggle`: 프로토콜 동적 시작/정지.
  - `POST /api/channels/select?id={id}`, `POST /api/channels/stop`: 채널 원격 선택 및 방송 종료.

### 2.4 `RtspStreamer` (RFC 2326 / RFC 3550 RTSP 라이브 스트리머)
- **파일**: `include/RtspStreamer.h`, `src/RtspStreamer.cpp`
- **핵심 역할**: 무선 Wi-Fi 환경 패킷 유실을 완벽 차단하는 RTSP 1.0 서버.
- **프로토콜 상태 머신**:
  - `OPTIONS`: 지원 메서드 응답.
  - `DESCRIBE`: RFC 2250 MPEG-2 TS Payload Type 33 SDP(Session Description Protocol) 생성.
  - `SETUP`: 전송 모드 설정 (`RTP/AVP/TCP;interleaved=0-1` 또는 `RTP/AVP;unicast`).
  - `PLAY`: 스트리밍 시작.
  - `TEARDOWN`: 세션 정상 종료 및 소켓 정리.
- **성능 최적화**:
  - **TCP 인터리빙**: 제어 포트(8554) 내에 `$00` (RTP data) 및 `$01` (RTCP) 바이너리 프레임을 직접 인터리빙하여 전송.
  - **2MB 소켓 버퍼 & `TCP_NODELAY`**: 소켓 송신 버퍼(`SO_SNDBUF`)를 2MB로 확장하고 Nagle 알고리즘을 비활성화하여 19.75Mbps 방송 순간 버스트 지연 제거.
  - **90kHz 마이크로초 클럭 동기화**: `std::chrono::steady_clock` 기반 마이크로초 단위 타임스탬프 계산으로 지터 및 오버플로우 방지.

### 2.5 `RtpStreamer` & `UdpStreamer` (멀티캐스트 전송 엔진)
- **파일**: `include/RtpStreamer.h`, `include/UdpStreamer.h`
- **RTP 멀티캐스트 (`239.255.0.1:5004`)**:
  - RFC 2250 헤더 규격에 맞추어 12바이트 RTP 헤더(Sequence Number, 90kHz Timestamp, SSRC) 추가 후 188바이트 TS 패킷 7개(1,316바이트)를 하나의 UDP datagram으로 패킹.
- **UDP 멀티캐스트 (`239.255.0.1:1234`)**:
  - 188바이트 TS 패킷 7개를 Raw UDP datagram으로 패킹하여 0.1초 극단적 초저지연 송출.

### 2.6 `QualityConfig` & `QualityStore` (품질 설정 파일 관리)
- **파일**: `include/QualityConfig.h`
- **역할**: `quality.json` 파일의 디인터레이싱 모드(`deinterlaceMode`), FFmpeg 스레드(`avcodecThreads`), 네트워크 버퍼(`networkCachingMs`), RTSP 전송 방식(`rtspTransport`), HW 가속(`hardwareAcceleration`) 설정을 읽고 저장.

---

## 3. REST API 전체 명세표

| HTTP 메서드 | 엔드포인트 URL | 요청 파라미터 / 헤더 | 응답 데이터 (JSON) | 비고 |
|:---|:---|:---|:---|:---|
| `GET` | `/api/status` | 없음 | `{"status":"ok", "receiving":true, "bitrateMbps":19.75, "isRtspEnabled":true, ...}` | 튜너 및 송출 상태 |
| `GET` | `/api/channels` | 없음 | `[{"id":"ch1", "name":"KBS1", "physicalChannel":15, ...}]` | 방송 채널 목록 |
| `GET` | `/api/epg` | 없음 | `{"channelName":"KBS1", "title":"KBS 뉴스 9", "startTime":"21:00", ...}` | 실시간 EPG 정보 |
| `GET` | `/api/rtsp/toggle` | 없음 | `{"status":"ok", "isRtspEnabled":true}` | RTSP 송출 On/Off 토글 |
| `GET` | `/api/rtp/toggle` | 없음 | `{"status":"ok", "isRtpEnabled":true}` | RTP 송출 On/Off 토글 |
| `GET` | `/api/udp/toggle` | 없음 | `{"status":"ok", "isUdpEnabled":true}` | UDP 송출 On/Off 토글 |
| `POST` | `/api/channels/select` | `?id={channelId}`<br>`X-TS-Action: 1` | `{"status":"ok", "selectedChannel":"ch1"}` | 방송 채널 선택 및 수신 |
| `POST` | `/api/channels/stop` | `X-TS-Action: 1` | `{"status":"ok", "message":"Tuner stopped"}` | 방송 수신 종료 (BDA 해제) |
| `POST` | `/api/scan/start` | `?input=cable&first=2&last=135`<br>`X-TS-Action: 1` | `{"status":"ok", "message":"Scan started"}` | 채널 스캔 시작 |
| `POST` | `/api/scan/cancel` | `X-TS-Action: 1` | `{"status":"ok", "message":"Scan canceled"}` | 채널 스캔 취소 |

---

## 4. 4대 라이브 스트리밍 프로토콜 패킷 특징 비교

| 프로토콜 | 전송 계층 | 포트 / 주소 | 패킷 구조 | 주요 사용 목적 |
|:---|:---|:---|:---|:---|
| **HTTP TS** | TCP | `8080/stream` | Raw TS Byte Stream | 모바일 브라우저 및 기본 플레이어 |
| **RTSP TCP** | TCP (Interleaved) | `8554/live` | RTSP Header + `$00` + RTP (1316B) | Wi-Fi 패킷 유실 방지 및 고화질 수신 |
| **RTP Multicast** | UDP (Multicast) | `239.255.0.1:5004` | 12B RTP Header + 7x TS (1316B) | 사내망/LAN 다수 기기 동시 시청 |
| **Raw UDP** | UDP (Multicast) | `239.255.0.1:1234` | 7x TS Packets (1316B) | 0.1초 극단적 초저지연 시청 |

---

## 5. 검증 및 CTest 회귀 테스트 스위트

- **`stream-regressions`**: 모의(Mock) 튜너 DLL을 통한 19.75Mbps TS 데이터 전송 무결성 검증.
- **`loader-missing` / `loader-version`**: `SidewayTunerCore.dll` 누락 및 API 버전 불일치 핸들링 검증.
- **`dashboard-regressions`**: Node.js 기반 웹 대시보드 REST API 및 HTML 상태 동기화 단언 검증.
