#include "pch.h"
#include "IgdbSync.h"
#include "Config.h"   // for Escape()

// ── Normalisation ─────────────────────────────────────────────────────────────

std::wstring IgdbSync::Normalise(const std::wstring& title) {
    std::wstring out;
    out.reserve(title.size());
    for (wchar_t c : title) {
        if (iswalpha(c) || iswdigit(c)) {
            out += towlower(c);
        } else if (iswspace(c) || c == L'-') {
            if (!out.empty() && out.back() != L' ')
                out += L' ';
        }
        // all other punctuation (.,!?:'") is dropped
    }
    while (!out.empty() && out.back() == L' ')
        out.pop_back();
    return out;
}

// ── Platform map ──────────────────────────────────────────────────────────────

struct PlatformSync {
    const char* dbKey;   // key used in romdb.json (matches Platform enum name)
    int igdbId;          // IGDB platform ID
};

static const PlatformSync kPlatforms[] = {
    { "NES",    18 },
    { "SNES",   19 },
    { "N64",     4 },
    { "PS1",     7 },
    { "PS2",     8 },
    { "Xbox",   11 },
    { "Xbox360", 12 },
};
static constexpr int kNumPlatforms = (int)(sizeof(kPlatforms)/sizeof(kPlatforms[0]));

// ── JSON writer helpers ───────────────────────────────────────────────────────

static std::string EscapeJson(const std::wstring& s) {
    std::string u = ToUtf8(s);
    std::string o;
    o.reserve(u.size() + 4);
    for (char c : u) {
        if      (c == '"')  o += "\\\"";
        else if (c == '\\') o += "\\\\";
        else if (c == '\n') o += "\\n";
        else                o += c;
    }
    return o;
}

// ── Worker ────────────────────────────────────────────────────────────────────

void IgdbSync::Worker(HWND hwnd, IgdbClient* client, std::wstring destPath) {
    // Re-authenticate if needed (token may have expired between launches)
    if (!client->IsAuthenticated() && !client->HasCredentials()) {
        PostMessageW(hwnd, WM_IGDBSYNC_DONE, 0, 0);
        return;
    }

    std::string json = "{\n  \"v\":2,\"igdb\":true,\n";
    int totalGames = 0;

    for (int pi = 0; pi < kNumPlatforms; ++pi) {
        auto& plat = kPlatforms[pi];
        json += "  \"";
        json += plat.dbKey;
        json += "\":{\n";

        bool firstEntry = true;
        int offset = 0;
        const int pageSize = 500;

        while (true) {
            auto games = client->FetchGamesByPlatform(plat.igdbId, offset, pageSize);
            if (games.empty()) break;

            for (auto& g : games) {
                if (g.id == 0 || g.name.empty()) continue;

                std::wstring key = IgdbSync::Normalise(g.name);
                if (key.empty()) continue;

                if (!firstEntry) json += ",\n";
                firstEntry = false;

                json += "    \"";
                json += EscapeJson(key);
                json += "\":{\"t\":\"";
                json += EscapeJson(g.name);
                json += "\",\"i\":";
                json += std::to_string(g.id);
                json += "}";

                ++totalGames;
            }

            offset += (int)games.size();
            if ((int)games.size() < pageSize) break;

            // Respect IGDB rate limit: 4 requests/sec → wait 250ms between pages
            Sleep(260);
        }

        json += "\n  }";
        if (pi + 1 < kNumPlatforms) json += ",";
        json += "\n";
    }

    json += "}\n";

    // Write atomically via temp file
    std::wstring tmp = destPath + L".tmp";
    HANDLE hf = CreateFileW(tmp.c_str(), GENERIC_WRITE, 0, nullptr,
                            CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    bool ok = false;
    if (hf != INVALID_HANDLE_VALUE) {
        DWORD written = 0;
        WriteFile(hf, json.data(), (DWORD)json.size(), &written, nullptr);
        CloseHandle(hf);
        if (written == json.size()) {
            DeleteFileW(destPath.c_str());
            ok = MoveFileW(tmp.c_str(), destPath.c_str()) != FALSE;
        }
    }
    if (!ok) DeleteFileW(tmp.c_str());

    PostMessageW(hwnd, WM_IGDBSYNC_DONE, ok ? (WPARAM)totalGames : 0, 0);
}

// ── Public API ────────────────────────────────────────────────────────────────

void IgdbSync::StartAsync(HWND hwnd, IgdbClient& client,
                           const std::wstring& destPath) {
    std::thread(Worker, hwnd, &client, destPath).detach();
}
