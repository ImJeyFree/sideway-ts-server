#include "ChannelStore.h"
#include <windows.h>
#include <fstream>
#include <set>
#include <stdexcept>
using nlohmann::json;
// 같은 주파수의 여러 방송을 구별하며, 이름이 바뀌어도 선택 상태를 유지하는 식별자다.
std::string Channel::Key() const {
    return std::string(cable ? "c" : "a") + (qam ? "q" : "v") + "-" + std::to_string(frequencyKHz) + "-" + std::to_string(serviceId);
}
void to_json(json& j, const Channel& c) {
    j = {{"id",c.Key()}, {"physicalChannel",c.physicalChannel}, {"frequencyKHz",c.frequencyKHz},
        {"input",c.cable?"cable":"antenna"}, {"modulation",c.qam?"QAM256":"8VSB"},
        {"serviceId",c.serviceId},{"name",c.name},{"pmtPid",c.pmtPid},{"pcrPid",c.pcrPid},{"streams",json::array()}};
    for (const auto& s:c.streams) j["streams"].push_back({{"pid",s.pid},{"type",s.type}});
}
void from_json(const json& j, Channel& c) {
    c.physicalChannel=j.at("physicalChannel").get<int>(); c.frequencyKHz=j.at("frequencyKHz").get<int>();
    const auto input=j.at("input").get<std::string>(), mod=j.at("modulation").get<std::string>();
    if ((input!="cable" && input!="antenna") || (mod!="8VSB" && mod!="QAM256")) throw std::runtime_error("수신 방식 오류");
    c.cable=input=="cable"; c.qam=mod=="QAM256";
    c.serviceId=j.at("serviceId").get<int>(); c.pmtPid=j.at("pmtPid").get<int>(); c.pcrPid=j.at("pcrPid").get<int>();
    c.name=j.at("name").get<std::string>(); c.streams.clear();
    for (const auto& s:j.at("streams")) c.streams.push_back({s.at("pid").get<int>(),s.at("type").get<int>()});
    if (c.physicalChannel<2 || c.physicalChannel>(c.cable?158:69) || c.frequencyKHz<40000 || c.frequencyKHz>1000000 ||
        c.serviceId<1 || c.serviceId>65535 || c.pmtPid<16 || c.pmtPid>8190 || c.pcrPid<0 || c.pcrPid>8191 ||
        c.name.size()>512 || c.streams.empty() || c.streams.size()>128) throw std::runtime_error("채널 값 범위 오류");
    for (const auto& s:c.streams) if (s.pid<16 || s.pid>8190 || s.type<0 || s.type>255) throw std::runtime_error("PID 오류");
}
std::vector<Channel> ChannelStore::Load(std::string& selected, std::string& warning) const {
    selected.clear(); warning.clear();
    if (!std::filesystem::exists(path)) return {};
    if (std::filesystem::file_size(path)==0) return {};
    try {
        if (std::filesystem::file_size(path)>8*1024*1024) throw std::runtime_error("JSON 파일이 너무 큼");
        std::ifstream in(path); const auto j=json::parse(in);
        if (j.is_null() || (j.is_object() && j.empty()) || (j.is_array() && j.empty())) return {};
        if (j.at("version")!=1) throw std::runtime_error("지원하지 않는 채널 JSON 버전");
        if (!j.at("channels").is_array()) throw std::runtime_error("channels가 배열이 아닙니다");
        std::vector<Channel> channels; std::set<std::string> keys;
        for (const auto& entry:j.at("channels")) {
            try { auto c=entry.get<Channel>(); if (keys.insert(c.Key()).second) channels.push_back(std::move(c)); }
            catch (const std::exception&) { warning="일부 잘못된 채널을 제외했습니다"; }
        }
        selected=j.value("selected",std::string{});
        if(channels.empty() && !j.at("channels").empty())throw std::runtime_error("유효한 채널 항목이 없습니다");
        if (!channels.empty() && !keys.count(selected)) selected=channels.front().Key();
        return channels;
    } catch (const std::exception& e) {
        const auto backup=path.wstring()+L".invalid-"+std::to_wstring(GetTickCount64());
        std::filesystem::copy_file(path, backup);
        warning=std::string("손상된 JSON을 별도 보관했습니다: ")+e.what(); return {};
    }
}
void ChannelStore::Save(const std::vector<Channel>& channels, const std::string& selected) const {
    if (channels.empty()) throw std::runtime_error("빈 스캔 결과로 채널 파일을 덮어쓰지 않습니다");
    json j={{"version",1},{"selected",selected},{"channels",channels}};
    // 기존 JSON을 직접 잘라 쓰지 않는다. 임시 파일 쓰기가 성공한 뒤에만 원본을 교체한다.
    auto temporary=path; temporary+=L".tmp";
    if (!path.parent_path().empty()) std::filesystem::create_directories(path.parent_path());
    { std::ofstream out(temporary,std::ios::binary|std::ios::trunc); out << j.dump(2) << '\n'; out.flush();
      if (!out) throw std::runtime_error("채널 JSON 쓰기 실패"); }
    if (!MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))
        throw std::runtime_error("채널 JSON 교체 실패");
}
