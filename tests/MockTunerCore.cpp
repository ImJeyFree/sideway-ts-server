#include "TunerCoreApi.h"
#include "third_party/json.hpp"
#include <cstring>
// 테스트 전용 모의 DLL. 실제 패킷을 생성하거나 장치를 열지 않는다.
#ifndef MOCK_API_VERSION
#define MOCK_API_VERSION STC_API_VERSION
#endif
uint32_t STC_CALL stc_api_version(){return MOCK_API_VERSION;}
int32_t STC_CALL stc_create(const StcCreateOptions* o,StcHandle* h){if(!o||!h)return STC_INVALID_ARGUMENT;*h=nullptr;if(o->api_version!=STC_API_VERSION)return STC_VERSION_MISMATCH;*h=new int(0);return STC_OK;}
int32_t STC_CALL stc_command(StcHandle h,const char* text){if(!h||!text)return STC_INVALID_ARGUMENT;try{auto op=nlohmann::json::parse(text).at("op").get<std::string>();if(op=="test-epg-unavailable"){*static_cast<int*>(h)=1;return STC_OK;}return op=="load"||op=="cancel"||op=="shutdown"||op=="stop"?STC_OK:STC_FAILED;}catch(...){return STC_FAILED;}}
int32_t STC_CALL stc_query(StcHandle h,const char* kind,char* out,uint32_t cap,uint32_t* needed){
 if(!h||!kind||!needed)return STC_INVALID_ARGUMENT;
 if(std::strcmp(kind,"epg")==0 && *static_cast<int*>(h))return STC_INVALID_ARGUMENT;
 nlohmann::json j;
 if(std::strcmp(kind,"status")==0)j={{"currentChannel",15},{"isClearQam",false},{"cableInput",false},{"receiving",false},{"driverStatus","테스트 장치 없음"}};
 else if(std::strcmp(kind,"catalog")==0)j={{"channels",nlohmann::json::array()},{"selected",""},{"scanning",false},{"state","idle"}};
 else if(std::strcmp(kind,"epg")==0)j={{"schemaVersion",1},{"state","idle"},{"channel",nullptr},{"events",nlohmann::json::array()},{"current",nullptr},{"next",nullptr}};
 else return STC_INVALID_ARGUMENT;
 auto s=j.dump();*needed=uint32_t(s.size()+1);if(!out||cap<*needed)return STC_BUFFER_TOO_SMALL;std::memcpy(out,s.c_str(),*needed);return STC_OK;
}
int32_t STC_CALL stc_last_error(StcHandle,char* out,uint32_t cap,uint32_t* needed){if(!needed)return STC_INVALID_ARGUMENT;*needed=1;if(!out||cap<1)return STC_BUFFER_TOO_SMALL;out[0]=0;return STC_OK;}
void STC_CALL stc_destroy(StcHandle h){delete static_cast<int*>(h);}
