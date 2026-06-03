#pragma once
#include "pch.h"
#include "GameLibrary.h"

// ── RomDatabase ───────────────────────────────────────────────────────────────
//
// Maps (Platform, stripped_rom_title) → canonical display title + IGDB ID.
//
// The stripped title is what EmulatorScanner::StripRomTags produces after
// removing region/revision/dump tags from the ROM filename — e.g.
//   "Super Mario Bros. (World)" → "Super Mario Bros."
//   "Contra (USA)"              → "Contra"
//
// Lookup is case-insensitive (keys are stored lowercase).
//
// Database format (romdb.json):
//   {
//     "v": 1,
//     "NES": {
//       "contra":          {"t": "Contra",          "i": 1695},
//       "super mario bros":{"t": "Super Mario Bros.","i": 1074}
//     },
//     "SNES": { ... },
//     ...
//   }
//
// "t" = canonical title to display in the launcher
// "i" = IGDB game ID (0 = unknown / not yet mapped)
//
// The database file is stored in %AppData%\ArcadeLauncher\romdb.json and can
// be updated independently of the launcher binary.
// ─────────────────────────────────────────────────────────────────────────────

class RomDatabase {
public:
    struct Entry {
        std::wstring title;      // canonical display title
        int64_t      igdbId = 0; // 0 = not mapped
    };

    // Load database from a JSON file. Returns false on failure.
    bool Load(const std::wstring& jsonPath);

    // Lookup a game by platform and stripped ROM title.
    // Returns nullptr if not found.
    const Entry* Lookup(Platform platform, const std::wstring& strippedTitle) const;

    bool IsLoaded() const { return !m_db.empty(); }
    int  EntryCount() const;

    // Download the latest romdb.json to destPath.
    // Returns true if downloaded and written successfully.
    static bool Download(const std::wstring& destPath);

    // URL for the hosted database file.
    static constexpr const wchar_t* kDbUrl =
        L"https://raw.githubusercontent.com/TheStonedGamer/ArcadeLauncher/main/data/romdb.json";

private:
    // (int)Platform → (lowercase_stripped_title → Entry)
    std::unordered_map<int, std::unordered_map<std::wstring, Entry>> m_db;
};
