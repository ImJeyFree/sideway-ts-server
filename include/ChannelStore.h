/**
 * @file ChannelStore.h
 * @brief ATSC TV 채널 주파수, 튜닝 설정 및 채널 맵 저장소
 * @author Sideway Team
 * @date 2026-09-16
 */

#pragma once

#include <filesystem>
#include <string>
#include <vector>
#include "third_party/json.hpp"

/**
 * @struct ChannelStream
 * @brief 채널 내부 개별 비디오/오디오/데이터 PID 및 스트림 유형 구조체
 */
struct ChannelStream {
    int pid = 0;   ///< MPEG-TS Packet Identifier (PID)
    int type = 0;  ///< Stream Type (예: 0x02 MPEG-2 Video, 0x81 AC-3 Audio)
};

/**
 * @struct Channel
 * @brief ATSC 디지털 TV 방송 채널 정보 및 튜닝 파라미터 구조체
 */
struct Channel {
    int physicalChannel = 0;  ///< 물리 채널 번호 (예: 15)
    int frequencyKHz = 0;     ///< 중심 주파수 (kHz)
    int serviceId = 0;        ///< ATSC Service ID (Program Number)
    int pmtPid = 0;           ///< PMT (Program Map Table) PID
    int pcrPid = 0;           ///< PCR (Program Clock Reference) PID
    bool cable = true;        ///< 케이블 입력 여부 (true: Cable, false: Antenna)
    bool qam = false;          ///< QAM 변조 여부 (true: Clear QAM, false: ATSC 8VSB)
    std::string name;         ///< 채널 서비스 명칭 (예: "KBS 1 HD")
    std::vector<ChannelStream> streams; ///< 채널 내 포함된 비디오/오디오 스트림 PID 목록

    /**
     * @brief 채널 식별 고유 키 문자열 생성
     */
    std::string Key() const;
};

/** @brief Channel 구조체를 JSON 객체로 직렬화 */
void to_json(nlohmann::json& j, const Channel& c);

/** @brief JSON 객체를 Channel 구조체로 역직렬화 */
void from_json(const nlohmann::json& j, Channel& c);

/**
 * @class ChannelStore
 * @brief ATSC 채널 데이터 파일(JSON)의 로드 및 저장 관리자
 */
class ChannelStore {
public:
    /**
     * @brief ChannelStore 생성자
     * @param path 채널 JSON 데이터 파일 경로
     */
    explicit ChannelStore(std::filesystem::path path) : path(std::move(path)) {}

    /**
     * @brief 저장된 채널 목록을 파일에서 로드
     * @param selected [out] 선택된 기본 채널 식별 키
     * @param warning [out] 경고 메시지
     * @return 로드된 채널 목록 벡터
     */
    std::vector<Channel> Load(std::string& selected, std::string& warning) const;

    /**
     * @brief 채널 목록을 JSON 파일로 영구 저장
     * @param channels 저장할 채널 목록
     * @param selected 현재 선택된 기본 채널 식별 키
     */
    void Save(const std::vector<Channel>& channels, const std::string& selected) const;

    const std::filesystem::path path; ///< 채널 JSON 파일 경로
};
