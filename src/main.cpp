#include "pch.h"
#include "App.h"


int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int) {
    // Single-instance check
    HANDLE hMutex = CreateMutexW(nullptr, TRUE, L"ArcadeLauncherSingleInstance");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        HWND existing = FindWindowW(L"ArcadeLauncherWnd", nullptr);
        if (existing) {
            ShowWindow(existing, SW_RESTORE);
            SetForegroundWindow(existing);
        }
        CloseHandle(hMutex);
        return 0;
    }

    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

    {
        App app;
        if (app.Initialize(hInstance))
            app.Run();
    }

    CoUninitialize();
    CloseHandle(hMutex);
    return 0;
}
