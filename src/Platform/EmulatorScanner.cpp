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

// Score a ROM filename using No-Intro / GoodNES naming conventions.
// Higher = better. We pick the highest-scoring ROM when titles collide.
static int RomScore(const std::wstring& fname) {
    std::wstring f = fname;
    for (auto& c : f) c = towlower(c);

    int score = 0;

    // Verified good dump — best possible
    if (f.find(L"[!]") != std::wstring::npos)          score += 20;

    // Alternates, bad dumps, over-dumps — avoid
    if (f.find(L"[a")  != std::wstring::npos)           score -= 10; // [a1],[a2]…
    if (f.find(L"[b")  != std::wstring::npos)           score -= 15; // bad dump
    if (f.find(L"[o")  != std::wstring::npos)           score -= 10; // over-dump

    // Prototypes and betas — usually interesting but not the release
    if (f.find(L"prototype") != std::wstring::npos)     score -=  8;
    if (f.find(L"beta")      != std::wstring::npos)     score -=  8;

    // Translation patches — fine but not the original
    if (f.find(L"trad-")     != std::wstring::npos)     score -=  5;

    // Prefer higher PRG / Rev revisions
    if (f.find(L"prg 0")     != std::wstring::npos)     score -=  2;
    if (f.find(L"prg 1")     != std::wstring::npos)     score +=  1;
    if (f.find(L"rev 0")     != std::wstring::npos)     score -=  2;
    if (f.find(L"rev a")     != std::wstring::npos)     score +=  1;
    if (f.find(L"rev b")     != std::wstring::npos)     score +=  2;

    // Prefer US/English releases over region-ambiguous or foreign
    if (f.find(L"(u)")       != std::wstring::npos)     score +=  3;
    if (f.find(L"(usa)")     != std::wstring::npos)     score +=  3;
    if (f.find(L"en,")       != std::wstring::npos)     score +=  1; // multi-lang with English

    // Hacks — deprioritise
    if (f.find(L"hack")      != std::wstring::npos)     score -= 12;

    return score;
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

    // ── Deduplicate ROM variants ───────────────────────────────────────────────
    // Many ROM collections contain multiple revisions/alternates of the same game
    // (e.g. "Zelda (U) (PRG 0).nes" and "Zelda (U) (PRG 1).nes"). After tag
    // stripping they produce the same title. Keep the highest-scored ROM only.
    //
    // We use a map: title → index of current winner in `games`.
    // Losers get their id cleared; we erase them at the end.
    std::unordered_map<std::wstring, size_t> bestIdx; // title → winner index
    for (size_t i = 0; i < games.size(); ++i) {
        const std::wstring& t = games[i].title;
        auto it = bestIdx.find(t);
        if (it == bestIdx.end()) {
            bestIdx[t] = i;
        } else {
            size_t prev = it->second;
            int scoreNew  = RomScore(games[i].romPath);
            int scorePrev = RomScore(games[prev].romPath);
            if (scoreNew > scorePrev) {
                games[prev].id.clear(); // mark old winner as discarded
                bestIdx[t] = i;
            } else {
                games[i].id.clear();   // discard new challenger
            }
        }
    }
    games.erase(std::remove_if(games.begin(), games.end(),
        [](const Game& g) { return g.id.empty(); }), games.end());

    return games;
}
