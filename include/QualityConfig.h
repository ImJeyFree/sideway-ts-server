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
#include <map>
#include <chrono>
#include <stdexcept>
#include <windows.h>
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

// 허용된 키·타입·범위를 모두 검사하고 완전한 후보 값만 호출자에게 넘긴다.
inline void from_json(const nlohmann::json& j, QualitySettings& q) {
    if(!j.is_object())throw std::invalid_argument("설정은 JSON 객체여야 합니다");
    for(auto it=j.begin();it!=j.end();++it)
        if(it.key()!="deinterlaceMode"&&it.key()!="avcodecThreads"&&it.key()!="networkCachingMs"&&it.key()!="rtspTransport"&&it.key()!="hardwareAcceleration")
            throw std::invalid_argument("알 수 없는 설정");
    auto text=[&](const char* key,std::string& out,std::initializer_list<const char*> allowed){
        if(!j.contains(key))return;
        auto v=j.at(key).get<std::string>();bool valid=false;for(auto item:allowed)if(v==item)valid=true;
        if(!valid)throw std::invalid_argument("설정 값 오류");out=v;
    };
    auto integer=[&](const char* key,int& out,int low,int high){
        if(!j.contains(key))return;
        const auto& v=j.at(key);
        if(!v.is_number_integer() || v<low || v>high)throw std::invalid_argument("설정 숫자 범위 오류");
        out=v.get<int>();
    };
    text("deinterlaceMode",q.deinterlaceMode,{"bob","yadif","blend","off"});
    text("rtspTransport",q.rtspTransport,{"tcp","udp"});
    text("hardwareAcceleration",q.hardwareAcceleration,{"auto","on","off"});
    integer("avcodecThreads",q.avcodecThreads,0,64);
    integer("networkCachingMs",q.networkCachingMs,100,10000);
}

class QualityStore {
public:
    explicit QualityStore(std::filesystem::path path):m_path(std::move(path)){Load();}
    QualitySettings Get() const {std::lock_guard lock(m_mutex);return m_settings;}
    std::string ToJsonString() const {
        std::lock_guard lock(m_mutex);nlohmann::json j=m_settings;j["revision"]=m_revision;return j.dump();
    }
    bool UpdateFromJson(const std::string& text) {
        std::lock_guard lock(m_mutex);
        try {
            auto candidate=m_settings;from_json(nlohmann::json::parse(text),candidate);
            if(!SaveInternal(candidate,m_revision+1))return false;
            m_settings=candidate;++m_revision;return true;
        }catch(...){return false;}
    }
    bool ResetToDefault(){return UpdateFromJson(nlohmann::json(QualitySettings{}).dump());}
    // Player가 실제 적용에 성공한 뒤 명시적으로 보고한다. 서버가 적용을 추측하지 않는다.
    bool Acknowledge(const nlohmann::json& j) {
        std::lock_guard lock(m_mutex);
        try{
            auto id=j.at("clientId").get<std::string>();
            if(id.empty()||id.size()>64||!j.at("revision").is_number_integer()||j.at("revision")!=m_revision)return false;
            Prune();if(!m_applied.count(id)&&m_applied.size()>=64)return false;
            m_applied[id]={m_revision,std::chrono::steady_clock::now()};return true;
        }catch(...){return false;}
    }
    nlohmann::json ApplicationStatus() {
        std::lock_guard lock(m_mutex);Prune();auto clients=nlohmann::json::array();
        for(auto& [id,entry]:m_applied)clients.push_back({{"clientId",id},{"revision",entry.first},{"current",entry.first==m_revision},
            {"reportedAgeSeconds",std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now()-entry.second).count()}});
        return {{"revision",m_revision},{"scope","player"},{"confirmation","player_reported"},{"clients",clients}};
    }
private:
    void Prune(){auto now=std::chrono::steady_clock::now();for(auto it=m_applied.begin();it!=m_applied.end();)if(now-it->second.second>std::chrono::minutes(5))it=m_applied.erase(it);else ++it;}
    bool Load() {
        try {
            if(!std::filesystem::exists(m_path)){if(!SaveInternal(m_settings,1))return false;m_revision=1;return true;}
            if(std::filesystem::file_size(m_path)>16384)return false;
            std::ifstream input(m_path);nlohmann::json j;input>>j;
            uint64_t revision=1;
            if(j.contains("revision")){if(!j["revision"].is_number_unsigned()||j["revision"]==0)return false;revision=j["revision"].get<uint64_t>();j.erase("revision");}
            auto candidate=m_settings;from_json(j,candidate);m_settings=candidate;m_revision=revision;return true;
        }catch(...){return false;}
    }
    bool SaveInternal(const QualitySettings& candidate,uint64_t revision) {
        auto temporary=m_path;temporary+=L".tmp-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetTickCount64());
        try {
            if(!revision)return false;
            nlohmann::json j=candidate;j["revision"]=revision;
            {std::ofstream out(temporary,std::ios::binary|std::ios::trunc);out<<j.dump(2)<<'\n';out.flush();if(!out)throw std::runtime_error("설정 쓰기 실패");out.close();if(!out)throw std::runtime_error("설정 닫기 실패");}
            if(!MoveFileExW(temporary.c_str(),m_path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw std::runtime_error("설정 교체 실패");
            return true;
        }catch(...){std::error_code ignored;std::filesystem::remove(temporary,ignored);return false;}
    }
    std::filesystem::path m_path;
    QualitySettings m_settings;
    uint64_t m_revision=0;
    std::map<std::string,std::pair<uint64_t,std::chrono::steady_clock::time_point>> m_applied;
    mutable std::mutex m_mutex;
};
