#include "pch.h"
#include "Config.h"

static std::string ReadField(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\":\"";
    size_t p = json.find(search);
    if (p == std::string::npos) return {};
    p += search.size();
    std::string val;
    while (p < json.size() && json[p] != '"') {
        if (json[p] == '\\' && p + 1 < json.size()) {
            ++p;
            if (json[p] == 'n') val += '\n';
            else val += json[p];
        } else val += json[p];
        ++p;
    }
    return val;
}

static bool ReadBool(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\":";
    size_t p = json.find(search);
    if (p == std::string::npos) return false;
    p += search.size();
    while (p < json.size() && json[p] == ' ') ++p;
    return json.substr(p, 4) == "true";
}

static int ReadInt(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\":";
    size_t p = json.find(search);
    if (p == std::string::npos) return 0;
    p += search.size();
    while (p < json.size() && json[p] == ' ') ++p;
    int v = 0; bool neg = false;
    if (p < json.size() && json[p] == '-') { neg = true; ++p; }
    while (p < json.size() && isdigit((unsigned char)json[p]))
        v = v * 10 + (json[p++] - '0');
    return neg ? -v : v;
}

static int64_t ReadInt64(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\":";
    size_t p = json.find(search);
    if (p == std::string::npos) return 0;
    p += search.size();
    while (p < json.size() && json[p] == ' ') ++p;
    int64_t v = 0; bool neg = false;
    if (p < json.size() && json[p] == '-') { neg = true; ++p; }
    while (p < json.size() && isdigit((unsigned char)json[p]))
        v = v * 10 + (json[p++] - '0');
    return neg ? -v : v;
}

static std::vector<std::wstring> ReadStringArray(const std::string& json,
                                                  const std::string& key) {
    std::string search = "\"" + key + "\":[";
    size_t p = json.find(search);
    if (p == std::string::npos) return {};
    p += search.size();
    std::vector<std::wstring> result;
    while (p < json.size() && json[p] != ']') {
        while (p < json.size() && json[p] != '"' && json[p] != ']') ++p;
        if (json[p] != '"') break;
        ++p;
        std::string val;
        while (p < json.size() && json[p] != '"') {
            if (json[p] == '\\' && p + 1 < json.size()) { ++p; }
            val += json[p++];
        }
        if (p < json.size()) ++p;
        result.push_back(ToWide(val));
    }
    return result;
}

std::string Config::Escape(const std::wstring& s) {
    std::string u = ToUtf8(s);
    std::string o;
    for (char c : u) {
        if (c == '"')  o += "\\\"";
        else if (c == '\\') o += "\\\\";
        else if (c == '\n') o += "\\n";
        else o += c;
    }
    return o;
}

void Config::Save(const std::wstring& path) const {
    auto& e  = m_cfg.emulators;
    auto& lb = m_cfg.libraries;

    auto B = [](bool v) { return std::string(v ? "true" : "false"); };
    auto writeArr = [](const std::vector<std::wstring>& v, const std::string& name) {
        std::string s = "  \"" + name + "\":[";
        for (size_t i = 0; i < v.size(); ++i) {
            s += "\"" + Config::Escape(v[i]) + "\"";
            if (i + 1 < v.size()) s += ",";
        }
        return s + "]";
    };

    std::string out = "{\n";
    out += "  \"firstLaunchDone\":" + B(m_cfg.firstLaunchDone) + ",\n";
    out += "  \"startFullscreen\":" + B(m_cfg.startFullscreen) + ",\n";
    out += "  \"minimizeOnLaunch\":" + B(m_cfg.minimizeOnLaunch) + ",\n";
    out += "  \"windowWidth\":"  + std::to_string(m_cfg.windowWidth)  + ",\n";
    out += "  \"windowHeight\":" + std::to_string(m_cfg.windowHeight) + ",\n";
    out += "  \"steamGridDbApiKey\":\"" + Escape(m_cfg.steamGridDbApiKey) + "\",\n";
    out += "  \"igdbClientId\":\"" + Escape(m_cfg.igdbClientId) + "\",\n";
    out += "  \"igdbClientSecret\":\"" + Escape(m_cfg.igdbClientSecret) + "\",\n";
    out += "  \"igdbAccessToken\":\"" + Escape(m_cfg.igdbAccessToken) + "\",\n";
    out += "  \"igdbTokenExpiry\":" + std::to_string(m_cfg.igdbTokenExpiry) + ",\n";
    // Library settings
    out += "  \"steamEnabled\":"  + B(lb.steamEnabled)  + ",\n";
    out += "  \"epicEnabled\":"   + B(lb.epicEnabled)   + ",\n";
    out += "  \"gogEnabled\":"    + B(lb.gogEnabled)    + ",\n";
    out += "  \"steamPath\":\"" + Escape(lb.steamPath) + "\",\n";
    out += writeArr(lb.steamExtraFolders, "steamExtraFolders") + ",\n";
    out += writeArr(lb.epicManifestDirs, "epicManifestDirs") + ",\n";
    out += "  \"customLibCount\":" + std::to_string(lb.customLibraries.size()) + ",\n";
    for (size_t i = 0; i < lb.customLibraries.size(); ++i) {
        auto& cl = lb.customLibraries[i];
        std::string idx = std::to_string(i);
        out += "  \"customLib" + idx + "Name\":\"" + Escape(cl.name)    + "\",\n";
        out += "  \"customLib" + idx + "Enabled\":" + B(cl.enabled)     + ",\n";
        out += writeArr(cl.dirs, "customLib" + idx + "Dirs")             + ",\n";
    }
    // Emulator settings
    out += "  \"dolphinPath\":\"" + Escape(e.dolphinPath) + "\",\n";
    out += "  \"ryujinxPath\":\"" + Escape(e.ryujinxPath) + "\",\n";
    out += "  \"rpcs3Path\":\""   + Escape(e.rpcs3Path)   + "\",\n";
    out += "  \"n64Path\":\""     + Escape(e.n64Path)     + "\",\n";
    out += "  \"nesPath\":\""     + Escape(e.nesPath)     + "\",\n";
    out += "  \"snesPath\":\""    + Escape(e.snesPath)    + "\",\n";
    out += "  \"dolphinArgs\":\"" + Escape(e.dolphinArgs) + "\",\n";
    out += "  \"ryujinxArgs\":\"" + Escape(e.ryujinxArgs) + "\",\n";
    out += "  \"rpcs3Args\":\""   + Escape(e.rpcs3Args)   + "\",\n";
    out += "  \"n64Args\":\""     + Escape(e.n64Args)     + "\",\n";
    out += "  \"nesArgs\":\""     + Escape(e.nesArgs)     + "\",\n";
    out += "  \"snesArgs\":\""    + Escape(e.snesArgs)    + "\",\n";
    out += writeArr(e.dolphinRomDirs, "dolphinRomDirs") + ",\n";
    out += writeArr(e.ryujinxRomDirs, "ryujinxRomDirs") + ",\n";
    out += writeArr(e.rpcs3RomDirs,   "rpcs3RomDirs")   + ",\n";
    out += writeArr(e.n64RomDirs,     "n64RomDirs")     + ",\n";
    out += writeArr(e.nesRomDirs,     "nesRomDirs")     + ",\n";
    out += writeArr(e.snesRomDirs,    "snesRomDirs")    + ",\n";
    out += "  \"dolphinTag\":\"" + Escape(e.dolphinTag) + "\",\n";
    out += "  \"ryujinxTag\":\"" + Escape(e.ryujinxTag) + "\",\n";
    out += "  \"rpcs3Tag\":\""   + Escape(e.rpcs3Tag)   + "\",\n";
    out += "  \"n64Tag\":\""     + Escape(e.n64Tag)     + "\",\n";
    out += "  \"nesTag\":\""     + Escape(e.nesTag)     + "\",\n";
    out += "  \"snesTag\":\""    + Escape(e.snesTag)    + "\"\n";
    out += "}\n";

    std::ofstream f(path);
    f << out;
}

void Config::Load(const std::wstring& path) {
    std::ifstream f(path);
    if (!f) return;
    std::string json((std::istreambuf_iterator<char>(f)),
                      std::istreambuf_iterator<char>());

    m_cfg.firstLaunchDone    = ReadBool(json, "firstLaunchDone");
    m_cfg.startFullscreen    = ReadBool(json, "startFullscreen");
    m_cfg.minimizeOnLaunch   = ReadBool(json, "minimizeOnLaunch");
    int w = ReadInt(json, "windowWidth");
    int h = ReadInt(json, "windowHeight");
    if (w > 0) m_cfg.windowWidth  = w;
    if (h > 0) m_cfg.windowHeight = h;

    m_cfg.steamGridDbApiKey  = ToWide(ReadField(json, "steamGridDbApiKey"));
    m_cfg.igdbClientId       = ToWide(ReadField(json, "igdbClientId"));
    m_cfg.igdbClientSecret   = ToWide(ReadField(json, "igdbClientSecret"));
    m_cfg.igdbAccessToken    = ToWide(ReadField(json, "igdbAccessToken"));
    m_cfg.igdbTokenExpiry    = ReadInt64(json, "igdbTokenExpiry");

    // Library settings — default true if key absent (existing installs keep behaviour)
    auto readBoolDefault = [&](const std::string& key, bool def) {
        std::string s = "\"" + key + "\":";
        size_t p = json.find(s);
        if (p == std::string::npos) return def;
        p += s.size();
        while (p < json.size() && json[p] == ' ') ++p;
        return json.substr(p, 4) == "true";
    };
    m_cfg.libraries.steamEnabled = readBoolDefault("steamEnabled", true);
    m_cfg.libraries.epicEnabled  = readBoolDefault("epicEnabled",  true);
    m_cfg.libraries.gogEnabled   = readBoolDefault("gogEnabled",   true);
    m_cfg.libraries.steamPath         = ToWide(ReadField(json, "steamPath"));
    m_cfg.libraries.steamExtraFolders = ReadStringArray(json, "steamExtraFolders");
    m_cfg.libraries.epicManifestDirs  = ReadStringArray(json, "epicManifestDirs");

    // Migrate old single epicManifestDir
    if (m_cfg.libraries.epicManifestDirs.empty()) {
        auto old = ToWide(ReadField(json, "epicManifestDir"));
        if (!old.empty()) m_cfg.libraries.epicManifestDirs.push_back(old);
    }

    // Load custom libraries
    int clCount = ReadInt(json, "customLibCount");
    for (int i = 0; i < clCount; ++i) {
        std::string idx = std::to_string(i);
        CustomLibraryConfig cl;
        cl.name    = ToWide(ReadField(json, "customLib" + idx + "Name"));
        cl.enabled = readBoolDefault("customLib" + idx + "Enabled", true);
        cl.dirs    = ReadStringArray(json, "customLib" + idx + "Dirs");
        if (cl.name.empty()) cl.name = L"Custom Library";
        m_cfg.libraries.customLibraries.push_back(std::move(cl));
    }

    // Migrate old repacksDirs / customGameDirs into a custom library entry
    if (clCount == 0) {
        std::vector<std::wstring> old = ReadStringArray(json, "repacksDirs");
        if (old.empty()) old = ReadStringArray(json, "customGameDirs");
        if (!old.empty()) {
            CustomLibraryConfig cl;
            cl.name = L"Repacks";
            cl.enabled = readBoolDefault("repacksEnabled", true);
            cl.dirs = std::move(old);
            m_cfg.libraries.customLibraries.push_back(std::move(cl));
        }
    }

    m_cfg.emulators.dolphinPath    = ToWide(ReadField(json, "dolphinPath"));
    m_cfg.emulators.ryujinxPath    = ToWide(ReadField(json, "ryujinxPath"));
    m_cfg.emulators.rpcs3Path      = ToWide(ReadField(json, "rpcs3Path"));
    m_cfg.emulators.n64Path        = ToWide(ReadField(json, "n64Path"));
    m_cfg.emulators.nesPath        = ToWide(ReadField(json, "nesPath"));
    m_cfg.emulators.snesPath       = ToWide(ReadField(json, "snesPath"));
    m_cfg.emulators.dolphinArgs    = ToWide(ReadField(json, "dolphinArgs"));
    m_cfg.emulators.ryujinxArgs    = ToWide(ReadField(json, "ryujinxArgs"));
    m_cfg.emulators.rpcs3Args      = ToWide(ReadField(json, "rpcs3Args"));
    m_cfg.emulators.n64Args        = ToWide(ReadField(json, "n64Args"));
    m_cfg.emulators.nesArgs        = ToWide(ReadField(json, "nesArgs"));
    m_cfg.emulators.snesArgs       = ToWide(ReadField(json, "snesArgs"));
    m_cfg.emulators.dolphinRomDirs = ReadStringArray(json, "dolphinRomDirs");
    m_cfg.emulators.ryujinxRomDirs = ReadStringArray(json, "ryujinxRomDirs");
    m_cfg.emulators.rpcs3RomDirs   = ReadStringArray(json, "rpcs3RomDirs");
    m_cfg.emulators.n64RomDirs     = ReadStringArray(json, "n64RomDirs");
    m_cfg.emulators.nesRomDirs     = ReadStringArray(json, "nesRomDirs");
    m_cfg.emulators.snesRomDirs    = ReadStringArray(json, "snesRomDirs");
    m_cfg.emulators.dolphinTag     = ToWide(ReadField(json, "dolphinTag"));
    m_cfg.emulators.ryujinxTag     = ToWide(ReadField(json, "ryujinxTag"));
    m_cfg.emulators.rpcs3Tag       = ToWide(ReadField(json, "rpcs3Tag"));
    m_cfg.emulators.n64Tag         = ToWide(ReadField(json, "n64Tag"));
    m_cfg.emulators.nesTag         = ToWide(ReadField(json, "nesTag"));
    m_cfg.emulators.snesTag        = ToWide(ReadField(json, "snesTag"));
}
