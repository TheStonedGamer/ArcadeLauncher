#include "pch.h"
#include "GameLibrary.h"

void GameLibrary::AddGame(Game game) {
    std::lock_guard<std::mutex> lk(m_mutex);
    for (auto& g : m_games)
        if (g.id == game.id) { g = std::move(game); return; }
    m_games.push_back(std::move(game));
}

void GameLibrary::RemoveGame(const std::wstring& id) {
    std::lock_guard<std::mutex> lk(m_mutex);
    m_games.erase(std::remove_if(m_games.begin(), m_games.end(),
        [&](const Game& g){ return g.id == id; }), m_games.end());
}

void GameLibrary::UpdateGame(const Game& game) {
    std::lock_guard<std::mutex> lk(m_mutex);
    for (auto& g : m_games)
        if (g.id == game.id) { g = game; return; }
}

void GameLibrary::MergeGames(std::vector<Game> scanned) {
    std::lock_guard<std::mutex> lk(m_mutex);
    for (auto& s : scanned) {
        bool found = false;
        for (auto& g : m_games) {
            if (g.id == s.id) {
                // Preserve playtime, stats, and IGDB metadata on rescan
                s.playtimeSeconds = g.playtimeSeconds;
                s.lastPlayed      = g.lastPlayed;
                s.coverArtPath    = g.coverArtPath;
                s.igdbId          = g.igdbId;
                s.igdbMatched     = g.igdbMatched;
                s.summary         = g.summary;
                s.genres          = g.genres;
                s.igdbRating      = g.igdbRating;
                s.releaseDate     = g.releaseDate;
                s.igdbPlatformId  = g.igdbPlatformId;
                g = std::move(s);
                found = true;
                break;
            }
        }
        if (!found) m_games.push_back(std::move(s));
    }
}

std::vector<const Game*> GameLibrary::Filter(Platform p) const {
    std::lock_guard<std::mutex> lk(m_mutex);
    std::vector<const Game*> out;
    for (auto& g : m_games)
        if (g.platform == p) out.push_back(&g);
    return out;
}

std::vector<const Game*> GameLibrary::Search(const std::wstring& query) const {
    std::lock_guard<std::mutex> lk(m_mutex);
    std::wstring lq = query;
    for (auto& c : lq) c = towlower(c);

    std::vector<const Game*> out;
    for (auto& g : m_games) {
        std::wstring lt = g.title;
        for (auto& c : lt) c = towlower(c);
        if (lt.find(lq) != std::wstring::npos)
            out.push_back(&g);
    }
    return out;
}

Game* GameLibrary::FindById(const std::wstring& id) {
    for (auto& g : m_games)
        if (g.id == id) return &g;
    return nullptr;
}

// ── Persistence (hand-rolled JSON) ───────────────────────────────────────────

static std::wstring JEsc(const std::wstring& s) {
    std::wstring o;
    for (wchar_t c : s) {
        if (c == L'"')  { o += L"\\\""; }
        else if (c == L'\\') { o += L"\\\\"; }
        else if (c == L'\n') { o += L"\\n"; }
        else o += c;
    }
    return o;
}

static std::wstring JField(const std::wstring& key, const std::wstring& val) {
    return L"\"" + key + L"\":\"" + JEsc(val) + L"\"";
}

static std::wstring JFieldN(const std::wstring& key, uint64_t val) {
    return L"\"" + key + L"\":" + std::to_wstring(val);
}

static std::wstring JFieldF(const std::wstring& key, float val) {
    // Write as integer (IGDB ratings are 0-100)
    return L"\"" + key + L"\":" + std::to_wstring((int)(val * 100)) + L"e-2";
}

static std::wstring JFieldB(const std::wstring& key, bool val) {
    return L"\"" + key + L"\":" + (val ? L"true" : L"false");
}

static std::string ReadJsonField(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\":\"";
    size_t p = json.find(search);
    if (p == std::string::npos) return {};
    p += search.size();
    std::string val;
    while (p < json.size() && json[p] != '"') {
        if (json[p] == '\\' && p + 1 < json.size()) {
            ++p;
            if (json[p] == 'n') val += '\n';
            else if (json[p] == '\\') val += '\\';
            else if (json[p] == '"') val += '"';
            else val += json[p];
        } else val += json[p];
        ++p;
    }
    return val;
}

static uint64_t ReadJsonNum(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\":";
    size_t p = json.find(search);
    if (p == std::string::npos) return 0;
    p += search.size();
    while (p < json.size() && json[p] == ' ') ++p;
    uint64_t v = 0;
    while (p < json.size() && isdigit((unsigned char)json[p]))
        v = v * 10 + (json[p++] - '0');
    return v;
}

void GameLibrary::Save(const std::wstring& path) const {
    std::lock_guard<std::mutex> lk(m_mutex);
    std::wstring out = L"[\n";
    for (size_t i = 0; i < m_games.size(); ++i) {
        auto& g = m_games[i];
        out += L"  {";
        out += JField(L"id", g.id) + L",";
        out += JField(L"title", g.title) + L",";
        out += JField(L"platform", PlatformName(g.platform)) + L",";
        out += JField(L"launchUri", g.launchUri) + L",";
        out += JField(L"exePath", g.exePath) + L",";
        out += JField(L"arguments", g.arguments) + L",";
        out += JField(L"emulatorPath", g.emulatorPath) + L",";
        out += JField(L"romPath", g.romPath) + L",";
        out += JField(L"coverArtPath", g.coverArtPath) + L",";
        out += JField(L"coverArtUrl", g.coverArtUrl) + L",";
        out += JField(L"steamAppId", g.steamAppId) + L",";
        out += JField(L"epicAppName", g.epicAppName) + L",";
        out += JField(L"gogGameId", g.gogGameId) + L",";
        out += JFieldN(L"playtimeSeconds", g.playtimeSeconds) + L",";
        out += JFieldN(L"lastPlayed", (uint64_t)g.lastPlayed) + L",";
        out += JFieldN(L"igdbId", (uint64_t)g.igdbId) + L",";
        out += JFieldB(L"igdbMatched", g.igdbMatched) + L",";
        out += JField(L"summary", g.summary) + L",";
        out += JField(L"genres", g.genres) + L",";
        out += JFieldF(L"igdbRating", g.igdbRating) + L",";
        out += JFieldN(L"releaseDate", (uint64_t)g.releaseDate) + L",";
        out += JFieldN(L"igdbPlatformId", (uint64_t)g.igdbPlatformId);
        out += L"}";
        if (i + 1 < m_games.size()) out += L",";
        out += L"\n";
    }
    out += L"]\n";

    // Write as UTF-8
    std::string utf8 = ToUtf8(out);
    std::ofstream f(path, std::ios::binary);
    f.write(utf8.data(), utf8.size());
}

void GameLibrary::Load(const std::wstring& path) {
    std::ifstream f(path);
    if (!f) return;
    std::string raw((std::istreambuf_iterator<char>(f)),
                     std::istreambuf_iterator<char>());

    std::lock_guard<std::mutex> lk(m_mutex);
    m_games.clear();

    // Split on },{
    size_t pos = 0;
    while (true) {
        size_t start = raw.find('{', pos);
        if (start == std::string::npos) break;
        size_t end = raw.find('}', start);
        if (end == std::string::npos) break;
        std::string obj = raw.substr(start, end - start + 1);
        pos = end + 1;

        Game g;
        g.id           = ToWide(ReadJsonField(obj, "id"));
        if (g.id.empty()) continue;
        g.title        = ToWide(ReadJsonField(obj, "title"));
        g.launchUri    = ToWide(ReadJsonField(obj, "launchUri"));
        g.exePath      = ToWide(ReadJsonField(obj, "exePath"));
        g.arguments    = ToWide(ReadJsonField(obj, "arguments"));
        g.emulatorPath = ToWide(ReadJsonField(obj, "emulatorPath"));
        g.romPath      = ToWide(ReadJsonField(obj, "romPath"));
        g.coverArtPath = ToWide(ReadJsonField(obj, "coverArtPath"));
        g.coverArtUrl  = ToWide(ReadJsonField(obj, "coverArtUrl"));
        g.steamAppId   = ToWide(ReadJsonField(obj, "steamAppId"));
        g.epicAppName  = ToWide(ReadJsonField(obj, "epicAppName"));
        g.gogGameId    = ToWide(ReadJsonField(obj, "gogGameId"));
        g.playtimeSeconds = ReadJsonNum(obj, "playtimeSeconds");
        g.lastPlayed      = (int64_t)ReadJsonNum(obj, "lastPlayed");
        g.igdbId          = (int64_t)ReadJsonNum(obj, "igdbId");
        g.summary         = ToWide(ReadJsonField(obj, "summary"));
        g.genres          = ToWide(ReadJsonField(obj, "genres"));
        g.releaseDate     = (int64_t)ReadJsonNum(obj, "releaseDate");
        g.igdbPlatformId  = (int)ReadJsonNum(obj, "igdbPlatformId");

        // igdbMatched — look for boolean true/false
        {
            std::string search = "\"igdbMatched\":";
            size_t bp = obj.find(search);
            if (bp != std::string::npos) {
                bp += search.size();
                while (bp < obj.size() && obj[bp] == ' ') ++bp;
                g.igdbMatched = (obj.substr(bp, 4) == "true");
            }
        }

        // igdbRating stored as integer * 100 with e-2 suffix
        {
            std::string search = "\"igdbRating\":";
            size_t rp = obj.find(search);
            if (rp != std::string::npos) {
                rp += search.size();
                while (rp < obj.size() && obj[rp] == ' ') ++rp;
                int iv = 0;
                while (rp < obj.size() && isdigit((unsigned char)obj[rp]))
                    iv = iv * 10 + (obj[rp++] - '0');
                g.igdbRating = (float)iv / 100.0f;
            }
        }

        std::string plat = ReadJsonField(obj, "platform");
        if      (plat == "Steam")   g.platform = Platform::Steam;
        else if (plat == "Epic")    g.platform = Platform::Epic;
        else if (plat == "GOG")     g.platform = Platform::GOG;
        else if (plat == "Dolphin") g.platform = Platform::Dolphin;
        else if (plat == "Ryujinx") g.platform = Platform::Ryujinx;
        else                        g.platform = Platform::Repacks;

        m_games.push_back(std::move(g));
    }
}
