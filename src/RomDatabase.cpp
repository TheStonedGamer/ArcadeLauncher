#include "pch.h"
#include "RomDatabase.h"

// ── Normalisation (shared with IgdbSync) ─────────────────────────────────────

static std::wstring NormaliseKey(const std::wstring& s) {
    std::wstring out;
    out.reserve(s.size());
    for (wchar_t c : s) {
        if      (iswalpha(c) || iswdigit(c)) out += towlower(c);
        else if (iswspace(c) || c == L'-') {
            if (!out.empty() && out.back() != L' ') out += L' ';
        }
    }
    while (!out.empty() && out.back() == L' ') out.pop_back();
    return out;
}

// ── Minimal JSON helpers ──────────────────────────────────────────────────────

static std::string ReadStr(const std::string& json, const std::string& key) {
    std::string srch = "\"" + key + "\":\"";
    size_t p = json.find(srch);
    if (p == std::string::npos) return {};
    p += srch.size();
    std::string v;
    while (p < json.size() && json[p] != '"') {
        if (json[p] == '\\' && p + 1 < json.size()) { ++p; }
        v += json[p++];
    }
    return v;
}

static int64_t ReadNum(const std::string& json, const std::string& key) {
    std::string srch = "\"" + key + "\":";
    size_t p = json.find(srch);
    if (p == std::string::npos) return 0;
    p += srch.size();
    while (p < json.size() && json[p] == ' ') ++p;
    bool neg = (p < json.size() && json[p] == '-');
    if (neg) ++p;
    int64_t v = 0;
    while (p < json.size() && isdigit((unsigned char)json[p]))
        v = v * 10 + (json[p++] - '0');
    return neg ? -v : v;
}

// ── RomDatabase::Load ─────────────────────────────────────────────────────────

bool RomDatabase::Load(const std::wstring& jsonPath) {
    std::ifstream f(jsonPath);
    if (!f) return false;
    std::string raw((std::istreambuf_iterator<char>(f)),
                     std::istreambuf_iterator<char>());

    m_db.clear();

    // Map platform name string → Platform enum
    static const struct { const char* name; Platform p; } kPlatforms[] = {
        { "NES",    Platform::NES    },
        { "SNES",   Platform::SNES   },
        { "N64",    Platform::N64    },
        { "PS1",    Platform::PS1    },
        { "PS2",    Platform::PS2    },
        { "Xbox",   Platform::Xbox   },
        { "Xbox360",Platform::Xbox360},
        { "GBC",    Platform::Repacks}, // unused for now
        { nullptr,  Platform::Repacks },
    };

    for (auto* pm = kPlatforms; pm->name; ++pm) {
        // Find the platform section: "NES":{...}
        std::string platKey = std::string("\"") + pm->name + "\":{";
        size_t sec = raw.find(platKey);
        if (sec == std::string::npos) continue;
        sec += platKey.size();

        // Walk the section extracting "key":{"t":"...","i":...} pairs
        auto& platMap = m_db[(int)pm->p];

        // Find the matching closing brace for this section
        int depth = 1;
        size_t pos = sec;
        while (pos < raw.size() && depth > 0) {
            // Skip to next key: "
            size_t ks = raw.find('"', pos);
            if (ks == std::string::npos) break;

            // Read the ROM key string
            ++ks;
            std::string romKey;
            while (ks < raw.size() && raw[ks] != '"') {
                if (raw[ks] == '\\' && ks + 1 < raw.size()) { ++ks; }
                romKey += raw[ks++];
            }
            if (ks >= raw.size()) break;
            ++ks; // skip closing "

            // Skip the colon
            while (ks < raw.size() && raw[ks] != '{' && raw[ks] != '}') ++ks;
            if (ks >= raw.size() || raw[ks] == '}') break;

            // Find the entry object {..."t":...,"i":...}
            size_t objStart = ks;
            size_t objEnd   = raw.find('}', objStart);
            if (objEnd == std::string::npos) break;
            std::string obj = raw.substr(objStart, objEnd - objStart + 1);

            Entry e;
            std::string t = ReadStr(obj, "t");
            if (!t.empty()) {
                e.title  = ToWide(t);
                e.igdbId = ReadNum(obj, "i");

                // Store with normalised key so hand-built entries also match.
                platMap[NormaliseKey(ToWide(romKey))] = std::move(e);
            }

            pos = objEnd + 1;
            // Adjust nesting depth
            for (char c : raw.substr(objStart, objEnd - objStart + 1))
                if (c == '{') ++depth; else if (c == '}') --depth;
            if (depth <= 1) { depth = 1; }  // reset: we're still in the platform block
        }
    }

    return !m_db.empty();
}

// ── RomDatabase::Lookup ───────────────────────────────────────────────────────

const RomDatabase::Entry* RomDatabase::Lookup(Platform platform,
                                               const std::wstring& strippedTitle) const {
    auto pit = m_db.find((int)platform);
    if (pit == m_db.end()) return nullptr;

    // Normalise the incoming title the same way IgdbSync stores keys.
    std::wstring key = NormaliseKey(strippedTitle);

    auto it = pit->second.find(key);
    return (it != pit->second.end()) ? &it->second : nullptr;
}

int RomDatabase::EntryCount() const {
    int n = 0;
    for (auto& [p, m] : m_db) n += (int)m.size();
    return n;
}

// ── RomDatabase::Download ─────────────────────────────────────────────────────

bool RomDatabase::Download(const std::wstring& destPath) {
    HINTERNET hSess = WinHttpOpen(L"ArcadeLauncher/RomDB",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSess) return false;
    WinHttpSetTimeouts(hSess, 10000, 10000, 10000, 30000);

    URL_COMPONENTSW uc{};
    uc.dwStructSize = sizeof(uc);
    wchar_t host[512]{}, path[2048]{};
    uc.lpszHostName = host; uc.dwHostNameLength = 512;
    uc.lpszUrlPath  = path; uc.dwUrlPathLength  = 2048;

    if (!WinHttpCrackUrl(kDbUrl, 0, 0, &uc)) {
        WinHttpCloseHandle(hSess); return false;
    }

    HINTERNET hConn = WinHttpConnect(hSess, host, uc.nPort, 0);
    if (!hConn) { WinHttpCloseHandle(hSess); return false; }

    DWORD flags = (uc.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hReq = WinHttpOpenRequest(hConn, L"GET", path, nullptr,
        WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);

    bool ok = false;
    if (hReq) {
        DWORD redir = WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS;
        WinHttpSetOption(hReq, WINHTTP_OPTION_REDIRECT_POLICY, &redir, sizeof(redir));

        if (WinHttpSendRequest(hReq, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                               WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
            WinHttpReceiveResponse(hReq, nullptr)) {
            DWORD status = 0, sz = sizeof(status);
            WinHttpQueryHeaders(hReq, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                                nullptr, &status, &sz, nullptr);
            if (status == 200) {
                std::vector<char> data;
                DWORD read = 0; char buf[8192];
                while (WinHttpReadData(hReq, buf, sizeof(buf), &read) && read > 0)
                    data.insert(data.end(), buf, buf + read);
                if (!data.empty()) {
                    HANDLE hf = CreateFileW(destPath.c_str(), GENERIC_WRITE, 0, nullptr,
                        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
                    if (hf != INVALID_HANDLE_VALUE) {
                        DWORD written;
                        WriteFile(hf, data.data(), (DWORD)data.size(), &written, nullptr);
                        CloseHandle(hf);
                        ok = (written == data.size());
                    }
                }
            }
        }
        WinHttpCloseHandle(hReq);
    }
    WinHttpCloseHandle(hConn);
    WinHttpCloseHandle(hSess);
    return ok;
}
