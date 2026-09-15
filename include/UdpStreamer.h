#pragma once

#include "Common.h"
#include "TsBroadcaster.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <atomic>
#include <thread>
#include <string>
#include <memory>

class UdpStreamer {
public:
    UdpStreamer(TsBroadcaster& broadcaster,
                const std::string& multicastIp = DEFAULT_UDP_MULTICAST_ADDR,
                int port = DEFAULT_UDP_PORT);
    ~UdpStreamer();

    bool Start();
    void Stop();

    void SetEnabled(bool enabled);
    bool IsEnabled() const { return m_enabled.load(); }

    std::string GetTargetAddress() const;

private:
    void StreamLoop();

    TsBroadcaster& m_broadcaster;
    std::shared_ptr<TsSubscriber> m_subscriber;

    std::string m_multicastIp;
    int m_port;

    SOCKET m_socket = INVALID_SOCKET;
    std::string m_interfaceAddress;
    sockaddr_in m_destAddr{};

    std::atomic<bool> m_running{false};
    std::atomic<bool> m_enabled{true};
    std::thread m_workerThread;
};
