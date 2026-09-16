/**
 * @file QualityConfig.h
 * @brief 실시간 스트리밍 및 디코딩 품질 설정 JSON 영구 저장소
 * @author Sideway Team
 * @date 2026-09-16
 */

#pragma once

#include <filesystem>
#include <fstream>
#include <string>
#include <mutex>
#include "third_party/json.hpp"

/**
 * @struct QualitySettings
 * @brief 비디오 디인터레이싱, 멀티스레드, 네트워크 버퍼링 및 RTSP 전송 품질 설정
 */
struct QualitySettings {
    std::string deinterlaceMode = "bob";       ///< 디인터레이스 모드: "bob" (저사양 권장), "yadif" (고화질), "blend", "off"
    int avcodecThreads = 4;                    ///< FFmpeg 디코딩 스레드: 0(자동), 1, 2, 4(기본), 8
    int networkCachingMs = 1000;               ///< 네트워크 버퍼링 캐시(ms): 300, 500, 1000(기본), 1500, 2000
    std::string rtspTransport = "tcp";         ///< RTSP 전송 방식: "tcp" (인터리빙 권장), "udp" (유니캐스트)
    std::string hardwareAcceleration = "auto"; ///< HW 디코딩 가속: "auto", "on", "off"
};

inline void to_json(nlohmann::json& j, const QualitySettings& q) {
    j = nlohmann::json{
        {"deinterlaceMode", q.deinterlaceMode},
        {"avcodecThreads", q.avcodecThreads},
        {"networkCachingMs", q.networkCachingMs},
        {"rtspTransport", q.rtspTransport},
        {"hardwareAcceleration", q.hardwareAcceleration}
    };
}

inline void from_json(const nlohmann::json& j, QualitySettings& q) {
    if (j.contains("deinterlaceMode") && j["deinterlaceMode"].is_string()) {
        q.deinterlaceMode = j["deinterlaceMode"].get<std::string>();
    }
    if (j.contains("avcodecThreads") && j["avcodecThreads"].is_number()) {
        q.avcodecThreads = j["avcodecThreads"].get<int>();
    }
    if (j.contains("networkCachingMs") && j["networkCachingMs"].is_number()) {
        q.networkCachingMs = j["networkCachingMs"].get<int>();
    }
    if (j.contains("rtspTransport") && j["rtspTransport"].is_string()) {
        q.rtspTransport = j["rtspTransport"].get<std::string>();
    }
    if (j.contains("hardwareAcceleration") && j["hardwareAcceleration"].is_string()) {
        q.hardwareAcceleration = j["hardwareAcceleration"].get<std::string>();
    }
}

/**
 * @class QualityStore
 * @brief quality.json 파일 관리 및 메모리 캐싱 클래스
 */
class QualityStore {
public:
    explicit QualityStore(std::filesystem::path path) : m_path(std::move(path)) {
        Load();
    }

    QualitySettings Get() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_settings;
    }

    std::string ToJsonString() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        nlohmann::json j = m_settings;
        return j.dump(2);
    }

    bool UpdateFromJson(const std::string& jsonStr) {
        std::lock_guard<std::mutex> lock(m_mutex);
        try {
            nlohmann::json j = nlohmann::json::parse(jsonStr);
            QualitySettings newSettings = m_settings;
            from_json(j, newSettings);
            m_settings = newSettings;
            return SaveInternal();
        } catch (...) {
            return false;
        }
    }

    bool ResetToDefault() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_settings = QualitySettings{};
        return SaveInternal();
    }

private:
    bool Load() {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!std::filesystem::exists(m_path)) {
            return SaveInternal(); // 파일 없으면 기본값으로 생성
        }
        try {
            std::ifstream file(m_path);
            if (!file.is_open()) return false;
            nlohmann::json j;
            file >> j;
            from_json(j, m_settings);
            return true;
        } catch (...) {
            return false;
        }
    }

    bool SaveInternal() {
        try {
            std::ofstream file(m_path);
            if (!file.is_open()) return false;
            nlohmann::json j = m_settings;
            file << j.dump(2);
            return true;
        } catch (...) {
            return false;
        }
    }

    std::filesystem::path m_path;
    QualitySettings m_settings;
    mutable std::mutex m_mutex;
};
