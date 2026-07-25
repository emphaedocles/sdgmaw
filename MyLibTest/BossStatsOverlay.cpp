#include "pch.h"
#include "BossStatsOverlay.h"
#include "MobRadar.h"
#include <windows.h>
#include <windowsx.h>
#include <objidl.h>
#include <gdiplus.h>
#include <string>
#include <sstream>
#include <map>
#include <numbers>

#pragma comment(lib, "gdiplus.lib")

using namespace Gdiplus;

// ---------------------------------------------------------------------------
// GDI+ ref-count
// ---------------------------------------------------------------------------
static ULONG_PTR s_gdipToken = 0;
static int s_gdipRef = 0;
static void GdipStart() { if (s_gdipRef++ == 0) { GdiplusStartupInput i; GdiplusStartup(&s_gdipToken, &i, nullptr); } }
static void GdipStop() { if (--s_gdipRef == 0 && s_gdipToken) { GdiplusShutdown(s_gdipToken); s_gdipToken = 0; } }

// ---------------------------------------------------------------------------
// Statics
// ---------------------------------------------------------------------------
static HWND s_hwnd = nullptr;
static const int SLOT_H = 48;   // height per boss slot (pixels)
static const int WIN_W = 300;  // fixed width (matches DEFAULT_W in MobRadar)
static const int MAX_BOSS = 8;   // maximum simultaneous boss entries shown

// Messages
static const UINT WM_BSO_UPDATE = WM_USER + 0x300;
static const UINT WM_BSO_REMOVE = WM_USER + 0x301;
static const UINT WM_BSO_CLEAR = WM_USER + 0x302;
static const UINT WM_BSO_VISIBLE = WM_USER + 0x303;

// Boss entry post packet (heap-allocated, handler frees)
struct BsoUpdatePost
{
    int id;
    int health;
    int maxHealth;
    int tier;
    wchar_t name[64];
};

// UI-thread storage
static std::map<int, BossStatEntry> s_bosses;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static std::wstring Utf8ToWide(const char* s)
{
    if (!s || !*s) return std::wstring();
    int len = MultiByteToWideChar(CP_UTF8, 0, s, -1, nullptr, 0);
    if (len <= 0) return std::wstring();
    std::wstring out(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s, -1, &out[0], len);
    if (!out.empty() && out.back() == L'\0') out.pop_back();
    return out;
}

// Resize window height to fit the current number of boss entries (called on UI thread)
static void ResizeToContent(HWND hwnd)
{
    int count = (int)s_bosses.size();
    // Always at least one slot: either boss entries or the "no bosses" placeholder
    if (count < 1) count = 1;
    int newH = count * SLOT_H + 4; // +4 for bottom padding
    RECT wr; GetWindowRect(hwnd, &wr);
    int curW = wr.right - wr.left;
    SetWindowPos(hwnd, nullptr, 0, 0, curW, newH, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
}

// ---------------------------------------------------------------------------
// Painting
// ---------------------------------------------------------------------------
static void OnPaint(HWND hwnd)
{
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);

    RECT rc;
    GetClientRect(hwnd, &rc);
    int W = rc.right - rc.left;
    int H = rc.bottom - rc.top;
    if (W <= 0 || H <= 0) { EndPaint(hwnd, &ps); return; }

    if (s_gdipToken)
    {
        Bitmap bmp(W, H, PixelFormat32bppPARGB);
        Graphics g(&bmp);
        g.SetSmoothingMode(SmoothingModeHighQuality);

        // Window background
        SolidBrush bgBrush(Color(210, 18, 18, 18));
        g.FillRectangle(&bgBrush, RectF(0, 0, (REAL)W, (REAL)H));

        FontFamily ff(L"Segoe UI");
        Font nameFont(&ff, 11.0f, FontStyleBold, UnitPixel);
        Font hpFont(&ff, 10.0f, FontStyleRegular, UnitPixel);
        Font emptyFont(&ff, 11.0f, FontStyleItalic, UnitPixel);
        SolidBrush textBrush(Color(255, 230, 230, 230));
        SolidBrush dimBrush(Color(255, 110, 110, 110));   // muted grey for placeholder
        Pen slotPen(Color(100, 80, 80, 80), 1.0f);

        const float margin = 6.0f;
        const float barH = 14.0f;

        // --- Empty state ---
        if (s_bosses.empty())
        {
            RectF clientF(0.0f, 0.0f, (REAL)W, (REAL)H);
            StringFormat sf;
            sf.SetAlignment(StringAlignmentCenter);
            sf.SetLineAlignment(StringAlignmentCenter);
            g.DrawString(L"< No Bosses Detected >", -1, &emptyFont, clientF, &sf, &dimBrush);
        }
        else
        {
            int slotIdx = 0;
            for (auto& kv : s_bosses)
            {
                const BossStatEntry& boss = kv.second;
                float slotTop = (float)(slotIdx * SLOT_H);

                // Slot separator line (except first)
                if (slotIdx > 0)
                    g.DrawLine(&slotPen, 0.0f, slotTop, (REAL)W, slotTop);

                // -- Name row --
                std::wstring wname = Utf8ToWide(boss.Name.c_str());
                wname += L"  [T" + std::to_wstring(boss.Tier) + L"]";
                g.DrawString(wname.c_str(), -1, &nameFont,
                    PointF(margin, slotTop + 4.0f), &textBrush);

                // -- Health bar background --
                float barY = slotTop + 22.0f;
                float barW = (float)W - margin * 2.0f;
                RectF barBg(margin, barY, barW, barH);
                SolidBrush barBackBrush(Color(255, 45, 45, 45));
                g.FillRectangle(&barBackBrush, barBg);

                // -- Health fraction --
                float fraction = 0.0f;
                if (boss.MaxHealth > 0)
                    fraction = (float)boss.Health / (float)boss.MaxHealth;
                if (fraction < 0.0f) fraction = 0.0f;
                if (fraction > 1.0f) fraction = 1.0f;

                // -- Gradient health bar (red -> yellow -> green) --
                if (fraction > 0.0f && barW > 0.0f)
                {
                    LinearGradientBrush gradBrush(
                        PointF(margin, barY),
                        PointF(margin + barW, barY),
                        Color(255, 220, 20, 20),
                        Color(255, 20, 220, 20));

                    Color gradColors[3] = {
                        Color(255, 220, 20, 20),
                        Color(255, 240, 220, 20),
                        Color(255, 20, 220, 20)
                    };
                    REAL gradPos[3] = { 0.0f, 0.5f, 1.0f };
                    gradBrush.SetInterpolationColors(gradColors, gradPos, 3);

                    RectF filledRect(margin, barY, barW * fraction, barH);
                    Region oldClip;
                    g.GetClip(&oldClip);
                    g.SetClip(filledRect);
                    g.FillRectangle(&gradBrush, barBg);
                    g.SetClip(&oldClip);
                }

                // -- Bar border --
                Pen barPen(Color(150, 150, 150, 150), 1.0f);
                g.DrawRectangle(&barPen, barBg);

                // -- Health text overlay (centred on bar) --
                std::wostringstream hpStr;
                hpStr << boss.Health << L" / " << boss.MaxHealth;
                StringFormat sf;
                sf.SetAlignment(StringAlignmentCenter);
                sf.SetLineAlignment(StringAlignmentCenter);
                SolidBrush hpTextBrush(Color(255, 64, 128, 128));
                g.DrawString(hpStr.str().c_str(), -1, &hpFont, barBg, &sf, &hpTextBrush);

                ++slotIdx;
                if (slotIdx >= MAX_BOSS) break;
            }
        }

        // Blit to screen
        Graphics screenG(hdc);
        screenG.DrawImage(&bmp, 0, 0, W, H);
    }
    else
    {
        // GDI fallback
        HBRUSH bg = CreateSolidBrush(RGB(18, 18, 18));
        FillRect(hdc, &rc, bg);
        DeleteObject(bg);
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(110, 110, 110));
        RECT tr = rc;
        DrawTextW(hdc, L"< No Bosses Detected >", -1, &tr,
            DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    EndPaint(hwnd, &ps);
}

// ---------------------------------------------------------------------------
// Window procedure
// ---------------------------------------------------------------------------
static LRESULT CALLBACK BsoWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
        s_hwnd = hwnd;
        return 0;

    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT:
        OnPaint(hwnd);
        return 0;

    case WM_NCHITTEST:
    {
        // Allow dragging from any part of the client area via top-strip
        LRESULT hit = DefWindowProc(hwnd, WM_NCHITTEST, wParam, lParam);
        if (hit == HTCLIENT)
        {
            POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            ScreenToClient(hwnd, &pt);
            if (pt.y >= 0 && pt.y <= 18)
                return HTCAPTION;
        }
        return hit;
    }

    case WM_BSO_UPDATE:
    {
        auto* pkt = reinterpret_cast<BsoUpdatePost*>(lParam);
        if (pkt)
        {
            // Convert wide name back to UTF-8 for storage
            char nameBuf[128] = {};
            WideCharToMultiByte(CP_UTF8, 0, pkt->name, -1, nameBuf, sizeof(nameBuf), nullptr, nullptr);

            BossStatEntry e;
            e.id = pkt->id;
            e.Name = nameBuf;
            e.Health = pkt->health;
            e.MaxHealth = pkt->maxHealth;
            e.Tier = pkt->tier;

            if (pkt->health <= 0)
                s_bosses.erase(pkt->id);   // dead / remove
            else
                s_bosses[pkt->id] = e;

            delete pkt;
            ResizeToContent(hwnd);
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;
    }

    case WM_BSO_REMOVE:
    {
        int id = (int)wParam;
        s_bosses.erase(id);
        ResizeToContent(hwnd);
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }

    case WM_BSO_CLEAR:
        s_bosses.clear();
        ResizeToContent(hwnd);
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;

    case WM_BSO_VISIBLE:
        if ((int)wParam)
            SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
        else
            ShowWindow(hwnd, SW_HIDE);
        return 0;

    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        s_hwnd = nullptr;
        GdipStop();
        return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------
void BossStatsOverlay::Show(const char* title)
{
    if (s_hwnd) return;
    GdipStart();

    HINSTANCE hInst = GetModuleHandle(nullptr);
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = BsoWndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = L"BossStatsOverlayClass";
    RegisterClassExW(&wc);

    std::wstring wtitle = Utf8ToWide(title ? title : "Boss Stats");

    // Position just below the radar window (if visible), otherwise top-right
    int x = 0, y = 0;
    HWND radarHwnd = MobRadar::GetHwnd();
    if (radarHwnd)
    {
        RECT rr; GetWindowRect(radarHwnd, &rr);
        x = rr.left;
        y = rr.bottom + 2; // 2px gap below radar
    }
    else
    {
        HMONITOR hMon = MonitorFromPoint({ 0,0 }, MONITOR_DEFAULTTOPRIMARY);
        MONITORINFO mi; mi.cbSize = sizeof(mi);
        if (GetMonitorInfoW(hMon, &mi))
        {
            x = mi.rcWork.right - WIN_W - 10;
            y = mi.rcWork.top + 10;
        }
    }

    DWORD ex = WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED;
    DWORD style = WS_POPUP | WS_VISIBLE;

    s_hwnd = CreateWindowExW(ex, wc.lpszClassName, wtitle.c_str(), style,
        x, y, WIN_W, SLOT_H, nullptr, nullptr, hInst, nullptr);
    if (!s_hwnd) { GdipStop(); return; }

    SetLayeredWindowAttributes(s_hwnd, 0, 210, LWA_ALPHA);
    ShowWindow(s_hwnd, SW_SHOW);
    UpdateWindow(s_hwnd);
}

void BossStatsOverlay::Close()
{
    if (s_hwnd) DestroyWindow(s_hwnd);
}

void BossStatsOverlay::UpdateBoss(int id, const char* name, int health, int maxHealth, int tier)
{
    if (!s_hwnd) return;
    auto* pkt = new BsoUpdatePost();
    pkt->id = id;
    pkt->health = health;
    pkt->maxHealth = maxHealth;
    pkt->tier = tier;
    // Convert name to wide for packet
    if (name)
        MultiByteToWideChar(CP_UTF8, 0, name, -1, pkt->name, 64);
    else
        pkt->name[0] = L'\0';

    PostMessageW(s_hwnd, WM_BSO_UPDATE, 0, (LPARAM)pkt);
}

void BossStatsOverlay::RemoveBoss(int id)
{
    if (s_hwnd) PostMessageW(s_hwnd, WM_BSO_REMOVE, (WPARAM)id, 0);
}

void BossStatsOverlay::ClearBosses()
{
    if (s_hwnd) PostMessageW(s_hwnd, WM_BSO_CLEAR, 0, 0);
}

void BossStatsOverlay::SetVisible(bool visible)
{
    if (s_hwnd) PostMessageW(s_hwnd, WM_BSO_VISIBLE, (WPARAM)(visible ? 1 : 0), 0);
}

bool BossStatsOverlay::IsVisible()
{
    return s_hwnd && IsWindowVisible(s_hwnd);
}

HWND BossStatsOverlay::GetHwnd()
{
    return s_hwnd;
}