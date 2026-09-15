#pragma once
#include <stdint.h>
#ifdef _WIN32
#define STC_CALL __cdecl
#ifdef STC_BUILD
#define STC_API __declspec(dllexport)
#else
#define STC_API
#endif
#else
#define STC_CALL
#define STC_API
#endif
#ifdef __cplusplus
extern "C" {
#endif
#define STC_API_VERSION 1u
// DLL 내부 객체는 불투명 핸들로만 전달한다. 호출자는 직접 해제하거나 내부 구조를 참조하지 않는다.
typedef void* StcHandle;
// packets는 콜백이 반환될 때까지만 유효한 DLL 소유 버퍼이며 bytes는 188의 배수다.
// 콜백은 코어 송출 스레드에서 호출된다. 필요한 데이터는 복사하고 예외를 경계 밖으로 던지지 않는다.
typedef void (STC_CALL *StcTsCallback)(void* context, const uint8_t* packets, uint32_t bytes);
typedef struct StcCreateOptions {
    uint32_t size;
    uint32_t api_version;
    const char* channels_path_utf8;
    StcTsCallback on_ts;
    void* context;
} StcCreateOptions;
enum { STC_OK=0, STC_INVALID_ARGUMENT=1, STC_BUFFER_TOO_SMALL=2, STC_FAILED=3, STC_VERSION_MISMATCH=4 };
STC_API uint32_t STC_CALL stc_api_version(void);
STC_API int32_t STC_CALL stc_create(const StcCreateOptions*, StcHandle*);
STC_API int32_t STC_CALL stc_command(StcHandle, const char* json_utf8);
// required는 문자열 끝의 NUL을 포함한 바이트 수다. 버퍼가 작으면 STC_BUFFER_TOO_SMALL로 재할당을 요청한다.
STC_API int32_t STC_CALL stc_query(StcHandle, const char* kind, char* output, uint32_t capacity, uint32_t* required);
STC_API int32_t STC_CALL stc_last_error(StcHandle, char* output, uint32_t capacity, uint32_t* required);
// 동일 핸들의 다른 API 호출이 끝난 뒤 호출한다. 반환 후에는 콜백 context도 해제할 수 있다.
STC_API void STC_CALL stc_destroy(StcHandle);
#ifdef __cplusplus
}
#endif
