#include "Common.h"
#include "TsBroadcaster.h"
#include "TunerClient.h"
#include "UdpStreamer.h"
#include "HttpStreamer.h"
#include "TrayApp.h"
#include "ScanClient.h"
#include <iostream>
#include <windows.h>
#include <objbase.h>
#include <csignal>
#include <shellapi.h>

static DWORD g_mainThreadId = 0;

static BOOL WINAPI ConsoleCtrlHandler(DWORD ctrlType) {
    if (ctrlType == CTRL_C_EVENT || ctrlType == CTRL_CLOSE_EVENT || ctrlType == CTRL_SHUTDOWN_EVENT) {
        // 자원 정리는 메인 스레드 한 곳에서 수행한다.
        PostThreadMessageW(g_mainThreadId, WM_QUIT, 0, 0);
        return TRUE;
    }
    return FALSE;
}

static int Run(int argc, char* argv[]) {
    // 시작 실패와 중복 실행 안내도 UTF-8로 출력한다.
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    wchar_t executable[32768]{};GetModuleFileNameW(nullptr,executable,32768);
    std::filesystem::path channelFile=std::filesystem::path(executable).parent_path()/L"channels.json";
    bool forceScan=false,noScan=false;
    int wideArgc=0;auto wideArgs=CommandLineToArgvW(GetCommandLineW(),&wideArgc);
    struct ArgsGuard { LPWSTR* p; ~ArgsGuard(){if(p)LocalFree(p);} } argsGuard{wideArgs};
    for(int i=1;i<argc;++i){
        std::string arg=argv[i];
        if(arg=="--scan")forceScan=true;
        else if(arg=="--no-scan")noScan=true;
        else if(arg=="--channels" && i+1<argc){++i;channelFile=(wideArgs && i<wideArgc)?std::filesystem::path(wideArgs[i]):std::filesystem::path(argv[i]);}
        else if(arg=="--help"){std::cout<<"사용법: sideway-ts-server [--scan | --no-scan] [--channels JSON경로]\n";return 0;}
        else{std::cerr<<"지원하지 않는 실행 옵션: "<<arg<<std::endl;return 2;}
    }
    if(forceScan && noScan){std::cerr<<"--scan과 --no-scan은 함께 사용할 수 없습니다\n";return 2;}
    // 튜너/네트워크를 열기 전에 동일 사용자 세션의 중복 실행을 차단한다.
    HANDLE instanceMutex = CreateMutexW(nullptr, FALSE, L"Local\\SidewayTsServer.SingleInstance");
    const DWORD mutexError = GetLastError();
    if (!instanceMutex || mutexError == ERROR_ALREADY_EXISTS) {
        std::cerr << "[Main] 서버가 이미 실행 중이거나 실행 잠금을 얻지 못했습니다." << std::endl;
        if (instanceMutex) CloseHandle(instanceMutex);
        return 2;
    }
    struct InstanceGuard { HANDLE handle; ~InstanceGuard() { CloseHandle(handle); } } guard{instanceMutex};
    std::cout << "==================================================" << std::endl;
    std::cout << "    📡 Sideway TS Dual Streaming Server v1.1     " << std::endl;
    std::cout << "   1:N Multicast Hub & HTTP / UDP Dual Stream     " << std::endl;
    std::cout << "==================================================" << std::endl;

    // 1. Winsock 초기화
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "[-] WSAStartup 실패!" << std::endl;
        return 1;
    }
    struct WinsockGuard { ~WinsockGuard() { WSACleanup(); } } winsockGuard;

    // 2. COM 라이브러리 초기화 (BDA 및 DirectShow용)
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hr)) {
        std::cerr << "[-] CoInitializeEx 실패!" << std::endl;
        return 1;
    }
    struct ComGuard { ~ComGuard() { CoUninitialize(); } } comGuard;

    // 콘솔 종료 시그널 등록
    g_mainThreadId = GetCurrentThreadId();
    MSG startupMessage{};
    PeekMessageW(&startupMessage, nullptr, WM_USER, WM_USER, PM_NOREMOVE);
    SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE);

    // 3. 코어 컴포넌트 생성 및 연결 (1:N Fan-out 브로드캐스터 기반)
    TsBroadcaster broadcaster;
    TunerClient tuner(broadcaster,channelFile);
    UdpStreamer udpStreamer(broadcaster, DEFAULT_UDP_MULTICAST_ADDR, DEFAULT_UDP_PORT);
    HttpStreamer streamer(broadcaster, tuner, &udpStreamer, DEFAULT_SERVER_PORT);
    ScanClient scanner(tuner);
    streamer.SetScanner(&scanner);
    bool loaded=false;
    try{loaded=scanner.Load();}catch(const std::exception& e){std::cerr<<"채널 파일 읽기 실패: "<<e.what()<<std::endl;return 1;}

    // 4. 튜너 및 듀얼 스트리밍 엔진 시작
    if (!tuner.Initialize()) {
        std::cerr << "[-] 튜너 초기화 실패" << std::endl;
    }
    if (!udpStreamer.Start()) {
        std::cerr << "[Main] UDP 시작 실패" << std::endl;
        return 1;
    }

    if (!streamer.Start()) {
        std::cerr << "[-] HTTP 서버 시작 실패" << std::endl;
        udpStreamer.Stop();
        tuner.Stop();
        return 1;
    }
    // 명시적 스캔 요청을 우선하고, 목록이 없을 때만 자동 스캔한다. 저장된 목록은 선택 방송을 복원한다.
    if(forceScan || (!loaded && !noScan))scanner.Start(ScanOptions{});
    else if(loaded && !scanner.RestoreSelected())std::cerr<<"저장된 방송 수신 실패: 웹에서 다시 선택하거나 스캔하세요\n";

    // 5. 윈도우 시스템 트레이 앱 초기화
    HINSTANCE hInstance = GetModuleHandle(nullptr);
    TrayApp tray(hInstance, DEFAULT_SERVER_PORT, [&]() {
        std::cout << "[Main] 트레이 메뉴에서 서버 종료 요청됨." << std::endl;
    });

    if (tray.Initialize()) {
        tray.ShowNotification(
            L"Sideway TS 서버 시작됨",
            L"서버가 정상 가동되었습니다. 트레이 아이콘을 더블클릭하면 웹 대시보드가 열립니다."
        );
    }

    std::cout << "\n✅ 서버가 성공적으로 가동되었습니다!" << std::endl;
    std::cout << "🌐 웹 대시보드 : http://localhost:" << DEFAULT_SERVER_PORT << "/" << std::endl;
    std::cout << "📱 안드로이드: 대시보드의 서버 LAN 주소를 사용하세요." << std::endl;
    std::cout << "💡 작업표시줄 시스템 트레이에 아이콘이 상주합니다. (종료: Ctrl+C 또는 트레이 우클릭 Exit)\n" << std::endl;

    // 6. 트레이 윈도우 메시지 루프 가동 (메인 스레드 블로킹)
    tray.RunMessageLoop();

    // 새 HTTP 작업을 차단한 뒤 스캔 스레드와 UDP 송출을 종료하고 마지막으로 장치를 정지한다.
    // 7. 자원 정리
    streamer.Stop();
    scanner.Shutdown();
    udpStreamer.Stop();
    tuner.Stop();

    SetConsoleCtrlHandler(ConsoleCtrlHandler, FALSE);

    std::cout << "[Main] 서버가 안전하게 종료되었습니다. 안녕히 가세요! 🐾" << std::endl;
    return 0;
}

int main(int argc,char* argv[]){try{return Run(argc,argv);}catch(const std::exception& e){std::cerr<<e.what()<<std::endl;return 1;}}
