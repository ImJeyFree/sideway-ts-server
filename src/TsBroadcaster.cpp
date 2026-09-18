#include "TsBroadcaster.h"
#include <cstring>
#include <algorithm>
#include <stdexcept>

// ============================================================================
// TsSubscriber 구현 (개별 클라이언트 독립 버퍼)
// ============================================================================

// 용량과 입출력을 모두 TS 패킷 단위로 맞춰 버퍼 순환이나 오래된 데이터 폐기 후에도 경계를 보존한다.
TsSubscriber::TsSubscriber(size_t capacity)
    : m_ringBuffer((capacity / TS_PACKET_SIZE) * TS_PACKET_SIZE),
      m_capacity((capacity / TS_PACKET_SIZE) * TS_PACKET_SIZE) {
    if (m_capacity < TS_PACKET_SIZE) throw std::invalid_argument("TS buffer too small");
}

TsSubscriber::~TsSubscriber() {
    Stop();
}

void TsSubscriber::PushData(const uint8_t* data, size_t size) {
    if (!data || size == 0) return;
    if (size % TS_PACKET_SIZE) throw std::invalid_argument("TS input must be packet aligned");

    std::unique_lock<std::mutex> lock(m_mutex);
    if (!m_active) return;
    if (size > m_capacity) {
        data += size - m_capacity;
        size = m_capacity;
    }

    // 특정 클라이언트의 수신이 느려서 버퍼가 꽉 찼을 경우 오래된 데이터를 드롭하여 실시간성 보장
    if (m_count + size > m_capacity) {
        size_t overflow = (m_count + size) - m_capacity;
        m_tail = (m_tail + overflow) % m_capacity;
        m_count -= overflow;
    }

    size_t firstPart = std::min(size, m_capacity - m_head);
    std::memcpy(&m_ringBuffer[m_head], data, firstPart);
    if (size > firstPart) {
        std::memcpy(&m_ringBuffer[0], data + firstPart, size - firstPart);
    }
    m_head = (m_head + size) % m_capacity;
    m_count += size;

    lock.unlock();
    m_cv.notify_one();
}

bool TsSubscriber::PopData(uint8_t* outBuf, size_t size, unsigned timeoutMs) {
    if (!outBuf || size == 0 || size > m_capacity || size % TS_PACKET_SIZE) return false;

    std::unique_lock<std::mutex> lock(m_mutex);
    if (!m_cv.wait_for(lock, std::chrono::milliseconds(timeoutMs), [this, size]() {
        return m_count >= size || !m_active;
    })) return false;

    if (!m_active) return false;

    size_t firstPart = std::min(size, m_capacity - m_tail);
    std::memcpy(outBuf, &m_ringBuffer[m_tail], firstPart);
    if (size > firstPart) {
        std::memcpy(outBuf + firstPart, &m_ringBuffer[0], size - firstPart);
    }
    m_tail = (m_tail + size) % m_capacity;
    m_count -= size;

    return true;
}

void TsSubscriber::Stop() {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_active = false;
    }
    m_cv.notify_all();
}

void TsSubscriber::Reset() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_head = 0;
    m_tail = 0;
    m_count = 0;
}

// ============================================================================
// TsBroadcaster 구현 (1:N Fan-out 허브)
// ============================================================================

TsBroadcaster::TsBroadcaster() = default;

TsBroadcaster::~TsBroadcaster() {
    Stop();
}

std::shared_ptr<TsSubscriber> TsBroadcaster::CreateSubscriber(size_t bufferCapacity) {
    auto subscriber = std::make_shared<TsSubscriber>(bufferCapacity);
    std::lock_guard<std::mutex> lock(m_subscribersMutex);
    if (m_active) {
        m_subscribers.insert(subscriber);
    } else {
        subscriber->Stop();
    }
    return subscriber;
}

void TsBroadcaster::RemoveSubscriber(std::shared_ptr<TsSubscriber> subscriber) {
    if (!subscriber) return;
    subscriber->Stop();

    std::lock_guard<std::mutex> lock(m_subscribersMutex);
    m_subscribers.erase(subscriber);
}

void TsBroadcaster::PushData(const uint8_t* data, size_t size) {
    if (!data || size == 0) return;

    std::lock_guard<std::mutex> lock(m_subscribersMutex);
    if (!m_active) return;
    for (const auto& subscriber : m_subscribers) {
        subscriber->PushData(data, size);
    }
}

void TsBroadcaster::Stop() {
    std::lock_guard<std::mutex> lock(m_subscribersMutex);
    m_active = false;
    for (const auto& subscriber : m_subscribers) {
        subscriber->Stop();
    }
    m_subscribers.clear();
}

void TsBroadcaster::Reset() {
    std::lock_guard<std::mutex> lock(m_subscribersMutex);
    for (const auto& subscriber : m_subscribers) {
        subscriber->Reset();
    }
}

size_t TsBroadcaster::GetSubscriberCount() {
    std::lock_guard<std::mutex> lock(m_subscribersMutex);
    return m_subscribers.size();
}

