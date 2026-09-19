# Sideway TS Player 개인정보 처리방침 (Privacy Policy)

> **최종 개정일 / Last Updated**: 2026년 9월 18일 / September 18, 2026  
> **애플리케이션 명칭 / Application Name**: Sideway TS Player  
> **패키지 명칭 / Package Name**: `com.sideway.tsplayer`  
> **개발자 연락처 / Developer Email**: `im.jey.free@gmail.com`  
> **공식 저장소 / Official Repository**: [https://github.com/ImJeyFree/sideway-ts-player](https://github.com/ImJeyFree/sideway-ts-player)

---

## 🇰🇷 [한국어 버전]

### 1. 개인정보 수집 및 처리 개요
**Sideway TS Player** (이하 '앱')는 사용자의 개인정보 및 프라이버시를 최우선으로 보호합니다. 본 앱은 개인정보보호법 및 Google Play 개발자 정책을 준수하며, **사용자의 위치, 연락처, 기기 식별자, 시청 기록 등 어떠한 개인정보도 외부 서버로 수집, 저장, 전송하지 않습니다.**

---

### 2. 수집하는 개인정보 항목 및 처리 목적
본 앱은 회원가입이나 로그인 절차가 없으며, 사용자의 개인정보를 전혀 수집하지 않습니다.
- **수집하는 개인정보 항목**: 없음 (회원가입 미지원)
- **개인정보의 외부 전송**: 없음 (어떠한 사용자 데이터도 외부 서버로 전송되지 않음)

---

### 3. 단말기 로컬 데이터 저장 (Local Storage)
사용자가 앱 내에서 설정한 모든 정보는 외부 서버가 아닌 **사용자의 단말기 내부 저장소(`SharedPreferences` 및 로컬 미디어 폴더)**에만 안전하게 보관됩니다.
1. **앱 환경 설정값 (`SharedPreferences`)**:
   - 사용자가 직접 입력한 로컬 TV 튜너 서버 주소(IP:Port), 프로토콜 선택(HTTP/RTSP/RTP/UDP), 자막 및 언어 선택 설정값
2. **방송 녹화 파일 (PVR .ts 파일)**:
   - 사용자가 실시간 방송 시청 중 직접 [녹화] 버튼을 눌러 생성한 영상 파일은 단말기 내부 로컬 폴더(`Movies/SidewayPlayer`)에만 물리적으로 보관됩니다.

---

### 4. 접근 권한 사용 목적 (App Permissions)
본 앱은 로컬 네트워크 내 TV 튜너 서버와의 방송 스트림 통신에 필수적인 아래 권한만을 사용합니다.
- **`android.permission.INTERNET`**: 사용자의 홈 로컬 네트워크(Wi-Fi) 내 TV 튜너 서버와 라이브 미디어 스트림을 수신하기 위한 필수 권한
- **`android.permission.ACCESS_NETWORK_STATE`**: Wi-Fi 네트워크 연결 상태를 확인하기 위한 권한
- **`android.permission.CHANGE_WIFI_MULTICAST_STATE`**: UDP 및 RTP 라이브 멀티캐스트 스트림 패킷을 차단 없이 수신하기 위한 WifiManager 멀티캐스트 락 권한
- **`android.permission.RECORD_AUDIO`**: 사용자가 [AI 실시간 자막] 모드를 선택한 경우 온디바이스 음성인식을 가동하기 위한 선택적 권한 (음성 데이터는 외부로 저장/전송되지 않음)

---

### 5. 타사 SDK 및 광고 미포함 고지
- **광고 SDK 미포함**: 본 앱에는 어떠한 서드파티 인앱 광고(AdMob 등)가 포함되어 있지 않습니다.
- **추적기 미포함**: 사용자 행동 분석 및 마케팅 추적 트래커(Firebase Analytics 등)를 일체 사용하지 않습니다.

---

### 6. 개인정보 보호책임자 및 문의처
본 개인정보 처리방침에 관한 문의사항이 있으신 경우 아래 연락처로 문의해 주시기 바랍니다.
- **개발자/운영자**: Sideway
- **이메일**: `im.jey.free@gmail.com`
- **GitHub 저장소**: [https://github.com/ImJeyFree/sideway-ts-player](https://github.com/ImJeyFree/sideway-ts-player)

---

## 🇺🇸 [English Version]

### 1. Privacy Overview
**Sideway TS Player** ("the App") respects and protects user privacy. The App complies with the Personal Information Protection Act and Google Play Developer Policies. **The App does NOT collect, store, or transmit any personal user information, location data, contacts, or device identifiers to external servers.**

---

### 2. Information Collection and Usage
The App does not require registration or login and collects no personal information whatsoever.
- **Personal Data Collected**: None.
- **External Data Transmission**: None. No user data is transmitted to external servers.

---

### 3. Local Data Storage
All data configured by the user in the App is stored exclusively on the user's local device storage (`SharedPreferences` and local media directories) and never on external servers.
1. **App Environment Settings (`SharedPreferences`)**:
   - Local TV tuner server address (IP:Port), protocol selections (HTTP/RTSP/RTP/UDP), caption, and language preference settings configured directly by the user.
2. **Broadcast Recording Files (PVR .ts files)**:
   - Recorded video files created when the user presses the [Record] button during live broadcast viewing are stored exclusively in the local folder (`Movies/SidewayPlayer`) on the device.

---

### 4. App Permissions Usage
The App requests only permissions essential for live broadcast stream communication with the user's local TV tuner server.
- **`android.permission.INTERNET`**: Required to receive live media streams from the TV tuner server on the user's local home Wi-Fi network.
- **`android.permission.ACCESS_NETWORK_STATE`**: Used to check Wi-Fi network connectivity status.
- **`android.permission.CHANGE_WIFI_MULTICAST_STATE`**: Used to acquire a WifiManager Multicast Lock to receive UDP and RTP live multicast stream packets without filtering.
- **`android.permission.RECORD_AUDIO`**: Optional permission used solely for on-device AI speech recognition when the user enables [AI Live Caption] mode. Audio data is processed locally on-device and is never stored or transmitted externally.

---

### 5. No Third-Party SDKs or Advertising
- **No Advertising SDKs**: The App contains no third-party advertising SDKs (e.g., AdMob).
- **No Trackers**: The App contains no user behavior analytics or marketing tracking tools (e.g., Firebase Analytics).

---

### 6. Contact Information
If you have any questions regarding this Privacy Policy, please contact:
- **Developer/Operator**: Sideway
- **Email**: `im.jey.free@gmail.com`
- **GitHub Repository**: [https://github.com/ImJeyFree/sideway-ts-player](https://github.com/ImJeyFree/sideway-ts-player)
