#include "TrayApp.h"
#include <iostream>

constexpr UINT WM_TRAYICON = WM_USER + 1;
constexpr UINT ID_TRAY_OPEN_WEB = 1001;
constexpr UINT ID_TRAY_EXIT = 1002;

TrayApp* TrayApp::s_instance = nullptr;

TrayApp::TrayApp(HINSTANCE hInstance, int port, std::function<void()> onExitCallback)
    : m_hInstance(hInstance), m_port(port), m_onExitCallback(onExitCallback) {
    s_instance = this;
}

TrayApp::~TrayApp() {
    Stop();
    s_instance = nullptr;
}

bool TrayApp::Initialize() {
    const wchar_t CLASS_NAME[] = L"SidewayTsServerTrayClass";

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = TrayApp::WndProc;
    wc.hInstance = m_hInstance;
    wc.lpszClassName = CLASS_NAME;

    if (!RegisterClassExW(&wc)) {
        // 이미 등록된 경우 통과
    }

    m_hwnd = CreateWindowExW(
        0, CLASS_NAME, L"Sideway TS Server",
        0, 0, 0, 0, 0,
        HWND_MESSAGE, nullptr, m_hInstance, nullptr
    );

    if (!m_hwnd) {
        std::cerr << "[TrayApp] 메시지 윈도우 생성 실패" << std::endl;
        return false;
    }

    // 트레이 아이콘 등록
    std::memset(&m_nid, 0, sizeof(m_nid));
    m_nid.cbSize = sizeof(NOTIFYICONDATAW);
    m_nid.hWnd = m_hwnd;
    m_nid.uID = 1;
    m_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    m_nid.uCallbackMessage = WM_TRAYICON;
    m_nid.hIcon = LoadIcon(nullptr, IDI_APPLICATION); // 기본 윈도우 아이콘
    wcscpy_s(m_nid.szTip, L"Sideway TS Streaming Server");

    m_running = true;

    if (!Shell_NotifyIconW(NIM_ADD, &m_nid)) {
        std::cerr << "[TrayApp] 알림: 현재 세션에서 트레이 아이콘 등록 불가 (콘솔/헤드리스 모드로 계속 실행)" << std::endl;
    } else {
        std::cout << "[TrayApp] 시스템 트레이 아이콘 등록 완료 (더블클릭: 대시보드 열기)" << std::endl;
    }

    return true;
}


void TrayApp::RunMessageLoop() {
    MSG msg;
    while (m_running && GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

void TrayApp::Stop() {
    if (!m_running) return;
    m_running = false;

    if (m_nid.hWnd) {
        Shell_NotifyIconW(NIM_DELETE, &m_nid);
        m_nid.hWnd = nullptr;
    }

    if (m_hwnd) {
        PostMessage(m_hwnd, WM_CLOSE, 0, 0);
        m_hwnd = nullptr;
    }
}

void TrayApp::ShowNotification(const std::wstring& title, const std::wstring& message) {
    if (!m_hwnd) return;
    m_nid.uFlags |= NIF_INFO;
    wcscpy_s(m_nid.szInfoTitle, title.c_str());
    wcscpy_s(m_nid.szInfo, message.c_str());
    m_nid.dwInfoFlags = NIIF_INFO;
    Shell_NotifyIconW(NIM_MODIFY, &m_nid);
    m_nid.uFlags &= ~NIF_INFO; // 플래그 복구
}

void TrayApp::OpenWebDashboard() {
    std::wstring url = L"http://localhost:" + std::to_wstring(m_port) + L"/";
    ShellExecuteW(nullptr, L"open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

void TrayApp::HandleTrayMenu() {
    POINT pt;
    GetCursorPos(&pt);

    HMENU hMenu = CreatePopupMenu();
    InsertMenuW(hMenu, 0, MF_BYPOSITION | MF_STRING, ID_TRAY_OPEN_WEB, L"🌐 웹 대시보드 열기 (http://localhost:8080)");
    InsertMenuW(hMenu, 1, MF_BYPOSITION | MF_SEPARATOR, 0, nullptr);
    InsertMenuW(hMenu, 2, MF_BYPOSITION | MF_STRING, ID_TRAY_EXIT, L"❌ 서버 종료 (Exit)");

    SetForegroundWindow(m_hwnd);
    UINT clicked = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_NONOTIFY, pt.x, pt.y, 0, m_hwnd, nullptr);
    DestroyMenu(hMenu);

    if (clicked == ID_TRAY_OPEN_WEB) {
        OpenWebDashboard();
    } else if (clicked == ID_TRAY_EXIT) {
        if (m_onExitCallback) {
            m_onExitCallback();
        }
        Stop();
    }
}

LRESULT CALLBACK TrayApp::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_TRAYICON) {
        if (lParam == WM_RBUTTONUP) {
            if (s_instance) s_instance->HandleTrayMenu();
        } else if (lParam == WM_LBUTTONDBLCLK) {
            if (s_instance) s_instance->OpenWebDashboard();
        }
        return 0;
    }

    if (msg == WM_DESTROY) {
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}
