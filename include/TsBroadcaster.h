#pragma once

#include "Common.h"
#include <vector>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <unordered_set>
#include <bitset>

// 개별 클라이언트 전용 수신 버퍼 큐 (Subscriber)
class TsSubscriber {
public:
    TsSubscriber(size_t capacity = 1024 * 1024 * 4); // 기본 4MB 전용 버퍼
    ~TsSubscriber();

    void PushData(const uint8_t* data, size_t size);
    bool PopData(uint8_t* outBuf, size_t size, unsigned timeoutMs = 1000);
    void Stop();
    void Reset();

private:
    std::vector<uint8_t> m_ringBuffer;
    size_t m_capacity;
    size_t m_head = 0;
    size_t m_tail = 0;
    size_t m_count = 0;
    std::mutex m_mutex;
    std::condition_variable m_cv;
    bool m_active = true;
};

// 1:N 멀티 클라이언트 브로드캐스터 (Fan-out Hub)
class TsBroadcaster {
public:
    TsBroadcaster();
    ~TsBroadcaster();

    // 새 클라이언트 연결 시 전용 구독자 생성 및 등록
    std::shared_ptr<TsSubscriber> CreateSubscriber(size_t bufferCapacity = 1024 * 1024 * 4);

    // 클라이언트 연결 종료 시 구독자 제거
    void RemoveSubscriber(std::shared_ptr<TsSubscriber> subscriber);

    // 튜너 수신 스레드에서 호출 -> 접속된 모든 클라이언트에게 1:N 동시 복제 전송 (Fan-out)
    void PushData(const uint8_t* data, size_t size);

    // 전체 정지
    void Stop();

    // 전체 버퍼 초기화 (채널 변경 시)
    void Reset();

    // 현재 활성 구독자 수
    size_t GetSubscriberCount();

private:
    std::mutex m_subscribersMutex;
    std::unordered_set<std::shared_ptr<TsSubscriber>> m_subscribers;
    bool m_active = true;

};
