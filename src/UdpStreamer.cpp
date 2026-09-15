#include "UdpStreamer.h"
#include <iostream>
#include <vector>
#include <cstdlib>

UdpStreamer::UdpStreamer(TsBroadcaster& broadcaster, const std::string& multicastIp, int port)
    : m_broadcaster(broadcaster), m_multicastIp(multicastIp), m_port(port) {
}

UdpStreamer::~UdpStreamer() {
    Stop();
}

bool UdpStreamer::Start() {
    if (m_running) return true;

    // UDP 소켓 생성
    m_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (m_socket == INVALID_SOCKET) {
        std::cerr << "[UdpStreamer] UDP 소켓 생성 실패: " << WSAGetLastError() << std::endl;
        return false;
    }

    auto setOption = [this](int level, int option, const void* value, int size) {
        if (setsockopt(m_socket, level, option, static_cast<const char*>(value), size) == 0) return true;
        std::cerr << "[UdpStreamer] 소켓 옵션 실패: " << WSAGetLastError() << std::endl;
        closesocket(m_socket);
        m_socket = INVALID_SOCKET;
        return false;
    };
    // 다중 NIC 환경에서는 송신 인터페이스를 명시할 수 있다. 미지정 시 운영체제 라우팅을 따른다.
    if (const char* address = std::getenv("TS_MULTICAST_INTERFACE")) {
        in_addr local{};
        if (inet_pton(AF_INET, address, &local) != 1) {
            std::cerr << "[UdpStreamer] TS_MULTICAST_INTERFACE IPv4 주소 오류" << std::endl;
            closesocket(m_socket); m_socket = INVALID_SOCKET; return false;
        }
        if (!setOption(IPPROTO_IP, IP_MULTICAST_IF, &local, sizeof(local))) return false;
        m_interfaceAddress = address;
    }
    // 멀티캐스트 TTL 1 설정 (로컬 서브넷 내 전송)
    DWORD ttl = 1;
    if (!setOption(IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl))) return false;

    // 멀티캐스트 로컬 루프백 활성화 (동일 PC의 VLC 수신 허용)
    BOOL loopOpt = TRUE;
    if (!setOption(IPPROTO_IP, IP_MULTICAST_LOOP, &loopOpt, sizeof(loopOpt))) return false;

    // 브로드캐스트 허용 플래그
    BOOL broadcastOpt = TRUE;
    if (!setOption(SOL_SOCKET, SO_BROADCAST, &broadcastOpt, sizeof(broadcastOpt))) return false;

    // 송신 버퍼 512KB 확장
    int sndbuf = 1024 * 512;
    if (!setOption(SOL_SOCKET, SO_SNDBUF, &sndbuf, sizeof(sndbuf))) return false;

    // 목적지 주소 설정
    std::memset(&m_destAddr, 0, sizeof(m_destAddr));
    m_destAddr.sin_family = AF_INET;
    m_destAddr.sin_port = htons(static_cast<u_short>(m_port));
    if (inet_pton(AF_INET, m_multicastIp.c_str(), &m_destAddr.sin_addr) != 1) {
        closesocket(m_socket); m_socket = INVALID_SOCKET; return false;
    }

    // 1:N 브로드캐스터로부터 UDP 전용 구독자 생성
    m_subscriber = m_broadcaster.CreateSubscriber(1024 * 1024 * 2); // 2MB 버퍼

    m_running = true;
    m_workerThread = std::thread(&UdpStreamer::StreamLoop, this);

    std::cout << "[UdpStreamer] UDP 멀티캐스트 스트리머 가동 -> udp://"
              << m_multicastIp << ":" << m_port << " (MTU 1316B 단위 송출)" << std::endl;
    return true;
}

void UdpStreamer::Stop() {
    if (!m_running) return;
    m_running = false;

    if (m_subscriber) {
        m_broadcaster.RemoveSubscriber(m_subscriber);
    }

    if (m_workerThread.joinable()) {
        m_workerThread.join();
    }
    m_subscriber.reset();
    if (m_socket != INVALID_SOCKET) {
        closesocket(m_socket);
        m_socket = INVALID_SOCKET;
    }

    std::cout << "[UdpStreamer] UDP 멀티캐스트 스트리머 정지" << std::endl;
}

void UdpStreamer::SetEnabled(bool enabled) {
    m_enabled = enabled;
    std::cout << "[UdpStreamer] UDP 송출 상태 변경: " << (enabled ? "ON" : "OFF") << std::endl;
}

std::string UdpStreamer::GetTargetAddress() const {
    return m_multicastIp + ":" + std::to_string(m_port);
}

void UdpStreamer::StreamLoop() {
    // 표준 IPTV 스트리밍 규격: 188바이트 * 7개 = 1316 바이트 단위 송출 (MTU 1500 이내)
    std::vector<uint8_t> buffer(UDP_DATAGRAM_SIZE);

    while (m_running) {
        if (!m_subscriber || !m_subscriber->PopData(buffer.data(), UDP_DATAGRAM_SIZE)) {
            continue;
        }

        // UDP 기능이 켜져 있을 때만 실제 패킷 송출
        if (m_enabled.load()) {
            // 송신 성공은 로컬 소켓의 접수 결과다. AP의 Wi-Fi 멀티캐스트 전달이나 수신 완료를 보장하지 않는다.
            int sent = sendto(m_socket,
                   reinterpret_cast<const char*>(buffer.data()),
                   static_cast<int>(UDP_DATAGRAM_SIZE),
                   0,
                   reinterpret_cast<sockaddr*>(&m_destAddr),
                   sizeof(m_destAddr));
            if (sent != static_cast<int>(UDP_DATAGRAM_SIZE)) {
                std::cerr << "[UdpStreamer] 송출 실패: " << WSAGetLastError() << std::endl;
                m_enabled = false;
            }
        }
    }
}
