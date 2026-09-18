#include "TunerClient.h"
#include <stdexcept>
namespace {
template<class T>T Symbol(HMODULE m,const char* name){auto p=GetProcAddress(m,name);if(!p)throw std::runtime_error("튜너 DLL API 누락");return reinterpret_cast<T>(p);}
}
TunerClient::TunerClient(TsBroadcaster& h,const std::filesystem::path& file):hub(h) {
    wchar_t path[32768]{};GetModuleFileNameW(nullptr,path,32768);
    const auto dll=std::filesystem::path(path).parent_path()/L"SidewayTunerCore.dll";
    // 실행 파일 옆의 DLL만 지정하고 의존 DLL 검색도 해당 폴더와 시스템 폴더로 제한한다.
    module=LoadLibraryExW(dll.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);
    if(!module)throw std::runtime_error("SidewayTunerCore.dll을 실행 파일 옆에 설치하세요. DLL 또는 의존 런타임을 불러올 수 없습니다.");
    try {
        auto version=Symbol<decltype(&stc_api_version)>(module,"stc_api_version");
        if(version()!=STC_API_VERSION)throw std::runtime_error("튜너 DLL API 버전 불일치");
        auto create=Symbol<decltype(&stc_create)>(module,"stc_create");
        commandFn=Symbol<decltype(&stc_command)>(module,"stc_command");
        queryFn=Symbol<decltype(&stc_query)>(module,"stc_query");
        destroyFn=Symbol<decltype(&stc_destroy)>(module,"stc_destroy");
        const auto u8=file.u8string();std::string utf8(u8.begin(),u8.end());
        StcCreateOptions o{sizeof(o),STC_API_VERSION,utf8.c_str(),OnTs,this};
        if(create(&o,&handle)!=STC_OK)throw std::runtime_error("튜너 코어 생성 실패");
    }catch(...){FreeLibrary(module);module=nullptr;throw;}
}
// 콜백 스레드를 포함한 코어를 먼저 파괴한 뒤 DLL을 해제해야 실행 중인 코드가 사라지지 않는다.
TunerClient::~TunerClient(){if(handle)destroyFn(handle);if(module)FreeLibrary(module);}
void STC_CALL TunerClient::OnTs(void* context,const uint8_t* data,uint32_t size) {
    // DLL 소유 버퍼를 콜백 안에서 복사한다. C 경계 밖으로 예외를 보내지 않는다.
    try{static_cast<TunerClient*>(context)->hub.PushData(data,size);}catch(...){}
}
bool TunerClient::Command(nlohmann::json j) const{
    const auto op=j.value("op",std::string{});
    bool tuneChanged=false;
    if(op=="tune"){
        const auto before=Query("status");
        tuneChanged=before.value("currentChannel",0)!=j.value("channel",0) || before.value("isClearQam",false)!=j.value("qam",false);
    }
    const bool ok=commandFn(handle,j.dump().c_str())==STC_OK;
    // 방송 전환 명령이 성공하면 공개 송출 버퍼에 남아 있는 이전 방송 데이터를 비운다.
    if(ok && (op=="select" || op=="restore" || op=="scan" || op=="stop" || tuneChanged))hub.Reset();
    return ok;
}
nlohmann::json TunerClient::Query(const char* kind) const {
    std::vector<char> buffer(65536);
    // 크기 조회와 재조회 사이에도 목록이 커질 수 있으므로 여유 공간을 두고 제한 횟수만 재시도한다.
    for(int attempt=0;attempt<4;++attempt){uint32_t needed=0;auto result=queryFn(handle,kind,buffer.data(),uint32_t(buffer.size()),&needed);
        if(result==STC_OK)return nlohmann::json::parse(buffer.data());
        if(result!=STC_BUFFER_TOO_SMALL || needed>8*1024*1024)break;buffer.resize(needed+1024);
    }
    throw std::runtime_error("튜너 코어 상태 조회 실패");
}
