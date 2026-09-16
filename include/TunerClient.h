/**
 * @file TunerClient.h
 * @brief Sideway Tuner Core(C-API/DLL) 모듈과 연동하여 TV 튜너 제어 및 수신 TS 패킷을 브로드캐스터로 전달하는 IPC 클라이언트
 * @author Sideway Team
 * @date 2026-09-16
 */

#pragma once

#include "TsBroadcaster.h"
#include "third_party/json.hpp"
#include "TunerCoreApi.h"
#include <filesystem>
#include <windows.h>

/**
 * @class TunerClient
 * @brief sideway-tuner-core DLL(STC 모듈)을 동적 로드하여 BDA TV 튜너 하드웨어 제어 명령을 전달하고 수신 TS 스트림을 수신하는 C++ 클라이언트
 */
class TunerClient {
public:
    /**
     * @brief TunerClient 생성자 (DLL 동적 로드 및 콜백 등록 준비)
     * @param broadcaster TS 패킷을 전달할 공유 링버퍼 브로드캐스터
     * @param dllPath sideway-tuner-core DLL 파일 경로
     */
    TunerClient(TsBroadcaster& broadcaster, const std::filesystem::path& dllPath);

    /**
     * @brief TunerClient 소멸자 (DLL 핸들 및 튜너 세션 해제)
     */
    ~TunerClient();

    TunerClient(const TunerClient&) = delete;
    TunerClient& operator=(const TunerClient&) = delete;

    /**
     * @brief 튜너 코어 엔진으로 JSON 명령(Operation) 전송
     * @param command JSON 포맷 명령 객체 (예: { "op": "tune", "channel": 15 })
     * @return 성공 시 true, 실패 시 false
     */
    bool Command(nlohmann::json command) const;

    /**
     * @brief 튜너 코어 엔진의 상태 정보(Status) 조회
     * @param kind 조회 유형 문자열 (예: "status")
     * @return 튜너 상태 정보가 담긴 JSON 객체
     */
    nlohmann::json Query(const char* kind) const;

    /** @brief 튜너 하드웨어 초기화 명령 전송 */
    bool Initialize() { return Command({{"op", "initialize"}}); }

    /** @brief 튜너 TS 스트림 수신 시작 */
    bool Start() { return Command({{"op", "start"}}); }

    /** @brief 튜너 TS 스트림 수신 중지 */
    void Stop() { Command({{"op", "stop"}}); }

    /**
     * @brief 지정 채널 및 변조 방식으로 튜닝 실행
     * @param ch 물리 채널 번호 (예: 15)
     * @param qam QAM(케이블) 변조 여부 (false: ATSC 8VSB 지상파)
     * @return 성공 여부
     */
    bool Tune(int ch, bool qam) { return Command({{"op", "tune"}, {"channel", ch}, {"qam", qam}}); }

    /** @brief 현재 튜닝된 물리 채널 번호 반환 */
    int GetCurrentChannel() const { return Query("status").value("currentChannel", 0); }

    /** @brief Clear QAM 케이블 방송 여부 반환 */
    bool IsClearQam() const { return Query("status").value("isClearQam", false); }

    /** @brief 현재 TS 패킷 수신 중 여부 반환 */
    bool IsReceiving() const { return Query("status").value("receiving", false); }

    /** @brief 튜너 신호 고정(Lock) 성공 여부 반환 */
    bool IsLocked() const { return Query("status").value("tunerLocked", false); }

    /** @brief 신호 상태 수치 이용 가능 여부 반환 */
    bool IsSignalStatusAvailable() const { return Query("status").value("signalStatusAvailable", false); }

    /** @brief 실물 BDA TV 튜너 하드웨어 감지 여부 반환 */
    bool HasHardwareTuner() const { return Query("status").value("hasHardwareTuner", false); }

    /** @brief 가상 모드 여부 반환 */
    bool IsVirtualMode() const { return false; }

    /** @brief 지정 채널 지원 가능 여부 확인 */
    bool IsChannelSupported(int ch, bool qam) const {
        return ch >= 2 && ch <= ((qam || Query("status").value("cableInput", false)) ? 158 : 69);
    }

    /** @brief 감지된 튜너 장치 명칭 반환 */
    std::string GetDeviceName() const { return Query("status").value("deviceName", std::string{}); }

    /** @brief 튜너 하드웨어 장치 ID 반환 */
    std::string GetHardwareId() const { return Query("status").value("hardwareId", std::string{}); }

    /** @brief 튜너 드라이버 동작 상태 반환 */
    std::string GetDriverStatus() const { return Query("status").value("driverStatus", std::string{}); }

    /** @brief 현재 변조 방식 문자열 반환 (8VSB / 256QAM) */
    std::string GetModulation() const { return Query("status").value("modulation", std::string{}); }

    /** @brief 지원 방송 규격 목록 문자열 반환 */
    std::string GetSupportedStandards() const { return Query("status").value("supportedStandards", std::string{}); }

    /** @brief 튜너 누적 수신 바이너리 바이트 수 반환 */
    uint64_t GetHardwareBytes() const { return Query("status").value("hardwareBytes", uint64_t{}); }

    /** @brief 최근 수신 샘플 경과 시간 (ms) 반환 */
    int64_t GetLastSampleAgeMs() const { return Query("status").value("lastSampleAgeMs", int64_t{-1}); }

    /** @brief 실시간 비트레이트 (Mbps) 반환 */
    double GetBitrateMbps() const { return Query("status").value("bitrateMbps", 0.0); }

private:
    HMODULE module = nullptr;                   ///< 동적 로드된 DLL 모듈 핸들
    StcHandle handle = nullptr;                 ///< Tuner Core 인스턴스 핸들
    decltype(&stc_command) commandFn = nullptr; ///< C-API stc_command 함수 포인터
    decltype(&stc_query) queryFn = nullptr;     ///< C-API stc_query 함수 포인터
    decltype(&stc_destroy) destroyFn = nullptr; ///< C-API stc_destroy 함수 포인터
    TsBroadcaster& hub;                         ///< TS 브로드캐스터 참조

    /**
     * @brief 튜너 코어에서 수신된 TS 패킷 바이너리를 브로드캐스터로 전달하는 C-API 수신 콜백
     */
    static void STC_CALL OnTs(void* context, const uint8_t* data, uint32_t len);
};
