# Sideway DTV Direct Closed Caption (CEA-708) Client Integration Guide

> **작성자**: 찌링 🐾  
> **최종 갱신일**: 2026-09-18  
> **대상**: Sideway TS Player(안드로이드 앱), 스마트 TV 플레이어, 팟플레이어, 웹 기반 플레이어 개발자

---

## 1. 개요

Sideway TS Server는 지상파 디지털 방송(ATSC A/53)의 MPEG-2 비디오 스트림에 탑재된 사용자 데이터(User Data, GA94)로부터 **CEA-708(DTVCC) 폐쇄자막**을 실시간 파싱하여 REST API 형태로 제공합니다.

### 핵심 특징
1. **스트리밍 파이프라인과 100% 완전 독립 (Zero-blocking)**:  
   자막 파싱 및 JSON 직렬화는 백그라운드 전용 워커 스레드에서 수행되며, 메인 비디오/오디오 스트리밍 전송에 0μs 지연으로 동작하여 동영상 끊김을 원천 차단합니다.
2. **다중 투명 윈도우 지원 (Multi-Window Pop-on Layout)**:  
   방송국에서 송출하는 다중 화자(윈도우 0: 대화자, 윈도우 1: 기자/해설자 등) 자막을 각각의 위치(`topPct`, `leftPct`) 및 색상 정보와 함께 분리하여 제공합니다.
3. **이중 렌더링 옵션 제공 (서버 가공 모드 vs 원시 제어코드 모드)**:  
   - **서버 가공 모드**: `windows` 또는 `lines` 배열을 읽어 화면에 즉시 투영 (대부분의 앱 권장).
   - **원시 제어코드 모드**: `events` 배열에 담긴 원시 CEA-708 명령어(`DFx`, `CLW`, `DSW`, `SPL`, `CR`, `WRITE` 등)를 클라이언트 자체 자막 엔진으로 직접 해석하여 렌더링.

---

## 2. API 엔드포인트 사양

### 요청 (Request)
```http
GET /api/caption HTTP/1.1
Host: <서버_IP>:8080
```

* **권장 폴링 주기**: 300ms ~ 400ms (평균 350ms 권장)
* **캐시 제어**: 응답 헤더에 `Cache-Control: no-store`가 포함됩니다.

### 응답 (Response Example)
```json
{
  "visible": true,
  "pts": 193599462,
  "timestampMs": 1789735564548,
  "text": "오전부터 긴 줄이 준비한 물량 400여 개가 1시간",
  "windows": [
    {
      "windowId": 0,
      "visible": true,
      "topPct": 80.0,
      "leftPct": 10.0,
      "lines": [
        {
          "text": "오전부터 긴 줄이",
          "color": "#FFFFFF",
          "topPct": 80.0,
          "leftPct": 10.0,
          "italic": false,
          "underline": false
        }
      ]
    },
    {
      "windowId": 1,
      "visible": true,
      "topPct": 92.0,
      "leftPct": 28.5,
      "lines": [
        {
          "text": "준비한 물량 400여 개가 1시간",
          "color": "#FFFF00",
          "topPct": 92.0,
          "leftPct": 28.5,
          "italic": false,
          "underline": false
        }
      ]
    }
  ],
  "lines": [
    {
      "text": "오전부터 긴 줄이",
      "color": "#FFFFFF",
      "topPct": 80.0,
      "leftPct": 10.0,
      "italic": false,
      "underline": false
    },
    {
      "text": "준비한 물량 400여 개가 1시간",
      "color": "#FFFF00",
      "topPct": 92.0,
      "leftPct": 28.5,
      "italic": false,
      "underline": false
    }
  ],
  "events": [
    { "cmd": "DF1", "code": "0x99", "win": 1, "rowCount": 3, "colCount": 42, "visible": true },
    { "cmd": "SPC", "code": "0x91", "win": 1, "color": "#FFFF00" },
    { "cmd": "SPL", "code": "0x92", "win": 1, "row": 2, "col": 5 },
    { "cmd": "WRITE", "code": "0x18", "win": 1, "row": 2, "col": 5, "text": "준", "color": "#FFFF00", "rawHex": "C1D8" }
  ]
}
```

---

## 3. 필드 상세 명세

### 3.1 최상위 루트 필드
| 필드명 | 타입 | 설명 |
| :--- | :--- | :--- |
| `visible` | Boolean | 현재 자막 표시 여부 (false 시 즉시 자막 숨김 처리) |
| `pts` | Long | 비디오 프레임 동기화용 Presentation Time Stamp (90kHz 기준) |
| `timestampMs` | Long | 서버 수신 시점의 Epoch 밀리초 타임스탬프 |
| `text` | String | 모든 활성 윈도우의 텍스트가 결합된 단일 평문 (단순 로그용) |
| `windows` | Array | **[권장]** CEA-708 개별 윈도우 배열 (다중 윈도우 렌더링용) |
| `lines` | Array | 모든 윈도우의 행을 하나로 모은 라인 배열 (단일 레이아웃 fallback용) |
| `events` | Array | CEA-708 원시 파싱 제어 이벤트 (자체 엔진 구현용) |

### 3.2 `windows` 배열 객체
| 필드명 | 타입 | 설명 |
| :--- | :--- | :--- |
| `windowId` | Integer | CEA-708 Window ID (0 ~ 7) |
| `visible` | Boolean | 해당 윈도우 표시 여부 |
| `topPct` | Double | 비디오 캔버스 상단 기준 Y 위치 백분율 (0.0 ~ 100.0%) |
| `leftPct` | Double | 비디오 캔버스 좌측 기준 X 위치 백분율 (0.0 ~ 100.0%) |
| `lines` | Array | 해당 윈도우에 속한 텍스트 행(`CaptionLine`) 목록 |

### 3.3 `lines` 배열 객체
| 필드명 | 타입 | 설명 |
| :--- | :--- | :--- |
| `text` | String | UTF-8 인코딩 완료된 자막 문자열 |
| `color` | String | 16진수 색상 코드 (`#FFFFFF`, `#FFFF00`, `#00FF00` 등) |
| `topPct` | Double | 해당 행의 캔버스 기준 수직 위치 백분율 |
| `leftPct` | Double | 해당 행의 캔버스 기준 수평 위치 백분율 |
| `italic` | Boolean | 기울임꼴 여부 |
| `underline` | Boolean | 밑줄 여부 |

### 3.4 `events` 배열 객체 (원시 제어 코드)
| 필드명 | 설명 |
| :--- | :--- |
| `cmd` | 명령어 식별자 (`DF0`~`DF7`, `CLW`, `DSW`, `HDW`, `SPL`, `SPC`, `SPA`, `CR`, `WRITE` 등) |
| `code` | CEA-708 원시 16진수 코드 (`0x99`, `0x88`, `0x91`, `0x18` 등) |
| `win` | 대상 윈도우 번호 (0 ~ 7) |
| `row` / `col` | 커서 행/열 좌표 |
| `color` | 적용 색상 |
| `text` | 출력된 문자 (WRITE 명령어의 경우) |
| `rawHex` | 원본 바이트 헥사 (KS X 1001 2바이트 등) |

---

## 4. 클라이언트 구현 가이드

### 4.1 모드 A: 서버 가공 다중 윈도우 렌더링 (적극 권장)
서버가 자막 잔상 소거, EUC-KR 완성형 한글 변환, 윈도우 좌표 계산을 100% 완료한 상태이므로, 클라이언트는 화면에 투명 컨테이너만 띄우고 데이터 바인딩만 수행하면 됩니다.

#### 안드로이드 플레이어 구현 패턴 (ConstraintLayout / FrameLayout Overlay)
```kotlin
// 1. 서버 API 호출 (350ms 주기)
val json = apiClient.getCaptionSnapshot() ?: return
val isVisible = json.optBoolean("visible", true)
val windowsArray = json.optJSONArray("windows")

if (!isVisible || windowsArray == null || windowsArray.length() == 0) {
    captionContainer0.visibility = View.GONE
    captionContainer1.visibility = View.GONE
    return
}

// 2. Window 0 (대화자) 및 Window 1 (해설자/기자) 독립 바인딩
for (i in 0 until windowsArray.length()) {
    val winObj = windowsArray.getJSONObject(i)
    val winId = winObj.optInt("windowId", 0)
    val topPct = winObj.optDouble("topPct", 80.0)
    val leftPct = winObj.optDouble("leftPct", 10.0)
    val linesArray = winObj.optJSONArray("lines") ?: continue

    val targetContainer = if (winId == 0) captionContainer0 else captionContainer1
    val targetTextViews = if (winId == 0) win0TextViews else win1TextViews

    // 위치 백분율을 캔버스 픽셀 마진으로 변환
    val marginStart = (canvasWidth * (leftPct / 100.0)).toInt()
    val marginBottom = (canvasHeight * ((100.0 - topPct) / 100.0)).toInt()
    
    val params = targetContainer.layoutParams as FrameLayout.LayoutParams
    params.marginStart = marginStart.coerceIn(0, canvasWidth - 100)
    params.bottomMargin = marginBottom.coerceIn(0, canvasHeight - 50)
    targetContainer.layoutParams = params
    targetContainer.visibility = View.VISIBLE

    // 라인별 텍스트 및 색상 렌더링 (OutlineTextView 권장)
    for (l in 0 until targetTextViews.size) {
        if (l < linesArray.length()) {
            val lineObj = linesArray.getJSONObject(l)
            targetTextViews[l].text = lineObj.getString("text")
            targetTextViews[l].setTextColor(Color.parseColor(lineObj.optString("color", "#FFFFFF")))
            targetTextViews[l].visibility = View.VISIBLE
        } else {
            targetTextViews[l].visibility = View.GONE
        }
    }
}
```

#### 시각적 가독성 확보 요건
* 방송 자막 특성상 밝은 배경(설원, 스튜디오 조명 등)에서도 글자가 또렷하게 보여야 하므로, 자막 텍스트 뷰는 **외곽선(Stroke)이 적용된 뷰(`OutlineTextView`)**를 사용하는 것을 강력히 권장합니다.

---

### 4.2 모드 B: 원시 제어 코드 이벤트 기반 렌더링
독자적인 자막 엔진을 갖춘 플레이어(예: 팟플레이어 커스텀 스크립트, 전문 방송 모니터링 앱)는 `events` 배열을 순차 실행하여 자신만의 가상 화면 그리드를 직접 제어할 수 있습니다.

1. **`DF0~DF7` 수신**:
   - `win` 인덱스에 해당하는 윈도우 생성.
   - `rowCount`, `colCount` 크기로 그리드 버퍼 초기화.
   - 윈도우 정의 시 이전 텍스트 버퍼 완전 초기화.
2. **`SPL (0x92)` 수신**:
   - 펜 커서를 지정된 `row`, `col` 위치로 이동.
   - `col == 0`일 경우 현재 행의 이전 텍스트 소거.
3. **`SPC (0x91)` 수신**:
   - 현재 활성 윈도우의 펜 색상(`color`) 변경.
4. **`WRITE` 수신**:
   - 현재 커서 위치(`row`, `col`)에 문자 기록 후 커서 `col++`.
5. **`CLW / HDW / DSW` 수신**:
   - 비트마스크에 해당하는 윈도우 클리어, 숨김, 표시 토글.

---

## 5. 타임아웃 및 예외 처리 가이드

* **자동 숨김 타임아웃 (4.5초)**:  
  방송사에서 명시적인 `CLW`/`HDW` 신호를 누락하더라도, 서버가 4.5초간 새 자막이 없으면 `visible: false`를 반환합니다. 클라이언트는 `visible == false` 시 화면의 모든 자막 컨테이너를 즉시 `GONE` 처리해야 이전 자막이 화면에 멈춰있는 현상을 방지할 수 있습니다.
* **채널 변경 감지**:  
  채널 전환 시 `pts`가 리셋되거나 불연속(Discontinuity)이 발생합니다. `pts`가 직전 값보다 급격히 작아지면 클라이언트는 자막 뷰를 즉시 클리어해야 합니다.

---

## 6. 문의 및 기여
* **엔진**: Sideway ATSC A/53 CEA-708 DTVCC Decoder Engine
* **작성자**: 찌링 (Jiling 🐾)
