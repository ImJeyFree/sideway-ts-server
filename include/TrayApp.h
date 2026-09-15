#pragma once

#include <windows.h>
#include <shellapi.h>
#include <string>
#include <functional>

class TrayApp {
public:
    TrayApp(HINSTANCE hInstance, int port, std::function<void()> onExitCallback);
    ~TrayApp();

    bool Initialize();
    void RunMessageLoop();
    void Stop();

    void ShowNotification(const std::wstring& title, const std::wstring& message);

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    void HandleTrayMenu();
    void OpenWebDashboard();

    HINSTANCE m_hInstance;
    int m_port;
    std::function<void()> m_onExitCallback;

    HWND m_hwnd = nullptr;
    NOTIFYICONDATAW m_nid{};
    bool m_running = false;

    static TrayApp* s_instance;
};
