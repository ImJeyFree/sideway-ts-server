#pragma once
#include "TunerClient.h"
// 케이블 연결 여부와 변조 방식은 독립적이다. 케이블에서도 8VSB를 수신할 수 있다.
struct ScanOptions {bool cable=true,qam=false;int first=2,last=135,waitMs=1800;};
// 웹 서버용 명령 어댑터. 실제 장치 제어와 스캔 작업의 수명은 비공개 코어가 관리한다.
class ScanClient {
public:
    explicit ScanClient(TunerClient& engine):engine(engine){}
    bool Load(){if(!engine.Command({{"op","load"}}))throw std::runtime_error("채널 파일 읽기 실패");return !Status().at("channels").empty();}
    bool Start(ScanOptions o){return engine.Command({{"op","scan"},{"cable",o.cable},{"qam",o.qam},{"first",o.first},{"last",o.last},{"waitMs",o.waitMs}});}
    void Cancel(){engine.Command({{"op","cancel"}});}
    void Shutdown(){engine.Command({{"op","shutdown"}});}
    void Stop(){engine.Stop();}
    bool RestoreSelected(){return engine.Command({{"op","restore"}});}
    bool IsScanning()const{return engine.Query("status").value("scanning",false);}
    bool Select(const std::string& id){return engine.Command({{"op","select"},{"id",id}});}
    bool TuneLegacy(int ch,bool qam){return engine.Tune(ch,qam);}
    nlohmann::json Status()const{return engine.Query("catalog");}
    std::string SelectedName()const{const auto s=Status();for(const auto& c:s.at("channels"))if(c.at("id")==s.at("selected"))return c.at("name").get<std::string>();return "선택된 방송 없음";}
private:TunerClient& engine;
};
