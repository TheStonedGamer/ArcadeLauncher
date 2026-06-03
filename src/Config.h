#pragma once
#include "pch.h"

struct CustomLibraryConfig {
    std::wstring name    = L"Custom Library";
    bool enabled         = true;
    std::vector<std::wstring> dirs;
};

struct LibraryConfig {
    bool steamEnabled  = true;
    bool epicEnabled   = true;
    bool gogEnabled    = true;

    std::wstring steamPath;                        // Steam install root override; empty = registry
    std::vector<std::wstring> steamExtraFolders;   // extra steamapps dirs beyond what VDF reports
    std::vector<std::wstring> epicManifestDirs;    // override list; empty = auto-detect

    std::vector<CustomLibraryConfig> customLibraries;
};

struct EmulatorConfig {
    bool dolphinEnabled  = true;
    bool ryujinxEnabled  = true;
    bool rpcs3Enabled    = true;
    bool n64Enabled      = true;
    bool nesEnabled      = true;
    bool snesEnabled     = true;

    std::wstring dolphinPath;
    std::wstring ryujinxPath;
    std::wstring rpcs3Path;
    std::wstring n64Path;
    std::wstring nesPath;
    std::wstring snesPath;

    std::wstring dolphinArgs;
    std::wstring ryujinxArgs;
    std::wstring rpcs3Args;
    std::wstring n64Args;
    std::wstring nesArgs;
    std::wstring snesArgs;

    std::vector<std::wstring> dolphinRomDirs;
    std::vector<std::wstring> ryujinxRomDirs;
    std::vector<std::wstring> rpcs3RomDirs;
    std::vector<std::wstring> n64RomDirs;
    std::vector<std::wstring> nesRomDirs;
    std::vector<std::wstring> snesRomDirs;

    // Last downloaded release tag (empty = never downloaded via launcher)
    std::wstring dolphinTag;
    std::wstring ryujinxTag;
    std::wstring rpcs3Tag;
    std::wstring n64Tag;
    std::wstring nesTag;
    std::wstring snesTag;
};

struct AppConfig {
    bool        firstLaunchDone  = false;
    bool        startFullscreen  = false;
    bool        minimizeOnLaunch = true;
    int         windowWidth  = 1280;
    int         windowHeight = 720;
    std::wstring steamGridDbApiKey;

    // IGDB / Twitch API credentials
    std::wstring igdbClientId;
    std::wstring igdbClientSecret;

    // Cached OAuth token
    std::wstring igdbAccessToken;
    int64_t      igdbTokenExpiry = 0;

    LibraryConfig  libraries;
    EmulatorConfig emulators;
};

class Config {
public:
    void Load(const std::wstring& path);
    void Save(const std::wstring& path) const;
    AppConfig& Get() { return m_cfg; }
    const AppConfig& Get() const { return m_cfg; }

private:
    AppConfig m_cfg;

    static std::string Escape(const std::wstring& s);
    static std::wstring Unescape(const std::string& s);
};
