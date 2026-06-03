#pragma once
#include "Scanner.h"

struct EmulatorRomConfig {
    std::wstring emulatorPath;
    std::wstring emulatorArgs;   // {rom} is replaced with ROM path
    std::vector<std::wstring> romDirs;
    std::vector<std::wstring> extensions;
    Platform platform;
};

class EmulatorScanner : public IScanner {
public:
    explicit EmulatorScanner(EmulatorRomConfig cfg);
    std::vector<Game> Scan() override;
    Platform GetPlatform() const override { return m_cfg.platform; }

private:
    EmulatorRomConfig m_cfg;
};
