/**
 * @file TsBroadcaster.h
 * @brief 1:N 멀티 클라이언트 MPEG-TS 데이터 복제 전송 (Fan-out Hub) 및 링버퍼
 * @author Sideway Team
 * @date 2026-09-16
 */

#pragma once

#include "Common.h"
#include <vector>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <unordered_set>
#include <bitset>

/**
 * @class TsSubscriber
 * @brief 개별 스트리밍 클라이언트 전용 스레드 안전 링버퍼 수신 큐
 */
class TsSubscriber {
public:
    /**
     * @brief TsSubscriber 생성자
     * @param capacity 전용 링버퍼 용량 (기본: 4MB)
     */
    TsSubscriber(size_t capacity = 1024 * 1024 * 4);

    /** @brief TsSubscriber 소멸자 */
    ~TsSubscriber();

    /**
     * @brief 링버퍼에 수신 TS 패킷 데이터 푸시
     * @param data TS 바이너리 데이터
     * @param size 데이터 크기 (바이트)
     */
    void PushData(const uint8_t* data, size_t size);

    /**
     * @brief 링버퍼에서 클라이언트 송출용 데이터 팝 (타임아웃 대기 지원)
     * @param outBuf [out] 출력 버퍼
     * @param size 읽을 크기
     * @param timeoutMs 대기 타임아웃 (ms)
     * @return 성공 시 true, 타임아웃/종료 시 false
     */
    bool PopData(uint8_t* outBuf, size_t size, unsigned timeoutMs = 1000);

    /** @brief 수신 큐 정지 및 대기 스레드 깨우기 */
    void Stop();

    /** @brief 링버퍼 헤드/테일 초기화 */
    void Reset();

private:
    std::vector<uint8_t> m_ringBuffer;         ///< 링버퍼 메모리
    size_t m_capacity;                          ///< 버퍼 최대 용량
    size_t m_head = 0;                          ///< 읽기 위치 (Head)
    size_t m_tail = 0;                          ///< 쓰기 위치 (Tail)
    size_t m_count = 0;                         ///< 현재 저장된 바이트 수
    std::mutex m_mutex;                         ///< 버퍼 동기화 뮤텍스
    std::condition_variable m_cv;               ///< 조건 변수
    bool m_active = true;                       ///< 활성 상태 플래그
};

/**
 * @class TsBroadcaster
 * @brief 수신된 MPEG-TS 스트림을 모든 활성 클라이언트(Subscriber)에게 복제 전송하는 Hub 클래스
 */
class TsBroadcaster {
public:
    /** @brief TsBroadcaster 생성자 */
    TsBroadcaster();

    /** @brief TsBroadcaster 소멸자 */
    ~TsBroadcaster();

    /**
     * @brief 신규 클라이언트 연결 시 전용 구독자(Subscriber) 생성 및 등록
     * @param bufferCapacity 구독자 전용 버퍼 용량 (기본: 4MB)
     * @return 등록된 구독자 스마트 포인터
     */
    std::shared_ptr<TsSubscriber> CreateSubscriber(size_t bufferCapacity = 1024 * 1024 * 4);

    /**
     * @brief 클라이언트 연결 종료 시 구독자 제거
     * @param subscriber 제거할 구독자 객체
     */
    void RemoveSubscriber(std::shared_ptr<TsSubscriber> subscriber);

    /**
     * @brief 수신 스레드에서 호출되어 모든 등록 클라이언트에게 1:N 동시 복제 전송 (Fan-out)
     * @param data 수신된 TS 바이너리 패킷
     * @param size 데이터 크기
     */
    void PushData(const uint8_t* data, size_t size);

    /** @brief 전체 브로드캐스트 정지 */
    void Stop();

    /** @brief 채널 변경 시 전체 구독자 링버퍼 초기화 */
    void Reset();

    /** @brief 현재 활성 구독자(클라이언트) 수 반환 */
    size_t GetSubscriberCount();

private:
    std::mutex m_subscribersMutex;              ///< 구독자 목록 동기화 뮤텍스
    std::unordered_set<std::shared_ptr<TsSubscriber>> m_subscribers; ///< 활성 구독자 집합
    bool m_active = true;                        ///< 활성 상태 플래그
};
