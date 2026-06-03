#include "pch.h"
#include "EmulatorScanner.h"

// Strip all trailing ROM tags: (USA), [!], (Rev 2), (Beta), etc.
static std::wstring StripRomTags(std::wstring title) {
    for (;;) {
        while (!title.empty() && iswspace(title.back())) title.pop_back();
        if (title.empty()) break;
        wchar_t close = title.back();
        if (close != L')' && close != L']') break;
        wchar_t open = (close == L')') ? L'(' : L'[';
        size_t p = title.rfind(open);
        if (p == std::wstring::npos) break;
        title = title.substr(0, p);
    }
    while (!title.empty() && iswspace(title.back())) title.pop_back();
    return title;
}

EmulatorScanner::EmulatorScanner(EmulatorRomConfig cfg) : m_cfg(std::move(cfg)) {}

std::vector<Game> EmulatorScanner::Scan() {
    std::vector<Game> games;
    if (m_cfg.emulatorPath.empty()) return games;
    if (m_cfg.romDirs.empty())     return games;

    for (auto& dir : m_cfg.romDirs) {
        if (dir.empty()) continue;
        WIN32_FIND_DATAW fd;
        HANDLE h = FindFirstFileW((dir + L"\\*").c_str(), &fd);
        if (h == INVALID_HANDLE_VALUE) continue;

        do {
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
            std::wstring fname = fd.cFileName;
            // Extract extension
            size_t dot = fname.rfind(L'.');
            if (dot == std::wstring::npos) continue;
            std::wstring ext = fname.substr(dot + 1);
            for (auto& c : ext) c = towlower(c);

            bool match = false;
            for (auto& e : m_cfg.extensions) {
                std::wstring le = e;
                for (auto& c : le) c = towlower(c);
                if (ext == le) { match = true; break; }
            }
            if (!match) continue;

            // Title = filename without extension, ROM tags stripped
            std::wstring title = StripRomTags(fname.substr(0, dot));

            std::wstring romPath = dir + L"\\" + fname;

            // Build launch args: replace {rom} placeholder or append
            std::wstring args = m_cfg.emulatorArgs;
            size_t ph = args.find(L"{rom}");
            if (ph != std::wstring::npos)
                args.replace(ph, 5, L"\"" + romPath + L"\"");
            else
                args += L" \"" + romPath + L"\"";

            Game g;
            g.id           = PlatformName(m_cfg.platform) + L"_" + fname;
            g.title        = title;
            g.platform     = m_cfg.platform;
            g.emulatorPath = m_cfg.emulatorPath;
            g.romPath      = romPath;
            g.arguments    = args;
            games.push_back(std::move(g));

        } while (FindNextFileW(h, &fd));
        FindClose(h);
    }
    return games;
}
