#include "pch.h"
#include "Renderer.h"
#include "Version.h"

// ── Color palette ─────────────────────────────────────────────────────────────
static const D2D1_COLOR_F C_BG         = D2D1::ColorF(0x0D1117);
static const D2D1_COLOR_F C_SIDEBAR    = D2D1::ColorF(0x13181E);
static const D2D1_COLOR_F C_TOPBAR     = D2D1::ColorF(0x161B22);
static const D2D1_COLOR_F C_CARD       = D2D1::ColorF(0x21262D);
static const D2D1_COLOR_F C_CARD_HOV   = D2D1::ColorF(0x30363D);
static const D2D1_COLOR_F C_ACCENT     = D2D1::ColorF(0x58A6FF);
static const D2D1_COLOR_F C_TEXT       = D2D1::ColorF(0xC9D1D9);
static const D2D1_COLOR_F C_SUBTEXT    = D2D1::ColorF(0x8B949E);
static const D2D1_COLOR_F C_SELECTED   = D2D1::ColorF(0x388BFD);
static const D2D1_COLOR_F C_WHITE      = D2D1::ColorF(D2D1::ColorF::White);
static const D2D1_COLOR_F C_OVERLAY    = D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.65f);

#define HR(x) { HRESULT _hr = (x); assert(SUCCEEDED(_hr)); }

Renderer::~Renderer() {}

bool Renderer::Initialize(HWND hwnd) {
    m_hwnd = hwnd;

    // D2D factory
    HR(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, m_factory.GetAddressOf()));

    // WIC factory
    HR(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                        IID_PPV_ARGS(m_wic.GetAddressOf())));

    // DirectWrite factory
    HR(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,
                           __uuidof(IDWriteFactory),
                           reinterpret_cast<IUnknown**>(m_dwFactory.GetAddressOf())));

    RECT rc;
    GetClientRect(hwnd, &rc);
    m_width  = rc.right;
    m_height = rc.bottom;

    D2D1_RENDER_TARGET_PROPERTIES rtp = D2D1::RenderTargetProperties();
    D2D1_HWND_RENDER_TARGET_PROPERTIES hwndRtp =
        D2D1::HwndRenderTargetProperties(hwnd, D2D1::SizeU(m_width, m_height));
    HR(m_factory->CreateHwndRenderTarget(rtp, hwndRtp, m_rt.GetAddressOf()));
    m_rt->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);

    CreateBrushes();
    Resize(m_width, m_height);
    return true;
}

void Renderer::CreateBrushes() {
    auto mk = [&](const D2D1_COLOR_F& c, ComPtr<ID2D1SolidColorBrush>& b) {
        HR(m_rt->CreateSolidColorBrush(c, b.GetAddressOf()));
    };
    mk(C_BG,       m_brushBg);
    mk(C_SIDEBAR,  m_brushSidebar);
    mk(C_TOPBAR,   m_brushTopbar);
    mk(C_CARD,     m_brushCard);
    mk(C_CARD_HOV, m_brushCardHover);
    mk(C_ACCENT,   m_brushAccent);
    mk(C_TEXT,     m_brushText);
    mk(C_SUBTEXT,  m_brushSubtext);
    mk(C_WHITE,    m_brushWhite);
    mk(C_OVERLAY,  m_brushOverlay);
    mk(C_SELECTED, m_brushSelected);

    auto fmt = [&](const wchar_t* font, float size, DWRITE_FONT_WEIGHT weight,
                   ComPtr<IDWriteTextFormat>& tf) {
        HR(m_dwFactory->CreateTextFormat(font, nullptr, weight,
            DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
            size, L"en-us", tf.GetAddressOf()));
        tf->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
        tf->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        tf->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    };

    fmt(L"Segoe UI", 22.0f, DWRITE_FONT_WEIGHT_SEMI_BOLD, m_fmtTitle);
    fmt(L"Segoe UI", 13.0f, DWRITE_FONT_WEIGHT_NORMAL,    m_fmtSmall);
    fmt(L"Segoe UI", 11.0f, DWRITE_FONT_WEIGHT_SEMI_BOLD, m_fmtCard);
    fmt(L"Segoe UI",  9.5f, DWRITE_FONT_WEIGHT_NORMAL,    m_fmtCardSub);
    fmt(L"Segoe UI", 28.0f, DWRITE_FONT_WEIGHT_BOLD,      m_fmtHeading);
    fmt(L"Segoe UI", 14.0f, DWRITE_FONT_WEIGHT_NORMAL,    m_fmtSearch);
    fmt(L"Segoe UI", 13.0f, DWRITE_FONT_WEIGHT_NORMAL,    m_fmtSidebar);
    fmt(L"Segoe UI", 16.0f, DWRITE_FONT_WEIGHT_NORMAL,    m_fmtDetail);

    // Summary text: wrapping, smaller
    HR(m_dwFactory->CreateTextFormat(L"Segoe UI", nullptr,
        DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL, 12.0f, L"en-us",
        m_fmtSummary.GetAddressOf()));
    m_fmtSummary->SetWordWrapping(DWRITE_WORD_WRAPPING_WRAP);
    m_fmtSummary->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    m_fmtSummary->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);

    // MDL2 Assets icon font for toolbar buttons (gear, etc.)
    HR(m_dwFactory->CreateTextFormat(L"Segoe MDL2 Assets", nullptr,
        DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL, 18.0f, L"en-us",
        m_fmtIcon.GetAddressOf()));
    m_fmtIcon->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
    m_fmtIcon->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    m_fmtIcon->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
}

void Renderer::LoadPlatformIcons(PlatformIcons& icons, const EmulatorConfig& emuCfg) {
    icons.Load(emuCfg, m_rt.Get(), m_wic.Get());
    m_platformIcons = &icons;
}

void Renderer::Resize(UINT w, UINT h) {
    m_width = w; m_height = h;
    if (m_rt) m_rt->Resize(D2D1::SizeU(w, h));

    // Recompute grid columns
    float gridW = (float)w - m_sidebarW;
    m_cols = std::max(1, (int)((gridW + m_tileGap) / (m_tileW + m_tileGap)));

    m_searchRect = D2D1::RectF((float)w * 0.35f, 14.0f, (float)w * 0.65f, 50.0f);
    m_settingsBtnRect = D2D1::RectF((float)w - 50.0f, 16.0f, (float)w - 14.0f, 48.0f);
    m_launchBtnRect = {}; // set during detail panel draw
}

// ── Main render ───────────────────────────────────────────────────────────────

void Renderer::Render(const std::vector<const Game*>& games, RenderState& state) {
    if (!m_rt) return;
    m_lastGameCount = (int)games.size();
    m_rt->BeginDraw();

    DrawBackground();
    DrawTopBar(state);
    DrawSidebar(state);

    if (state.detailOpen && state.detailIndex >= 0 &&
        state.detailIndex < (int)games.size())
        DrawDetailPanel(games[state.detailIndex]);
    else
        DrawGrid(games, state);

    m_rt->EndDraw();
}

void Renderer::DrawBackground() {
    m_rt->Clear(C_BG);
}

void Renderer::DrawTopBar(const RenderState& state) {
    float w = (float)m_width;
    D2D1_RECT_F bar = D2D1::RectF(0, 0, w, m_topbarH);
    m_rt->FillRectangle(bar, m_brushTopbar.Get());

    // App name
    D2D1_RECT_F titleRect = D2D1::RectF(m_sidebarW + 8, 0, m_sidebarW + 200, m_topbarH);
    m_rt->DrawText(L"ArcadeLauncher", 14, m_fmtTitle.Get(), titleRect, m_brushAccent.Get());

    // Search box background
    bool searchFocused = (state.focusArea == FocusArea::Search);
    auto& sr = m_searchRect;
    m_rt->FillRoundedRectangle(D2D1::RoundedRect(sr, 6, 6), m_brushCard.Get());
    m_rt->DrawRoundedRectangle(D2D1::RoundedRect(sr, 6, 6),
        searchFocused ? m_brushAccent.Get() : m_brushSubtext.Get(),
        searchFocused ? 2.0f : 1.0f);

    // Search text / placeholder + cursor when focused
    D2D1_RECT_F textRect = D2D1::RectF(sr.left + 10, sr.top, sr.right - 10, sr.bottom);
    if (!state.searchQuery.empty()) {
        std::wstring display = state.searchQuery + (searchFocused ? L"│" : L"");
        m_rt->DrawText(display.c_str(), (UINT32)display.size(),
                       m_fmtSearch.Get(), textRect, m_brushText.Get());
    } else if (searchFocused) {
        m_rt->DrawText(L"│", 1, m_fmtSearch.Get(), textRect, m_brushSubtext.Get());
    } else {
        m_rt->DrawText(L"Search games...", 15, m_fmtSearch.Get(),
                       textRect, m_brushSubtext.Get());
    }

    // Ctrl+F hint when not focused
    if (!searchFocused) {
        D2D1_RECT_F hintRect = D2D1::RectF(sr.right - 52, sr.top, sr.right - 6, sr.bottom);
        m_rt->DrawText(L"Ctrl+F", 6, m_fmtSmall.Get(), hintRect, m_brushSubtext.Get());
    }

    // Metadata scan status pill — shown while background fetch is running
    if (state.metaScanning) {
        static const wchar_t kScanText[] = L"Fetching metadata…";
        D2D1_RECT_F pill = D2D1::RectF(
            m_settingsBtnRect.left - 166.0f, 18.0f,
            m_settingsBtnRect.left - 8.0f,  46.0f);
        m_rt->FillRoundedRectangle(D2D1::RoundedRect(pill, 6, 6), m_brushCard.Get());
        m_rt->DrawRoundedRectangle(D2D1::RoundedRect(pill, 6, 6),
                                   m_brushSubtext.Get(), 1.0f);
        D2D1_RECT_F pillText = D2D1::RectF(pill.left + 8, pill.top,
                                            pill.right - 8, pill.bottom);
        m_rt->DrawText(kScanText, (UINT32)wcslen(kScanText),
                       m_fmtSmall.Get(), pillText, m_brushSubtext.Get());
    }

    // Settings gear button (Segoe MDL2 Assets U+E713)
    auto& sb = m_settingsBtnRect;
    m_rt->FillRoundedRectangle(D2D1::RoundedRect(sb, 6, 6), m_brushCard.Get());
    m_rt->DrawText(L"", 1, m_fmtIcon.Get(),
                   D2D1::RectF(sb.left, sb.top, sb.right, sb.bottom),
                   m_brushSubtext.Get());
}

// static
std::vector<Renderer::SidebarEntry> Renderer::BuildSidebarEntries(const RenderState& s) {
    std::vector<SidebarEntry> v;
    v.push_back({ L"All Games", true,  Platform::Repacks });
    if (s.showSteam)   v.push_back({ L"Steam",   false, Platform::Steam   });
    if (s.showEpic)    v.push_back({ L"Epic",    false, Platform::Epic    });
    if (s.showGog)     v.push_back({ L"GOG",     false, Platform::GOG     });
    if (s.showDolphin) v.push_back({ L"Dolphin", false, Platform::Dolphin });
    if (s.showRyujinx) v.push_back({ L"Ryujinx", false, Platform::Ryujinx });
    if (s.showRPCS3)   v.push_back({ L"RPCS3",   false, Platform::RPCS3   });
    if (s.showN64)     v.push_back({ L"N64",     false, Platform::N64     });
    if (s.showNES)     v.push_back({ L"NES",      false, Platform::NES     });
    if (s.showSNES)    v.push_back({ L"SNES",     false, Platform::SNES    });
    if (s.showPS1)     v.push_back({ L"PS1",      false, Platform::PS1     });
    if (s.showPS2)     v.push_back({ L"PS2",      false, Platform::PS2     });
    if (s.showXbox360) v.push_back({ L"Xbox 360", false, Platform::Xbox360 });
    if (s.showRepacks) v.push_back({ L"Repacks",  false, Platform::Repacks });
    return v;
}

void Renderer::DrawSidebar(const RenderState& state) {
    D2D1_RECT_F sbar = D2D1::RectF(0, 0, m_sidebarW, (float)m_height);
    m_rt->FillRectangle(sbar, m_brushSidebar.Get());

    // Separator line on right
    m_rt->DrawLine(D2D1::Point2F(m_sidebarW, 0), D2D1::Point2F(m_sidebarW, (float)m_height),
                   m_brushCard.Get(), 1.0f);

    auto entries = BuildSidebarEntries(state);

    bool sidebarKbFocus = (state.focusArea == FocusArea::Sidebar);
    float y = m_topbarH + 12.0f;
    int entryIdx = 0;
    for (auto& e : entries) {
        bool active = e.all ? state.filterAll :
                      (!state.filterAll && state.filterPlatform == e.p);
        bool kbFocus = sidebarKbFocus && (entryIdx == state.sidebarFocusIdx);
        D2D1_RECT_F row = D2D1::RectF(0, y, m_sidebarW, y + 38.0f);

        if (active) {
            m_rt->FillRoundedRectangle(D2D1::RoundedRect(
                D2D1::RectF(6, y + 2, m_sidebarW - 6, y + 36), 6, 6),
                m_brushCardHover.Get());
            // Left accent bar
            m_rt->FillRectangle(D2D1::RectF(0, y + 6, 3, y + 32), m_brushAccent.Get());
        }

        // Keyboard focus ring (distinct from active selection)
        if (kbFocus && !active) {
            m_brushAccent->SetColor(D2D1::ColorF(C_ACCENT.r, C_ACCENT.g, C_ACCENT.b, 0.5f));
            m_rt->DrawRoundedRectangle(D2D1::RoundedRect(
                D2D1::RectF(6, y + 2, m_sidebarW - 6, y + 36), 6, 6),
                m_brushAccent.Get(), 1.5f);
            m_brushAccent->SetColor(C_ACCENT);
        } else if (kbFocus && active) {
            m_brushAccent->SetColor(D2D1::ColorF(C_ACCENT.r, C_ACCENT.g, C_ACCENT.b, 0.9f));
            m_rt->DrawRoundedRectangle(D2D1::RoundedRect(
                D2D1::RectF(6, y + 2, m_sidebarW - 6, y + 36), 6, 6),
                m_brushAccent.Get(), 2.0f);
            m_brushAccent->SetColor(C_ACCENT);
        }

        if (!e.all) {
            // Try to draw platform icon; fall back to colored dot
            ID2D1Bitmap* icon = m_platformIcons ? m_platformIcons->Get(e.p) : nullptr;
            if (icon) {
                float iconSz = 20.0f;
                float iconX = 8.0f, iconY = y + (38.0f - iconSz) / 2.0f;
                m_rt->DrawBitmap(icon, D2D1::RectF(iconX, iconY, iconX + iconSz, iconY + iconSz),
                                 active ? 1.0f : 0.65f,
                                 D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
            } else {
                D2D1_ELLIPSE dot = D2D1::Ellipse(D2D1::Point2F(20.0f, y + 19.0f), 5, 5);
                m_brushAccent->SetColor(PlatformColor(e.p));
                m_rt->FillEllipse(dot, m_brushAccent.Get());
                m_brushAccent->SetColor(C_ACCENT);
            }
        }

        D2D1_RECT_F lbl = D2D1::RectF(e.all ? 16.0f : 34.0f, y, m_sidebarW - 8, y + 38);
        m_rt->DrawText(e.label, (UINT32)wcslen(e.label), m_fmtSidebar.Get(), lbl,
                       (active || kbFocus) ? m_brushText.Get() : m_brushSubtext.Get());
        y += 42.0f;
        ++entryIdx;
    }

    // Version number
    {
        static const std::wstring ver = L"v" ARCADE_VERSION_WSTR;
        D2D1_RECT_F vr = D2D1::RectF(8, (float)m_height - 76, m_sidebarW - 8, (float)m_height - 58);
        m_rt->DrawText(ver.c_str(), (UINT32)ver.size(), m_fmtSmall.Get(), vr, m_brushSubtext.Get());
    }

    // Tab hint at bottom of sidebar when not in sidebar focus
    if (!sidebarKbFocus) {
        D2D1_RECT_F hint = D2D1::RectF(8, (float)m_height - 56, m_sidebarW - 8, (float)m_height - 38);
        m_rt->DrawText(L"Tab to focus", 12, m_fmtSmall.Get(), hint, m_brushSubtext.Get());
    }

    // Footer: game count (passed via RenderState is not available here; caller sets it via games.size())
    // We store it in a member set each Render() call — use the cached count
    std::wstring footer = std::to_wstring((int)m_lastGameCount) + L" games";
    D2D1_RECT_F fr = D2D1::RectF(8, (float)m_height - 36, m_sidebarW - 8, (float)m_height - 8);
    m_rt->DrawText(footer.c_str(), (UINT32)footer.size(), m_fmtSmall.Get(), fr,
                   m_brushSubtext.Get());
}

void Renderer::DrawGrid(const std::vector<const Game*>& games, RenderState& state) {
    if (games.empty()) {
        m_emptyStateBtnRect = {};   // reset; set below if we draw the button

        float cx = (m_sidebarW + m_width) / 2.0f;
        float cy = (m_topbarH + m_height) / 2.0f;

        // Dim message above the button
        static const wchar_t kMsg[] = L"No games in this library";
        D2D1_RECT_F msgR = D2D1::RectF(m_sidebarW + 40, cy - 58.0f,
                                        (float)m_width - 40, cy - 16.0f);
        m_rt->DrawText(kMsg, (UINT32)(sizeof(kMsg)/sizeof(wchar_t) - 1),
                       m_fmtDetail.Get(), msgR, m_brushSubtext.Get());

        // "Open Settings" button
        float bw = 196.0f, bh = 38.0f;
        m_emptyStateBtnRect = D2D1::RectF(cx - bw / 2, cy - 2.0f,
                                           cx + bw / 2, cy - 2.0f + bh);
        auto rr = D2D1::RoundedRect(m_emptyStateBtnRect, 8.0f, 8.0f);
        m_rt->FillRoundedRectangle(rr, m_brushCard.Get());
        m_rt->DrawRoundedRectangle(rr, m_brushAccent.Get(), 1.5f);

        static const wchar_t kBtn[] = L"Open Settings";
        m_rt->DrawText(kBtn, (UINT32)(sizeof(kBtn)/sizeof(wchar_t) - 1),
                       m_fmtSidebar.Get(), m_emptyStateBtnRect, m_brushAccent.Get());
        return;
    }
    m_emptyStateBtnRect = {};

    // Clipping to grid area
    D2D1_RECT_F clip = D2D1::RectF(m_sidebarW, m_topbarH,
                                    (float)m_width, (float)m_height);
    m_rt->PushAxisAlignedClip(clip, D2D1_ANTIALIAS_MODE_ALIASED);

    float startX = m_sidebarW + m_tileGap;
    float startY = m_topbarH + m_tileGap - state.scrollOffset;

    int rows = ((int)games.size() + m_cols - 1) / m_cols;
    for (int i = 0; i < (int)games.size(); ++i) {
        int col = i % m_cols;
        int row = i / m_cols;
        float x = startX + col * (m_tileW + m_tileGap);
        float y = startY + row * (m_tileH + m_tileGap + 22.0f); // +22 for title below

        // Skip if off-screen
        if (y + m_tileH + 22 < m_topbarH || y > (float)m_height) continue;

        D2D1_RECT_F rect = D2D1::RectF(x, y, x + m_tileW, y + m_tileH);
        DrawCard(*games[i], rect, state.hoveredIndex == i, state.selectedIndex == i);
    }

    m_rt->PopAxisAlignedClip();
}

void Renderer::DrawCard(const Game& game, D2D1_RECT_F rect,
                         bool hovered, bool selected) {
    float rnd = 8.0f;

    // Card shadow / glow
    if (hovered || selected) {
        D2D1_RECT_F shadow = D2D1::RectF(rect.left - 3, rect.top - 3,
                                          rect.right + 3, rect.bottom + 3);
        auto col = selected ? C_SELECTED : C_ACCENT;
        col.a = 0.4f;
        m_brushCard->SetColor(col);
        m_rt->FillRoundedRectangle(D2D1::RoundedRect(shadow, rnd + 2, rnd + 2),
                                   m_brushCard.Get());
        m_brushCard->SetColor(C_CARD);
    }

    // Card background
    m_rt->FillRoundedRectangle(D2D1::RoundedRect(rect, rnd, rnd),
                                hovered ? m_brushCardHover.Get() : m_brushCard.Get());

    // Art / placeholder
    ID2D1Bitmap* bmp = GetArt(game.id);
    if (bmp) {
        D2D1_RECT_F src = D2D1::RectF(0, 0, (float)bmp->GetSize().width,
                                            (float)bmp->GetSize().height);
        // Use a layer with a rounded-rect geometry mask for clean art clipping
        ComPtr<ID2D1Layer> layer;
        if (SUCCEEDED(m_rt->CreateLayer(nullptr, layer.GetAddressOf()))) {
            D2D1_ROUNDED_RECT clipRR = D2D1::RoundedRect(rect, rnd, rnd);
            ComPtr<ID2D1RoundedRectangleGeometry> geom;
            m_factory->CreateRoundedRectangleGeometry(clipRR, geom.GetAddressOf());
            if (geom) {
                D2D1_LAYER_PARAMETERS lp = D2D1::LayerParameters(
                    D2D1::InfiniteRect(), geom.Get());
                m_rt->PushLayer(lp, layer.Get());
                m_rt->DrawBitmap(bmp, rect, 1.0f,
                                 D2D1_BITMAP_INTERPOLATION_MODE_LINEAR, src);
                m_rt->PopLayer();
            } else {
                m_rt->DrawBitmap(bmp, rect, 1.0f,
                                 D2D1_BITMAP_INTERPOLATION_MODE_LINEAR, src);
            }
        }
    } else {
        DrawPlaceholderArt(rect, game.platform);
    }

    // Bottom gradient overlay for title readability
    float gradH = 60.0f;
    D2D1_RECT_F gradRect = D2D1::RectF(rect.left, rect.bottom - gradH,
                                        rect.right, rect.bottom);
    m_brushOverlay->SetColor(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.75f));
    m_rt->FillRoundedRectangle(D2D1::RoundedRect(gradRect, 0, 0), m_brushOverlay.Get());
    m_brushOverlay->SetColor(C_OVERLAY);

    // Game title inside card at bottom
    D2D1_RECT_F titleRect = D2D1::RectF(rect.left + 6, rect.bottom - 40,
                                         rect.right - 6, rect.bottom - 4);
    m_fmtCard->SetWordWrapping(DWRITE_WORD_WRAPPING_WRAP);
    m_rt->DrawText(game.title.c_str(), (UINT32)game.title.size(),
                   m_fmtCard.Get(), titleRect, m_brushWhite.Get());
    m_fmtCard->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);

    // Platform badge (top-right corner)
    DrawPlatformBadge(game.platform,
                      D2D1::Point2F(rect.right - 14, rect.top + 14));

    // Selected border
    if (selected) {
        m_brushSelected->SetColor(C_SELECTED);
        m_rt->DrawRoundedRectangle(D2D1::RoundedRect(rect, rnd, rnd),
                                   m_brushSelected.Get(), 2.0f);
        m_brushSelected->SetColor(C_SELECTED);
    } else if (hovered) {
        m_brushAccent->SetColor(D2D1::ColorF(C_ACCENT.r, C_ACCENT.g, C_ACCENT.b, 0.6f));
        m_rt->DrawRoundedRectangle(D2D1::RoundedRect(rect, rnd, rnd),
                                   m_brushAccent.Get(), 1.5f);
        m_brushAccent->SetColor(C_ACCENT);
    }
}

void Renderer::DrawPlatformBadge(Platform p, D2D1_POINT_2F center) {
    D2D1_ELLIPSE e = D2D1::Ellipse(center, 8.0f, 8.0f);
    m_brushCard->SetColor(D2D1::ColorF(0.05f, 0.05f, 0.08f, 0.85f));
    m_rt->FillEllipse(e, m_brushCard.Get());
    m_brushAccent->SetColor(PlatformColor(p));
    D2D1_ELLIPSE inner = D2D1::Ellipse(center, 5.0f, 5.0f);
    m_rt->FillEllipse(inner, m_brushAccent.Get());
    m_brushCard->SetColor(C_CARD);
    m_brushAccent->SetColor(C_ACCENT);
}

void Renderer::DrawPlaceholderArt(D2D1_RECT_F rect, Platform p) {
    // Gradient placeholder using platform color
    D2D1_COLOR_F col = PlatformColor(p);
    col.a = 0.15f;
    m_brushCard->SetColor(col);
    m_rt->FillRoundedRectangle(D2D1::RoundedRect(rect, 8, 8), m_brushCard.Get());
    m_brushCard->SetColor(C_CARD);

    // Platform name centered
    m_brushSubtext->SetColor(D2D1::ColorF(col.r, col.g, col.b, 0.5f));
    std::wstring label = PlatformName(p);
    m_rt->DrawText(label.c_str(), (UINT32)label.size(), m_fmtTitle.Get(),
                   rect, m_brushSubtext.Get());
    m_brushSubtext->SetColor(C_SUBTEXT);
}

void Renderer::DrawDetailPanel(const Game* game) {
    if (!game) return;
    float w = (float)m_width, h = (float)m_height;

    // Dark overlay
    m_brushOverlay->SetColor(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.88f));
    m_rt->FillRectangle(D2D1::RectF(0, 0, w, h), m_brushOverlay.Get());
    m_brushOverlay->SetColor(C_OVERLAY);

    // Panel
    float panelW = std::min(900.0f, w - 80);
    float panelH = std::min(520.0f, h - 80);
    float px = (w - panelW) / 2;
    float py = (h - panelH) / 2;
    D2D1_RECT_F panel = D2D1::RectF(px, py, px + panelW, py + panelH);
    m_rt->FillRoundedRectangle(D2D1::RoundedRect(panel, 12, 12), m_brushSidebar.Get());
    m_rt->DrawRoundedRectangle(D2D1::RoundedRect(panel, 12, 12), m_brushCard.Get(), 1.5f);

    // Art on left
    float artW = 200, artH = 290;
    D2D1_RECT_F artRect = D2D1::RectF(px + 24, py + 24, px + 24 + artW, py + 24 + artH);
    ID2D1Bitmap* bmp = GetArt(game->id);
    if (bmp) {
        D2D1_RECT_F src = D2D1::RectF(0, 0, (float)bmp->GetSize().width,
                                            (float)bmp->GetSize().height);
        m_rt->DrawBitmap(bmp, artRect, 1.0f, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR, src);
    } else {
        DrawPlaceholderArt(artRect, game->platform);
    }

    // Info on right
    float ix = px + 24 + artW + 24;
    float iy = py + 28;
    float iw = panelW - artW - 72;

    // Title
    D2D1_RECT_F titleR = D2D1::RectF(ix, iy, ix + iw, iy + 50);
    m_fmtHeading->SetWordWrapping(DWRITE_WORD_WRAPPING_WRAP);
    m_rt->DrawText(game->title.c_str(), (UINT32)game->title.size(),
                   m_fmtHeading.Get(), titleR, m_brushText.Get());
    m_fmtHeading->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
    iy += 60;

    // Platform
    std::wstring platStr = L"Platform: " + PlatformName(game->platform);
    m_rt->DrawText(platStr.c_str(), (UINT32)platStr.size(), m_fmtDetail.Get(),
                   D2D1::RectF(ix, iy, ix + iw, iy + 26), m_brushSubtext.Get());
    iy += 30;

    // Playtime
    std::wstring ptStr = L"Playtime: " + game->PlaytimeStr();
    m_rt->DrawText(ptStr.c_str(), (UINT32)ptStr.size(), m_fmtDetail.Get(),
                   D2D1::RectF(ix, iy, ix + iw, iy + 26), m_brushSubtext.Get());
    iy += 30;

    // Last played
    if (game->lastPlayed > 0) {
        SYSTEMTIME st;
        FILETIME ft;
        LONGLONG ll = (LONGLONG)(game->lastPlayed) * 10000000LL + 116444736000000000LL;
        ft.dwLowDateTime  = (DWORD)(ll & 0xFFFFFFFF);
        ft.dwHighDateTime = (DWORD)(ll >> 32);
        FileTimeToSystemTime(&ft, &st);
        wchar_t dateBuf[64];
        swprintf_s(dateBuf, L"Last played: %04d-%02d-%02d",
                   st.wYear, st.wMonth, st.wDay);
        m_rt->DrawText(dateBuf, (UINT32)wcslen(dateBuf), m_fmtDetail.Get(),
                       D2D1::RectF(ix, iy, ix + iw, iy + 26), m_brushSubtext.Get());
        iy += 30;
    }

    // IGDB rating
    if (!game->RatingStr().empty()) {
        std::wstring rStr = L"Rating: " + game->RatingStr();
        D2D1_COLOR_F rCol = game->RatingColor();
        m_brushAccent->SetColor(rCol);
        m_rt->DrawText(rStr.c_str(), (UINT32)rStr.size(), m_fmtDetail.Get(),
                       D2D1::RectF(ix, iy, ix + iw, iy + 26), m_brushAccent.Get());
        m_brushAccent->SetColor(C_ACCENT);
        iy += 30;
    }

    // Genres
    if (!game->genres.empty()) {
        std::wstring gStr = L"Genres: " + game->genres;
        m_rt->DrawText(gStr.c_str(), (UINT32)gStr.size(), m_fmtSmall.Get(),
                       D2D1::RectF(ix, iy, ix + iw, iy + 22), m_brushSubtext.Get());
        iy += 26;
    }

    // Release year
    if (game->releaseDate > 0) {
        SYSTEMTIME rst;
        FILETIME rft;
        LONGLONG rll = (LONGLONG)(game->releaseDate) * 10000000LL + 116444736000000000LL;
        rft.dwLowDateTime  = (DWORD)(rll & 0xFFFFFFFF);
        rft.dwHighDateTime = (DWORD)(rll >> 32);
        FileTimeToSystemTime(&rft, &rst);
        std::wstring yrStr = L"Released: " + std::to_wstring(rst.wYear);
        m_rt->DrawText(yrStr.c_str(), (UINT32)yrStr.size(), m_fmtSmall.Get(),
                       D2D1::RectF(ix, iy, ix + iw, iy + 22), m_brushSubtext.Get());
        iy += 26;
    }

    // Summary
    if (!game->summary.empty()) {
        float summaryH = py + panelH - 80 - iy;
        if (summaryH > 30) {
            D2D1_RECT_F sumRect = D2D1::RectF(ix, iy, ix + iw, iy + summaryH);
            // Clip so it doesn't bleed into the button area
            m_rt->PushAxisAlignedClip(sumRect, D2D1_ANTIALIAS_MODE_ALIASED);
            m_rt->DrawText(game->summary.c_str(), (UINT32)game->summary.size(),
                           m_fmtSummary.Get(), sumRect, m_brushSubtext.Get());
            m_rt->PopAxisAlignedClip();
        }
    }

    // Launch button
    float btnW = 160, btnH = 44;
    float btnX = ix, btnY = py + panelH - 70;
    m_launchBtnRect = D2D1::RectF(btnX, btnY, btnX + btnW, btnY + btnH);
    m_rt->FillRoundedRectangle(D2D1::RoundedRect(m_launchBtnRect, 8, 8), m_brushAccent.Get());
    m_rt->DrawText(L"Launch  [Enter]", 15, m_fmtSmall.Get(), m_launchBtnRect, m_brushBg.Get());

    // Keyboard hints row (bottom-right of panel)
    D2D1_RECT_F hintR = D2D1::RectF(btnX + btnW + 12, btnY + 4, px + panelW - 10, btnY + btnH - 4);
    m_rt->DrawText(L"← → prev/next  Esc close", 25, m_fmtSmall.Get(), hintR, m_brushSubtext.Get());

    // Close X in top-right corner
    D2D1_RECT_F closeR = D2D1::RectF(px + panelW - 40, py + 8, px + panelW - 8, py + 36);
    m_rt->FillRoundedRectangle(D2D1::RoundedRect(closeR, 4, 4), m_brushCard.Get());
    m_rt->DrawText(L"✕", 1, m_fmtSmall.Get(), closeR, m_brushSubtext.Get());
}

// ── Art cache ─────────────────────────────────────────────────────────────────

bool Renderer::LoadGameArt(const std::wstring& gameId, const std::wstring& path) {
    ComPtr<ID2D1Bitmap> bmp = LoadBitmapFromFile(path);
    if (!bmp) return false;
    std::lock_guard<std::mutex> lk(m_artMutex);
    m_artCache[gameId] = std::move(bmp);
    return true;
}

void Renderer::UnloadGameArt(const std::wstring& gameId) {
    std::lock_guard<std::mutex> lk(m_artMutex);
    m_artCache.erase(gameId);
}

ID2D1Bitmap* Renderer::GetArt(const std::wstring& id) const {
    std::lock_guard<std::mutex> lk(m_artMutex);
    auto it = m_artCache.find(id);
    return (it != m_artCache.end()) ? it->second.Get() : nullptr;
}

ComPtr<ID2D1Bitmap> Renderer::LoadBitmapFromFile(const std::wstring& path) {
    if (path.empty()) return nullptr;

    ComPtr<IWICBitmapDecoder>    dec;
    ComPtr<IWICBitmapFrameDecode> frame;
    ComPtr<IWICFormatConverter>  conv;

    if (FAILED(m_wic->CreateDecoderFromFilename(path.c_str(), nullptr,
               GENERIC_READ, WICDecodeMetadataCacheOnLoad, dec.GetAddressOf())))
        return nullptr;
    if (FAILED(dec->GetFrame(0, frame.GetAddressOf()))) return nullptr;
    if (FAILED(m_wic->CreateFormatConverter(conv.GetAddressOf()))) return nullptr;
    if (FAILED(conv->Initialize(frame.Get(), GUID_WICPixelFormat32bppPBGRA,
               WICBitmapDitherTypeNone, nullptr, 0.0,
               WICBitmapPaletteTypeMedianCut))) return nullptr;

    ComPtr<ID2D1Bitmap> bmp;
    m_rt->CreateBitmapFromWicBitmap(conv.Get(), nullptr, bmp.GetAddressOf());
    return bmp;
}

// ── Hit testing ───────────────────────────────────────────────────────────────

int Renderer::HitTestGrid(float x, float y, const RenderState& state,
                           size_t gameCount) const {
    if (x < m_sidebarW || y < m_topbarH) return -1;
    float gx = x - m_sidebarW - m_tileGap;
    float gy = y - m_topbarH - m_tileGap + state.scrollOffset;
    int col = (int)(gx / (m_tileW + m_tileGap));
    int row = (int)(gy / (m_tileH + m_tileGap + 22.0f));
    if (col < 0 || col >= m_cols || row < 0) return -1;
    // Check we're actually inside a tile (not in the gap)
    float localX = fmodf(gx, m_tileW + m_tileGap);
    float localY = fmodf(gy, m_tileH + m_tileGap + 22.0f);
    if (localX > m_tileW || localY > m_tileH) return -1;
    int idx = row * m_cols + col;
    return (idx < (int)gameCount) ? idx : -1;
}

bool Renderer::HitTestSidebar(float x, float y, const RenderState& state,
                               Platform& outPlatform, bool& outAll) const {
    if (x >= m_sidebarW) return false;
    auto entries = BuildSidebarEntries(state);
    float ey = m_topbarH + 12.0f;
    for (auto& e : entries) {
        if (y >= ey && y <= ey + 38.0f) {
            outAll      = e.all;
            outPlatform = e.p;
            return true;
        }
        ey += 42.0f;
    }
    return false;
}

bool Renderer::HitTestSearch(float x, float y) const {
    return x >= m_searchRect.left && x <= m_searchRect.right &&
           y >= m_searchRect.top  && y <= m_searchRect.bottom;
}

bool Renderer::HitTestLaunchBtn(float x, float y) const {
    return x >= m_launchBtnRect.left && x <= m_launchBtnRect.right &&
           y >= m_launchBtnRect.top  && y <= m_launchBtnRect.bottom;
}

bool Renderer::HitTestSettingsBtn(float x, float y) const {
    return x >= m_settingsBtnRect.left && x <= m_settingsBtnRect.right &&
           y >= m_settingsBtnRect.top  && y <= m_settingsBtnRect.bottom;
}

bool Renderer::HitTestEmptyStateBtn(float x, float y) const {
    if (m_emptyStateBtnRect.right == m_emptyStateBtnRect.left) return false;
    return x >= m_emptyStateBtnRect.left && x <= m_emptyStateBtnRect.right &&
           y >= m_emptyStateBtnRect.top  && y <= m_emptyStateBtnRect.bottom;
}

float Renderer::ScrollForSelected(int idx, float currentScroll, float viewportH) const {
    if (idx < 0 || m_cols <= 0) return currentScroll;
    int row = idx / m_cols;
    float rowH   = m_tileH + m_tileGap + 22.0f;
    // On-screen top of this card:  m_topbarH + m_tileGap + row*rowH - scroll
    float cardTop    = m_topbarH + m_tileGap + (float)row * rowH;
    float cardBottom = cardTop + m_tileH;

    float screenTop    = cardTop    - currentScroll;
    float screenBottom = cardBottom - currentScroll;

    if (screenTop < m_topbarH + 4.0f) {
        // Card is above viewport — scroll up so it appears at top
        return std::max(0.0f, cardTop - m_topbarH - m_tileGap);
    }
    if (screenBottom > viewportH - 4.0f) {
        // Card is below viewport — scroll down so it appears at bottom
        return cardBottom - viewportH + m_tileGap;
    }
    return currentScroll;
}
