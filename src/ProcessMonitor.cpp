#include "pch.h"
#include "ProcessMonitor.h"

ProcessMonitor::~ProcessMonitor() {
    KillCurrent();
}

bool ProcessMonitor::Launch(const std::wstring& exe, const std::wstring& args,
                             const std::wstring& workDir, DoneCallback cb) {
    if (m_running.load()) return false;

    std::wstring cmdLine = L"\"" + exe + L"\"";
    if (!args.empty()) cmdLine += L" " + args;

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};

    std::wstring wd = workDir.empty() ?
        exe.substr(0, exe.rfind(L'\\')) : workDir;

    if (!CreateProcessW(nullptr, cmdLine.data(), nullptr, nullptr,
                        FALSE, 0, nullptr,
                        wd.empty() ? nullptr : wd.c_str(),
                        &si, &pi)) {
        return false;
    }
    CloseHandle(pi.hThread);
    m_hProcess = pi.hProcess;
    m_running.store(true);
    m_watchThread = std::thread(&ProcessMonitor::WatchThread, this, pi.hProcess, std::move(cb));
    return true;
}

bool ProcessMonitor::LaunchUri(const std::wstring& uri, const std::wstring& exeHint,
                                int timeoutSec, DoneCallback cb) {
    // ShellExecute the URI, then poll for a process matching exeHint
    HINSTANCE r = ShellExecuteW(nullptr, L"open", uri.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    if ((INT_PTR)r <= 32) return false;

    if (exeHint.empty() || cb == nullptr) return true;

    // Start a background thread that polls for the launched process
    m_running.store(true);
    m_watchThread = std::thread([this, exeHint, timeoutSec, cb = std::move(cb)]() {
        auto deadline = std::chrono::steady_clock::now() +
                        std::chrono::seconds(timeoutSec);
        HANDLE hFound = INVALID_HANDLE_VALUE;

        while (std::chrono::steady_clock::now() < deadline) {
            // Enumerate processes looking for exeHint
            HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
            if (hSnap != INVALID_HANDLE_VALUE) {
                PROCESSENTRY32W pe{ sizeof(pe) };
                if (Process32FirstW(hSnap, &pe)) {
                    do {
                        std::wstring exeName = pe.szExeFile;
                        std::wstring hint = exeHint;
                        for (auto& c : exeName) c = towlower(c);
                        for (auto& c : hint)    c = towlower(c);
                        if (exeName.find(hint) != std::wstring::npos) {
                            hFound = OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION,
                                                 FALSE, pe.th32ProcessID);
                            break;
                        }
                    } while (Process32NextW(hSnap, &pe));
                }
                CloseHandle(hSnap);
            }
            if (hFound != INVALID_HANDLE_VALUE) break;
            Sleep(1000);
        }

        if (hFound != INVALID_HANDLE_VALUE) {
            m_hProcess = hFound;
            WatchThread(hFound, cb);
        } else {
            m_running.store(false);
        }
    });
    return true;
}

void ProcessMonitor::WatchThread(HANDLE hProcess, DoneCallback cb) {
    auto start = std::chrono::steady_clock::now();
    WaitForSingleObject(hProcess, INFINITE);
    auto end = std::chrono::steady_clock::now();
    uint64_t elapsed = (uint64_t)std::chrono::duration_cast<std::chrono::seconds>(end - start).count();
    CloseHandle(hProcess);
    m_hProcess = INVALID_HANDLE_VALUE;
    m_running.store(false);
    if (cb) cb(elapsed);
}

void ProcessMonitor::KillCurrent() {
    if (m_hProcess != INVALID_HANDLE_VALUE)
        TerminateProcess(m_hProcess, 0);
    if (m_watchThread.joinable())
        m_watchThread.join();
}
