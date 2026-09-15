#pragma once
#include <filesystem>
#include <string>
#include <vector>
#include "third_party/json.hpp"

struct ChannelStream { int pid = 0, type = 0; };
struct Channel {
    int physicalChannel = 0, frequencyKHz = 0, serviceId = 0, pmtPid = 0, pcrPid = 0;
    bool cable = true, qam = false;
    std::string name;
    std::vector<ChannelStream> streams;
    std::string Key() const;
};
void to_json(nlohmann::json& j, const Channel& c);
void from_json(const nlohmann::json& j, Channel& c);
class ChannelStore {
public:
    explicit ChannelStore(std::filesystem::path path) : path(std::move(path)) {}
    std::vector<Channel> Load(std::string& selected, std::string& warning) const;
    void Save(const std::vector<Channel>& channels, const std::string& selected) const;
    const std::filesystem::path path;
};
