#include "HttpStreamer.h"
#include "ScanClient.h"
#include "UdpStreamer.h"
#include <iostream>
#include <stdexcept>
#include <cstring>
#include <future>
static void Require(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
}
static std::vector<uint8_t> Packets(size_t n) {
    std::vector<uint8_t> data(n * TS_PACKET_SIZE);
    for (size_t i = 0; i < n; ++i) {
        std::fill_n(data.data() + i * TS_PACKET_SIZE, TS_PACKET_SIZE, static_cast<uint8_t>(i));
        data[i * TS_PACKET_SIZE] = 0x47;
    }
    return data;
}
static std::string Request(const std::string& request) {
    SOCKET socket = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    Require(socket != INVALID_SOCKET, "client socket");
    DWORD timeout = 3000;
    setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(18080);
    inet_pton(AF_INET, "127.0.0.1", &address.sin_addr);
    Require(connect(socket, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0, "HTTP connect");
    // TCP에서 헤더가 여러 recv로 쪼개져 도착하는 경우도 처리해야 한다.
    for (char c : request) Require(send(socket, &c, 1, 0) == 1, "fragmented request");
    std::string response;
    char data[4096];
    int count;
    while ((count = recv(socket, data, sizeof(data), 0)) > 0) response.append(data, count);
    closesocket(socket);
    Require(count == 0, "HTTP did not close");
    return response;
}
int main(){try{
        const auto source = Packets(100);
        std::string received;
        int calls = 0;
        Require(HttpWire::SendAll(reinterpret_cast<const char*>(source.data()), source.size(),
            [&](const char* p, int n) {
                int count = (std::min)(n, (++calls % 17) + 1);
                received.append(p, count);
                return count;
            }), "partial send failed");
        Require(received.size() == source.size() && std::memcmp(received.data(), source.data(), source.size()) == 0,
            "partial send corrupted bytes");
        Require(!HttpWire::SendAll("abc", 3, [](const char*, int) { return 0; }), "zero send accepted");
        Require(!HttpWire::SendAll("abc", 3, [](const char*, int) { return -1; }), "socket error accepted");
        Require(HttpWire::JsonEscape("USB\\VID\"a\n\t") == "USB\\\\VID\\\"a\\u000a\\u0009", "JSON escaping");
        Require(HttpWire::JsonEscape("케이블") == "케이블", "UTF-8 changed");

        // 비정렬 용량, 초과 입력, 순환 경계, 오래된 패킷 버리기.
        TsSubscriber buffer(3 * TS_PACKET_SIZE + 17);
        auto five = Packets(5);
        buffer.PushData(five.data(), five.size());
        std::vector<uint8_t> out(3 * TS_PACKET_SIZE);
        Require(buffer.PopData(out.data(), out.size(), 10), "oversized push missing data");
        Require(std::memcmp(out.data(), five.data() + 2 * TS_PACKET_SIZE, out.size()) == 0, "wrong retained packets");
        buffer.PushData(five.data(), 2 * TS_PACKET_SIZE);
        Require(buffer.PopData(out.data(), TS_PACKET_SIZE, 10), "first packet missing");
        buffer.PushData(five.data() + 2 * TS_PACKET_SIZE, 3 * TS_PACKET_SIZE);
        Require(buffer.PopData(out.data(), out.size(), 10), "wrap pop failed");
        Require(std::memcmp(out.data(), five.data() + 2 * TS_PACKET_SIZE, out.size()) == 0, "wrap corrupted");
        Require(!buffer.PopData(out.data(), TS_PACKET_SIZE, 10), "empty pop did not time out");
        bool rejected = false;
        try { buffer.PushData(five.data(), 189); } catch (const std::invalid_argument&) { rejected = true; }
        Require(rejected, "unaligned push accepted");
        buffer.PushData(five.data(), TS_PACKET_SIZE);
        buffer.Reset();
        Require(!buffer.PopData(out.data(), TS_PACKET_SIZE, 10), "reset retained data");
        auto waiting = std::async(std::launch::async, [&] { return buffer.PopData(out.data(), TS_PACKET_SIZE, 10000); });
        buffer.Stop();
        Require(waiting.wait_for(std::chrono::seconds(1)) == std::future_status::ready && !waiting.get(),
            "stop did not wake receiver");
        TsBroadcaster hub;
        auto a = hub.CreateSubscriber(), b = hub.CreateSubscriber();
        hub.PushData(five.data(), five.size());
        Require(a->PopData(out.data(), out.size(), 10), "fanout A failed");
        Require(b->PopData(out.data(), out.size(), 10), "fanout B failed");
        hub.Stop();
        Require(!hub.CreateSubscriber()->PopData(out.data(), TS_PACKET_SIZE, 10), "stopped hub subscribed");
        WSADATA winsock{};
        Require(WSAStartup(MAKEWORD(2, 2), &winsock) == 0, "WSAStartup");
        {
            TsBroadcaster liveHub;
            TunerClient tuner(liveHub,"unused.json"); // 하드웨어를 열지 않는 HTTP 검증
            Require(!tuner.Start(), "uninitialized graph was started");
            Require(!tuner.IsReceiving() && tuner.GetHardwareBytes() == 0, "false hardware success");
            HttpStreamer http(liveHub, tuner, nullptr, 18080);
            UdpStreamer scanUdp(liveHub,"239.255.0.1",1234);
            ScanClient scanner(tuner);
            http.SetScanner(&scanner);
            Require(http.Start(), "HTTP start");
            const auto epg=Request("GET /api/epg HTTP/1.1\r\nHost: localhost\r\n\r\n");
            Require(epg.find("200 OK")!=std::string::npos && epg.find("Cache-Control: no-store")!=std::string::npos,"EPG HTTP headers");
            auto guide=nlohmann::json::parse(epg.substr(epg.find("\r\n\r\n")+4));
            Require(guide.at("schemaVersion")==1 && guide.at("events").empty() && guide.at("channel").is_null(),"EPG idle schema");
            Require(Request("POST /api/epg HTTP/1.1\r\nHost: localhost\r\n\r\n").find("405 Method Not Allowed")!=std::string::npos,"EPG method guard");
            Require(Request("GET /api/epg-other HTTP/1.1\r\nHost: localhost\r\n\r\n").find("404 Not Found")!=std::string::npos,"EPG exact route");
            Require(tuner.Command({{"op","test-epg-unavailable"}}),"mock EPG failure setup");
            Require(Request("GET /api/epg HTTP/1.1\r\nHost: localhost\r\n\r\n").find("503 Service Unavailable")!=std::string::npos,"old DLL EPG error");
            const auto status = Request("GET /api/status HTTP/1.1\r\nHost: localhost\r\n\r\n");
            Require(status.find("200 OK") != std::string::npos &&
                status.find("\"receiving\":false") != std::string::npos, "status response");
            Require(Request("GET /stream?ch=oops HTTP/1.1\r\nHost: localhost\r\n\r\n").find("400 Bad Request") != std::string::npos,
                "invalid channel response");
            Require(Request("GET /stream HTTP/1.1\r\nHost: localhost\r\n\r\n").find("503 Service Unavailable") != std::string::npos,
                "no tuner response");
            Require(Request("GET /api/tune?ch=159 HTTP/1.1\r\nHost: localhost\r\n\r\n").find("400 Bad Request") != std::string::npos,
                "out of range channel");
            Require(Request("GET /api/channels HTTP/1.1\r\nHost: localhost\r\n\r\n").find("\"channels\":[]")!=std::string::npos,"channel API empty list");
            Require(Request("POST /api/scan/start HTTP/1.1\r\nHost: localhost\r\nContent-Length: 0\r\n\r\n").find("400 Bad Request")!=std::string::npos,"unmarked scan POST accepted");
            Require(Request("POST /api/scan/start?first=2&last=999 HTTP/1.1\r\nHost: localhost\r\nX-TS-Action: 1\r\nContent-Length: 0\r\n\r\n").find("409")!=std::string::npos,"scan range not rejected");
            http.Stop();
        }
        WSACleanup();
        std::cout << "PASS: partial sends, JSON, TS overflow/wrap/alignment/reset/stop/fanout\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << '\n';
        return 1;
    }
}
