#pragma once
#include "TsBroadcaster.h"
#include "third_party/json.hpp"
#include "TunerCoreApi.h"
#include <filesystem>
#include <windows.h>

class TunerClient {
public:
    TunerClient(TsBroadcaster&,const std::filesystem::path&);
    ~TunerClient();
    TunerClient(const TunerClient&)=delete;
    TunerClient& operator=(const TunerClient&)=delete;
    bool Command(nlohmann::json command) const;
    nlohmann::json Query(const char* kind) const;
    bool Initialize(){return Command({{"op","initialize"}});}
    bool Start(){return Command({{"op","start"}});}
    void Stop(){Command({{"op","stop"}});}
    bool Tune(int ch,bool qam){return Command({{"op","tune"},{"channel",ch},{"qam",qam}});}
    int GetCurrentChannel() const{return Query("status").value("currentChannel",0);}
    bool IsClearQam() const{return Query("status").value("isClearQam",false);}
    bool IsReceiving() const{return Query("status").value("receiving",false);}
    bool IsLocked() const{return Query("status").value("tunerLocked",false);}
    bool IsSignalStatusAvailable() const{return Query("status").value("signalStatusAvailable",false);}
    bool HasHardwareTuner() const{return Query("status").value("hasHardwareTuner",false);}
    bool IsVirtualMode() const{return false;}
    bool IsChannelSupported(int ch,bool qam) const{return ch>=2 && ch<=((qam || Query("status").value("cableInput",false))?158:69);}
    std::string GetDeviceName() const{return Query("status").value("deviceName",std::string{});}
    std::string GetHardwareId() const{return Query("status").value("hardwareId",std::string{});}
    std::string GetDriverStatus() const{return Query("status").value("driverStatus",std::string{});}
    std::string GetModulation() const{return Query("status").value("modulation",std::string{});}
    std::string GetSupportedStandards() const{return Query("status").value("supportedStandards",std::string{});}
    uint64_t GetHardwareBytes() const{return Query("status").value("hardwareBytes",uint64_t{});}
    int64_t GetLastSampleAgeMs() const{return Query("status").value("lastSampleAgeMs",int64_t{-1});}
    double GetBitrateMbps() const{return Query("status").value("bitrateMbps",0.0);}
private:
    HMODULE module=nullptr;
    StcHandle handle=nullptr;
    decltype(&stc_command) commandFn=nullptr;
    decltype(&stc_query) queryFn=nullptr;
    decltype(&stc_destroy) destroyFn=nullptr;
    TsBroadcaster& hub;
    static void STC_CALL OnTs(void*,const uint8_t*,uint32_t);
};
