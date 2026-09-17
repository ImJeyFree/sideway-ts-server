# Sideway TS(Transport Stream) Server

Windows USB TV 튜너에서 수신한 방송을 HTTP와 UDP 멀티캐스트로 전달하는 서버입니다.
공개 서버 소스와 별도 배포되는 비공개 `SidewayTunerCore.dll`로 구성합니다. 전체 제품이 오픈소스인 것은 아닙니다.

공개 서버 코드·SDK API 헤더는 Apache-2.0([LICENSE](LICENSE))입니다. 비공개 DLL·import LIB는 [별도 바이너리 배포 조건](BINARY_LICENSE.md)을 적용합니다. 서드파티 코드는 원래 라이선스를 따릅니다.

개인 블로그 : https://side-ways.tistory.com/5, https://blog.naver.com/ibook/224413425060

## 구성

- 공개: 실행 옵션·웹 UI·HTTP/UDP 전송·구독자 버퍼·채널 JSON 형식·DLL 어댑터·모의 테스트.
- 비공개 DLL: BDA 장치 제어·스캔·방송 테이블 분석·선택 서비스 구성·장치 호환성 처리.
- 배포 바이너리 (`release/`): 즉시 실행 가능한 `sideway-ts-server.exe`, 실제 하드웨어 튜너 코어 `SidewayTunerCore.dll`, 빌드용 `SidewayTunerCore.lib`. 별도 빌드 없이 바로 실행할 수 있습니다.
- SDK 규격: [DLL API](docs/DLL_API.md), [공개 및 배포 범위](docs/공개_배포_범위.md).

## 프로젝트 디렉토리 구조

```text
C:\ws\ts\server
├── README.md                                   # 서버 사양, 웹 대시보드, 빌드/실행 및 REST API 문서
├── CMakeLists.txt / CMakePresets.json          # C++20 / MSVC / CMake 빌드 환경 설정
├── .env / .env.example                         # 서버 환경 설정 파일
├── LICENSE / BINARY_LICENSE.md                 # 공개 소스(Apache-2.0) 및 비공개 코어 DLL 배포 조건
├── docs/                                       # 상세 모듈 사양 및 변경 기록 문서
│   ├── 변경_기록.md                             # RTSP 동적 토글 및 품질 설정 연동 기록
│   ├── DLL_API.md                              # BDA 튜너 코어 SDK API 규격
│   ├── EPG_API.md                              # 실시간 EPG REST API 가이드
│   └── 공개_배포_범위.md                        # 오픈소스 및 비공개 라이브러리 범위
├── include/                                    # C++ 헤더 파일
│   ├── HttpStreamer.h                          # HTTP TS 스트리머 및 REST API 핸들러
│   ├── RtspStreamer.h                          # RFC 2326 / RFC 3550 RTSP 스트리머 (TCP Interleaved)
│   ├── QualityConfig.h                         # 디코딩/스트리밍 품질 동적 설정 모델
│   └── WebDashboard.h                          # 웹 대시보드 HTML/JS 및 동적 상태 제어
├── src/                                        # C++ 구현 파일
│   ├── HttpStreamer.cpp                        # HTTP/REST API 서버 및 세션 관리
│   ├── RtspStreamer.cpp                        # RTSP 상태 머신 및 TCP/UDP RTP 송출
│   └── main.cpp                                # 서버 진입점 및 서비스 초기화
├── release/                                    # 즉시 실행 가능한 사전 빌드 바이너리
│   ├── sideway-ts-server.exe                   # 최신 서버 실행 파일 (381 KB)
│   ├── SidewayTunerCore.dll                    # 실제 하드웨어 BDA 튜너 제어 코어 DLL
│   └── SidewayTunerCore.lib                    # SDK 링크 라이브러리
└── tests/                                      # CTest 및 Node.js 대시보드 회귀 테스트
    └── DashboardTests.js                       # 대시보드 UI/REST API 자동 검증 스크립트
```

## 빌드

Windows x64, Visual Studio C++ 도구, Windows SDK, CMake 3.20 이상이 필요합니다. 웹 테스트에는 Node.js를 사용합니다.

```powershell
cmake -S . -B build -A x64 -DTUNER_CORE_SDK=C:/SDK/SidewayTunerCore
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

SDK의 `bin/SidewayTunerCore.dll`을 실행 파일 옆에 복사합니다. SDK 없이도 공개 소스의 빌드·모의 테스트는 가능하지만 실제 TV 수신에는 DLL이 필요합니다. 서버는 실행 파일 옆 DLL만 로드하며 API 버전 불일치·누락 시 오류를 표시합니다. x64용 Microsoft Visual C++ 런타임이 필요할 수 있습니다.

## 실행

사전 빌드된 `release` 폴더의 바이너리를 사용하거나 직접 빌드한 실행 파일을 실행합니다.

```powershell
# [사전 빌드 바이너리 바로 실행 - 권장]
.\release\sideway-ts-server.exe

# 옵션 사용 예시
.\release\sideway-ts-server.exe --scan
.\release\sideway-ts-server.exe --no-scan
.\release\sideway-ts-server.exe --channels C:/TV/channels.json
```

- 실행 파일 옆 `channels.json`에 방송 목록·마지막 선택을 저장합니다. 없거나 비어 있으면 자동 스캔합니다.
- `--scan`은 강제 검색, `--no-scan`은 자동 검색 생략입니다. 동시에 사용할 수 없습니다.
- 기본 검색은 케이블 8VSB CH 2~135입니다. 웹에서 입력 방식·변조·범위를 변경합니다. 지역별 주파수표에 따라 검색 결과가 달라집니다.
- 웹 대시보드: http://localhost:8080/
- 웹 대시보드의 **`[선택 방송 재생] ↔ [방송 종료]`** 버튼으로 실시간 방송 수신을 토글할 수 있습니다. 방송 종료 시 튜너 장치 점유를 완전히 해제하여 다른 프로그램과의 충돌을 방지합니다.
- HTTP 스트림: http://localhost:8080/stream
- UDP 스트림: `udp://@239.255.0.1:1234` (기본 정지/OFF 상태, 웹 대시보드 또는 API로 켤 수 있음)
- RTP 스트림: `rtp://@239.255.0.1:5004` (기본 정지/OFF 상태, RFC 2250 / RFC 3550 기반 패킷 동기화 스트리밍, VLC 플레이어 권장)
- 다른 기기에서는 localhost 대신 서버 PC의 LAN IP를 사용합니다.
- 스캔 중 시청이 중단됩니다. 완료·취소 후 이전 방송으로 복귀하며 플레이어 재연결이 필요할 수 있습니다.
- 취소·빈 결과로 기존 JSON을 덮어쓰지 않습니다. 부분 스캔은 범위 밖 방송을 보존합니다.
- 한 개 튜너의 채널 변경은 모든 시청자에게 적용됩니다.
- Android VLC 호환성을 위해 송출에서 PSIP를 제외합니다. 스캔·웹 이름은 유지되지만 플레이어에 원본 PSIP 방송 이름·편성 정보가 전달되지 않습니다.
- 서버에는 접속 인증이 없습니다. 인터넷에 직접 포트를 공개하지 말고 신뢰할 수 있는 내부망에서 사용합니다.

## API

| 요청 | 기능 |
|---|---|
| GET /api/status | 장치·수신 상태 (UDP/RTP 송출 여부 포함) |
| GET /api/channels | 저장 방송 목록 |
| GET /api/scan | 스캔 진행 상태 |
| GET /api/epg | 선택 방송의 EPG·현재/다음 프로그램 JSON |
| GET /api/udp/toggle | UDP 멀티캐스트 송출 On/Off 토글 |
| GET /api/rtp/toggle | RTP 멀티캐스트 송출 On/Off 토글 |
| GET /api/rtsp/toggle | RTSP 스트림 송출 On/Off 토글 |
| POST /api/scan/start?input=cable&modulation=8VSB&first=2&last=135 | 검색 시작 |
| POST /api/scan/cancel | 검색 취소 |
| POST /api/channels/select?id=방송ID | 방송 선택 및 재생 시작 |
| POST /api/channels/stop | 방송 종료 (튜너 점유 해제) |

POST에는 `X-TS-Action: 1` 헤더가 필요합니다. 이는 사용자 인증 수단이 아닙니다.

선택 방송의 편성은 `GET /api/epg`로 조회합니다. Player 연동 방법, JSON 필드와 수집 대기 상태는 [EPG API](docs/EPG_API.md)를 참조하세요. EPG를 지원하는 서버 EXE와 튜너 DLL을 함께 사용해야 합니다.

## 검증과 배포

- **사전 빌드 바이너리 (`release/`)**: 별도 빌드 없이 바로 사용할 수 있도록 최신 실행 파일(`sideway-ts-server.exe`)과 실제 하드웨어 튜너 코어(`SidewayTunerCore.dll`), SDK 링크용(`SidewayTunerCore.lib`)이 저장소에 함께 배포됩니다.
- **테스트 및 검증**:
  - 공개 CTest 회귀 테스트(`stream-regressions`, `loader-missing`, `loader-version`)는 모의(Mock) DLL을 사용하여 실제 튜너 장치 없이도 API 및 전송 안정성을 검증합니다.
  - `dashboard-regressions`는 개발 환경에 Node.js가 있을 때 웹 대시보드 스크립트의 초기 동작을 자동 검증합니다 (서버 실행 자체는 Node.js 의존성 없음).
- **별도 아카이브 패키징**: `tools/Prepare-PublicRelease.ps1`을 사용하면 소스·실행·SDK를 버전별 독립 ZIP 아카이브 및 SHA256 해시 목록으로 로컬 패키징할 수 있습니다.
- **보안 및 라이선스**: 실제 채널 JSON·녹화물·장치 로그·디버그 심볼·비공개 코어 소스는 저장소에 포함되지 않습니다. 바이너리 사용 조건은 [별도 바이너리 배포 조건](BINARY_LICENSE.md) 및 [서드파티 고지](THIRD_PARTY_NOTICES.md)를 따릅니다.

상세 구현 범위·검증·알려진 제약은 [변경 기록](docs/변경_기록.md)을 확인하세요.

#WindowsBDA #DirectShow #SidewayTSServer #RTSP #MPEG2 #HDTV #TVTuner #MPEG2TS #WebDashboard #RestAPI

WindowsBDA, DirectShow, SidewayTSServer, RTSP, MPEG2, HDTV, TVTuner, MPEG2TS, WebDashboard, RestAPI
