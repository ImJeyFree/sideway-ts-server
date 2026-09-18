# 선택 채널 EPG API

## 사용 흐름

1. 기존 `POST /api/channels/select?id=방송ID`로 방송을 선택합니다. `X-TS-Action: 1` 헤더가 필요합니다.
2. Player가 `GET /api/epg`를 호출해 현재 선택 채널의 편성 JSON을 받습니다.
3. 응답의 `retryAfterSeconds` 간격으로 다시 조회합니다. 채널 선택 직후에는 수집 대기가 정상입니다.
4. 응답의 `channel.id`가 Player에서 기대하는 채널과 같은지 확인한 뒤 표시합니다. 튜너 한 개의 채널 선택은 모든 Player에 공통으로 적용됩니다.

```powershell
Invoke-RestMethod http://localhost:8080/api/epg | ConvertTo-Json -Depth 10
```

다른 장치에서는 `localhost`를 서버 PC 주소로 바꾸세요. API는 HTTP 응답으로 JSON을 전달하며 파일을 생성하거나 Player로 자동 푸시하지 않습니다. 일반 VLC가 이 별도 API를 자동 표시하지는 않으므로 전용 Player가 호출해야 합니다.

## 단계별 호출 예제

### 1. 방송 목록 조회와 채널 선택

```http
GET /api/channels
```

응답의 `channels` 배열에서 원하는 방송의 `id`를 확인한 뒤 선택합니다.

```http
POST /api/channels/select?id=방송ID
X-TS-Action: 1
```

이미 선택된 방송의 EPG만 조회한다면 채널 선택은 생략합니다. 불필요하게 같은 채널을 반복 선택하면 EPG 수집이 다시 시작됩니다.

PowerShell 예제입니다. 다른 장치에서 호출할 때는 `$baseUrl`의 `localhost`를 서버 PC의 LAN IP로 바꾸세요.

```powershell
$baseUrl = "http://localhost:8080"
$catalog = Invoke-RestMethod "$baseUrl/api/channels"
$catalog.channels | Select-Object id, name, physicalChannel

# 위 목록에서 확인한 실제 방송 ID로 바꾸세요.
$channelId = "방송ID"
$encodedId = [Uri]::EscapeDataString($channelId)
Invoke-RestMethod -Method Post `
    -Uri "$baseUrl/api/channels/select?id=$encodedId" `
    -Headers @{ "X-TS-Action" = "1" }
```

### 2. 선택 방송 EPG 조회

```http
GET /api/epg
```

```powershell
$epg = Invoke-RestMethod "$baseUrl/api/epg"
$epg | ConvertTo-Json -Depth 10

# 현재·다음 프로그램 및 전체 편성 확인
$epg.current
$epg.next
$epg.events | Select-Object title, description, startTimeUtc, endTimeUtc, durationSeconds
```

`channel`은 선택 방송, `events`는 편성 목록, `current`와 `next`는 현재·다음 프로그램입니다. `retryAfterSeconds`는 권장 재조회 간격입니다.

### 3. Player 상태별 처리

- `collecting`: 편성 수집 중 안내를 표시하고 다시 조회합니다.
- `ready`: 수집된 편성을 표시합니다. 전체 수집 여부는 `complete`로 확인합니다.
- `unavailable`, `empty`: 편성 정보가 없음을 표시하고 권장 간격으로 다시 조회합니다.
- `idle`: 방송 미선택 또는 정지 상태를 표시합니다.
- `scanning`: 채널 검색 중임을 표시합니다.
- `stale`: 오래된 편성임을 표시하며 현재 방송 정보로 단정하지 않습니다.

응답의 `retryAfterSeconds` 간격으로 반복 조회하되 요청이 겹치지 않도록 이전 요청 완료 후 다음 조회를 예약하세요. `channel`이 `null`인지 먼저 검사하고, 존재하면 `channel.id`가 Player에서 기대하는 채널과 일치할 때만 편성을 표시합니다.

시작·종료 시각은 UTC이므로 Player에서 현지 시각으로 변환합니다. `current`, `next`, 시각 필드의 `null`을 정상적으로 처리해야 합니다. EPG 정보가 없어도 영상·음성은 정상 재생될 수 있습니다.

구현 검증 당시 SPOTV는 `unavailable`과 빈 `events`를 반환했습니다. 이는 당시 관측 결과이며 모든 시점의 응답을 고정하는 규칙은 아닙니다. 일반 VLC는 이 API를 자동 표시하지 않으므로 전용 Player에서 별도로 호출해야 합니다.

## 응답

다음은 형식 설명용 예시이며 실제 방송 편성이 아닙니다.

```json
{
  "schemaVersion": 1,
  "source": "ATSC_PSIP",
  "state": "collecting",
  "channel": {
    "id": "cv-571250-2",
    "name": "예시 방송",
    "serviceId": 2,
    "physicalChannel": 82
  },
  "sourceId": null,
  "receiving": true,
  "clockReady": false,
  "complete": false,
  "updatedAtUtc": null,
  "serverTimeUtc": "2026-09-16T09:00:00Z",
  "broadcastTimeUtc": null,
  "retryAfterSeconds": 2,
  "events": [],
  "current": null,
  "next": null
}
```

### 최상위 필드

| 필드 | 의미 |
|---|---|
| `state` | 아래 상태 표 참조 |
| `channel` | 수집 대상 방송의 ID·이름·서비스 ID·물리 채널. 미선택·정지·스캔 중에는 `null` |
| `sourceId` | 방송 VCT에서 선택 서비스와 연결한 PSIP source ID. 아직 확인되지 않으면 `null` |
| `receiving` | 현재 튜너의 실제 TS 수신 여부 |
| `clockReady` | 방송 STT에서 GPS→UTC 오프셋을 받았는지 여부 |
| `complete` | MGT에 안내된 EIT들의 모든 섹션을 수집했는지 여부. 방송사가 제공하지 않는 기간이나 ETT 완전성은 보장하지 않음 |
| `updatedAtUtc` | 마지막 유효 EIT 수신 시 서버 UTC 시각 |
| `serverTimeUtc` | API 응답 시 서버 UTC 시각 |
| `broadcastTimeUtc` | 방송 STT 기준 현재 UTC 시각. 미수신 시 `null` |
| `retryAfterSeconds` | 권장 조회 간격: 수집 중 2초, 그 외 30초 |
| `events` | 시작 순으로 정렬한 수집 편성 목록. EIT 갱신 시 변경·삭제 반영 |
| `current`, `next` | 현재/다음 프로그램 객체. 판단에 필요한 방송 시각이 없거나 오래된 편성이면 `null` |

| 상태 | 의미 |
|---|---|
| `idle` | 방송 미선택 또는 수신 정지로 수집 대상 없음 |
| `collecting` | 선택 후 편성 테이블 수신 대기 |
| `unavailable` | 선택 후 30초 동안 선택 방송의 유효 EIT를 받지 못함. 이후 수신되면 자동 회복 |
| `empty` | 유효 EIT를 받았으나 현재 모은 이벤트가 없음 |
| `ready` | 하나 이상의 이벤트를 수집함. 전체 수집 여부는 `complete` 참조 |
| `stale` | 마지막 유효 EIT 수신 후 5분 초과. 목록은 남지만 현재/다음 표시에는 사용하지 않음 |
| `scanning` | 채널 검색 중. 다른 주파수의 편성을 반환하지 않음 |

### 프로그램 객체

| 필드 | 의미 |
|---|---|
| `eventId` | 방송사가 부여한 이벤트 ID. 목록 구분에는 채널 ID·시작 시각도 함께 사용 |
| `title`, `description` | 제목과 ETT 설명. 한국어 우선, 없으면 첫 지원 언어. 미제공·미지원이면 빈 문자열 |
| `titles`, `descriptions` | `{"language":"kor","text":"프로그램명"}` 형식의 지원 언어별 문자열 배열 |
| `textUnsupported` | 제목 또는 설명에 미지원 압축·문자 모드가 있어 문자열을 생략했는지 여부 |
| `startTimeGps` | 원본 방송 GPS 초 |
| `durationSeconds` | 방송 길이(초) |
| `startTimeUtc`, `endTimeUtc` | `YYYY-MM-DDTHH:mm:ssZ` 형식. STT 미수신 시 `null` |
| `startUnixSeconds`, `endUnixSeconds` | Unix 초(밀리초가 아님). STT 미수신 시 `null` |

Player는 UTC 값을 로컬 시간대로 변환해 표시하세요. ETT 설명은 제목보다 늦게 도착할 수 있습니다. 요청은 메모리 스냅샷을 반환하므로 장시간 수집을 기다리며 HTTP 연결을 유지하지 않습니다.

## 호환성과 제한

- 서버 EXE와 EPG를 지원하는 `SidewayTunerCore.dll`을 함께 사용해야 합니다. 기존 C ABI 버전 1의 `stc_query(..., "epg", ...)` 조회 종류를 확장했습니다.
- 오래된 DLL 등으로 조회가 실패하면 HTTP 503 JSON 오류를 반환합니다. GET 이외에는 HTTP 405와 `Allow: GET`을 반환합니다.
- 성공 응답은 UTF-8 JSON, `Cache-Control: no-store`입니다. 조회는 채널 변경·스캔·JSON 파일 저장을 수행하지 않습니다.
- 수집은 비공개 코어에서 선택 서비스 필터 적용 전 원본 PSIP를 읽습니다. Android VLC 호환성을 위해 플레이어 TS에서 PSIP를 제외하는 처리는 유지합니다.
- ATSC PSIP의 MGT·VCT·STT·EIT·이벤트 ETT를 처리합니다. 방송사가 해당 데이터를 제공하지 않으면 편성을 생성할 수 없습니다. 인터넷 편성표나 통신사 IPTV EPG를 가져오는 기능은 아닙니다.
- 비압축 UTF-16 및 A/65 Unicode 페이지 모드를 지원합니다. Huffman, SCSU, 별도 지역 문자 모드는 현재 미지원이며 해당 언어 문자열을 생략합니다. CRC·길이·연속 카운터를 검사하고 수집 이벤트는 최대 4096개로 제한합니다.
- 서버 재시작·채널 전환·스캔 시 다시 수집합니다. EPG JSON의 디스크 보관 기능은 포함하지 않습니다.

참조 규격: [ATSC A/65:2013](https://www.atsc.org/wp-content/uploads/2021/04/A65_2013.pdf).


## 구현 검증 기록

- 공개 서버 전체 Release 빌드 성공, 공개 CTest 4개 통과. 실제 HTTP 요청으로 성공 JSON·캐시 금지·GET 제한·정확한 경로·구형 DLL 오류 응답을 검사했습니다.
- 비공개 코어 전체 Release 빌드 성공, CTest 2개 및 DLL ABI 검사 통과. CRC·패킷 누락·섹션 조립·한글·윤초 오프셋·버전 변경·채널 전환과 플레이어 TS의 PSIP 제외 유지를 검사했습니다.
- 로컬 KBS1 수신 기록을 새 파서로 재처리하여 편성 32개와 한글 제목·설명·UTC 변환, 전체 EIT 수집 완료 상태를 확인했습니다. 수신 기록 파일과 편성 결과는 공개 대상에서 제외합니다.
- 위 기록 재처리는 과거 수신 데이터를 사용한 검증이며 현재 방송의 실시간 수신 검증과는 구분합니다.

- 사용자 승인 후 기존 실행 경로의 EXE·DLL을 교체하고 서버를 재시작했습니다. SPOTV 실제 TS 수신과 `/api/epg`의 HTTP 200·UTF-8 JSON·no-store 응답을 확인했습니다. 관측 구간에서 SPOTV EIT는 수집되지 않아 `unavailable`, 빈 `events`가 반환됐습니다. 이것만으로 방송사의 EPG 제공 여부를 단정하지 않습니다.
- 채널 목록과 화질 설정 파일은 교체 전후 SHA256이 동일했습니다.

## 배포 파일과 빌드 안내

- 공개 저장소에는 서버 소스와 사용법, EPG 지원 코어 DLL 바이너리를 포함합니다. 튜너 제어·PSIP 수집 구현 소스는 별도 비공개 로컬 저장소에 보관합니다.
- `/api/epg`는 HTTP API이며 DLL의 새 export 함수를 추가하지 않습니다. 기존 `stc_query`의 조회 종류를 확장했으므로 C ABI 버전은 1, export 함수는 기존 6개를 유지합니다.
- `.lib`는 DLL 연결용 import library입니다. export 구성이 같으면 DLL 구현이 갱신되어도 `.lib` 내용과 수정 시각이 유지될 수 있습니다. EPG 지원 여부는 `.lib` 날짜만으로 판단하지 말고 최신 DLL과 HTTP 응답으로 확인하세요.
- 공개 서버 빌드: `cmake -S . -B build -A x64`, `cmake --build build --config Release`, `ctest --test-dir build -C Release --output-on-failure`.
- 공개 빌드는 비공개 코어를 다시 컴파일하지 않습니다. 실제 수신에는 EPG 지원 DLL이 실행 파일과 같은 폴더에 있어야 합니다.

## 테이블 수신 진단

`diagnostics`의 `mgt`, `vct`, `stt`, `eit`, `ett` 각각에 `seen`과 `lastSeenUtc`가 포함됩니다. CRC가 정상인 테이블을 관측했는지 확인하는 값이며 선택 방송 편성 완료를 보장하지 않습니다. 채널 전환 시 초기화합니다.
