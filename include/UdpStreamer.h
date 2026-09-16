/**
 * @file UdpStreamer.h
 * @brief UDP 멀티캐스트 및 유니캐스트 MPEG-TS 실시간 방송 송출 엔진
 * @author Sideway Team
 * @date 2026-09-16
 */

#pragma once

#include "Common.h"
#include "TsBroadcaster.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <atomic>
#include <thread>
#include <string>
#include <memory>

/**
 * @class UdpStreamer
 * @brief 수신된 MPEG-TS 패킷을 UDP 멀티캐스트(239.255.0.1:1234)로 실시간 전송하는 스트리머 클래스
 */
class UdpStreamer {
public:
    /**
     * @brief UdpStreamer 생성자
     * @param broadcaster TS 패킷 브로드캐스터 참조
     * @param multicastIp 멀티캐스트 전송 타겟 IP (기본: 239.255.0.1)
     * @param port 멀티캐스트 전송 포트 (기본: 1234)
     */
    UdpStreamer(TsBroadcaster& broadcaster,
                const std::string& multicastIp = DEFAULT_UDP_MULTICAST_ADDR,
                int port = DEFAULT_UDP_PORT);

    /**
     * @brief UdpStreamer 소멸자 (작업 스레드 및 소켓 정리)
     */
    ~UdpStreamer();

    /**
     * @brief UDP 스트리밍 작업 스레드 생성 및 송출 시작
     * @return 성공 시 true, 실패 시 false
     */
    bool Start();

    /**
     * @brief UDP 스트리밍 송출 중지 및 스레드 종료
     */
    void Stop();

    /**
     * @brief UDP 송출 활성화/비활성화 상태 설정
     * @param enabled 활성화 여부
     */
    void SetEnabled(bool enabled);

    /**
     * @brief UDP 송출 활성화 여부 확인
     */
    bool IsEnabled() const { return m_enabled.load(); }

    /**
     * @brief UDP 송출 타겟 주소 문자열 반환 (예: "239.255.0.1:1234")
     */
    std::string GetTargetAddress() const;

private:
    /**
     * @brief 링버퍼 구독(Subscriber)으로부터 TS 패킷을 읽어 UDP 소켓으로 7개 패킷(1316바이트) 단위 송출 스레드 루프
     */
    void StreamLoop();

    TsBroadcaster& m_broadcaster;               ///< 공유 브로드캐스터 참조
    std::shared_ptr<TsSubscriber> m_subscriber; ///< TS 패킷 구독자 객체

    std::string m_multicastIp;                  ///< 멀티캐스트 타겟 IP 주소
    int m_port;                                 ///< 멀티캐스트 타겟 포트

    SOCKET m_socket = INVALID_SOCKET;           ///< UDP 전송 소켓 핸들
    std::string m_interfaceAddress;             ///< 네트워크 인터페이스 IP
    sockaddr_in m_destAddr{};                   ///< 목적지 소켓 주소 구조체

    std::atomic<bool> m_running{false};          ///< 스레드 가동 플래그
    std::atomic<bool> m_enabled{true};           ///< 송출 활성화 플래그
    std::thread m_workerThread;                 ///< UDP 송출 전용 작업 스레드
};
