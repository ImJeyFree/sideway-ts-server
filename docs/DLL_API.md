# 튜너 코어 DLL API v1

## 로딩과 호환성

Windows x64, C 호출 규약 `__cdecl`, `extern "C"`를 사용합니다. 공개 헤더는 `sdk/TunerCoreApi.h`입니다.
서버는 실행 파일 옆 DLL을 `LoadLibraryExW`로 로드하고 `stc_api_version()==1`을 확인합니다. DLL은 시스템 검색 디렉터리의 의존 DLL만 추가로 사용합니다. SDK의 import LIB를 사용하는 직접 연결 방식도 가능합니다.

DLL은 프로세스 내 모듈이며 보안 격리 경계가 아닙니다. 비공개 소스를 제공하지 않지만 역분석 불가능성을 보장하지 않습니다.

## 함수

| 함수 | 의미 |
|---|---|
| stc_api_version | 지원 API 버전 |
| stc_create | 옵션·UTF-8 채널 파일 경로·TS 콜백으로 핸들 생성. 장치를 열지는 않음 |
| stc_command | UTF-8 JSON 명령 실행. 성공 0 |
| stc_query | status 또는 catalog를 UTF-8 JSON으로 조회 |
| stc_last_error | 해당 핸들의 마지막 오류. 생성 실패는 동일 스레드에서 null 핸들로 조회 |
| stc_destroy | 콜백 스레드·스캔·장치를 종료하고 핸들 해제 |

상태 코드: 0 성공, 1 잘못된 인수, 2 출력 버퍼 부족, 3 작업 실패, 4 API 버전 불일치.
조회 함수의 required는 NUL을 포함한 바이트 수입니다. 출력 버퍼가 작으면 required 이상으로 확보해 재호출합니다. 스캔 중 목록 크기가 변할 수 있으므로 반복 확인합니다.

## 명령 JSON

공통 형식은 `{"op":"명령"}`입니다.

| op | 추가 값·동작 |
|---|---|
| initialize | 장치 초기화 |
| load | 채널 JSON 로드. 빈 목록도 정상이며 catalog로 확인 |
| restore | 저장된 선택 방송 복원 |
| scan | cable(bool), qam(bool), first(int), last(int), waitMs(int). 비동기 시작 |
| cancel | 스캔 취소 요청 |
| shutdown | 취소 후 스캔 스레드 종료까지 대기 |
| select | id(string): catalog의 방송 ID |
| tune | channel(int), qam(bool): 저장된 방송의 기존 물리 채널 선택 |
| start | 현재 장치 수신 시작 |
| stop | 진행 스캔 종료 후 장치 수신 정지 |

status는 currentChannel, isClearQam, cableInput, receiving, tunerLocked, signalStatusAvailable, hasHardwareTuner, deviceName, hardwareId, driverStatus, modulation, supportedStandards, hardwareBytes, lastSampleAgeMs, bitrateMbps, scanning을 제공합니다.

catalog는 channels, selected, scanning, state, message, completed, total, currentChannel, found, file을 제공합니다. channels 항목의 구조는 공개 `ChannelStore.h`를 참조합니다.

## 메모리·스레드·수명

- 옵션의 size와 api_version을 채우고, 입력 문자열은 호출이 반환될 때까지 유지합니다. 생성 시 경로는 DLL 내부에 복사됩니다.
- 핸들은 DLL에서 생성·파괴합니다. 호스트에서 delete하지 않습니다. STL·COM 객체를 경계로 넘기지 않습니다.
- TS 콜백은 188바이트 정렬된 패킷을 DLL 작업 스레드에서 전달합니다. 바이트 포인터는 콜백 동안만 유효합니다. 계속 사용할 데이터는 호스트에서 복사합니다.
- 콜백에서 코어 API를 다시 호출하거나 예외를 던지거나 장시간 대기하지 않습니다.
- 일반 명령·조회는 내부 직렬화됩니다. destroy는 다른 API 호출이 모두 끝난 뒤 단독 호출하며, 반환 후에는 콜백이 발생하지 않습니다.
- COM 초기화·장치 제어 스레드는 DLL이 소유합니다. DLL을 사용 중인 상태로 FreeLibrary하지 않습니다.
- 스캔 중 TS 콜백 송출은 중단되며 완료·취소 후 복원됩니다. HTTP·UDP 네트워크 소켓은 공개 서버가 소유합니다.
