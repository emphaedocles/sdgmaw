#include "pch.h"
#include "CharacterStatsGdi.h"
#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>
#include <string>
#include <sstream>

#pragma comment(lib, "gdiplus.lib")

using namespace Gdiplus;

ULONG_PTR CharacterStatsGdi::gdiplusToken_ = 0;
int CharacterStatsGdi::gdiplusRefCount_ = 0;

static std::wstring Utf8ToWide(const std::string& s)
{
	if (s.empty()) return std::wstring();
	int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
	if (len == 0) return std::wstring();
	std::wstring out; out.resize(len);
	MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &out[0], len);
	if (!out.empty() && out.back() == L'\0') out.pop_back();
	return out;
}

void CharacterStatsGdi::EnsureGdiplusStarted()
{
	if (gdiplusRefCount_++ == 0)
	{
		GdiplusStartupInput gdiplusStartupInput;
		Gdiplus::Status st = GdiplusStartup(&gdiplusToken_, &gdiplusStartupInput, nullptr);
		(void)st;
	}
}

void CharacterStatsGdi::EnsureGdiplusShutdown()
{
	if (--gdiplusRefCount_ == 0)
	{
		if (gdiplusToken_)
		{
			GdiplusShutdown(gdiplusToken_);
			gdiplusToken_ = 0;
		}
	}
}

CharacterStatsGdi::CharacterStatsGdi()
	: hwnd_(nullptr)
{
	// details_ default-constructed
}

CharacterStatsGdi::~CharacterStatsGdi()
{
	Destroy();
}

HWND CharacterStatsGdi::Create(HWND parent, int id, const RECT& rc)
{
	if (hwnd_) return hwnd_; // already created

	EnsureGdiplusStarted();

	// Register class if necessary
	WNDCLASSW wc = {};
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = CharacterStatsGdi::StaticWndProc;
	wc.hInstance = GetModuleHandle(nullptr);
	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wc.lpszClassName = L"CharacterStatsGdiClass";
	// RegisterClass may fail if already registered; that's fine.
	RegisterClassW(&wc);

	hwnd_ = CreateWindowExW(0, wc.lpszClassName, L"",
		WS_CHILD | WS_VISIBLE,
		rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top,
		parent, (HMENU)(intptr_t)id, GetModuleHandle(nullptr), this);

	return hwnd_;
}

void CharacterStatsGdi::Destroy()
{
	if (hwnd_)
	{
		// Clear user data so window proc won't reference freed object
		SetWindowLongPtr(hwnd_, GWLP_USERDATA, 0);
		DestroyWindow(hwnd_);
		hwnd_ = nullptr;
		EnsureGdiplusShutdown();
	}
}

void CharacterStatsGdi::SetDetails(const CharacterDetails& details)
{
	details_ = details;
	if (hwnd_) InvalidateRect(hwnd_, nullptr, TRUE);
}

LRESULT CALLBACK CharacterStatsGdi::StaticWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	CharacterStatsGdi* self = (CharacterStatsGdi*)GetWindowLongPtr(hwnd, GWLP_USERDATA);

	if (msg == WM_NCCREATE)
	{
		CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
		self = reinterpret_cast<CharacterStatsGdi*>(cs->lpCreateParams);
		SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)self);
		if (self) self->hwnd_ = hwnd;
		return DefWindowProc(hwnd, msg, wParam, lParam);
	}

	if (!self)
		return DefWindowProc(hwnd, msg, wParam, lParam);

	return self->WndProc(msg, wParam, lParam);
}

LRESULT CALLBACK CharacterStatsGdi::WndProc(UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_PAINT:
		OnPaint();
		return 0;
	case WM_SIZE:
		InvalidateRect(hwnd_, nullptr, TRUE);
		return 0;
	case WM_ERASEBKGND:
		// handled by painting code (double-buffered)
		return TRUE;
	case WM_DESTROY:
		// Object will shutdown GDI+ in Destroy() call chain.
		return 0;
	}
	return DefWindowProc(hwnd_, msg, wParam, lParam);
}

void CharacterStatsGdi::OnPaint()
{
	PAINTSTRUCT ps;
	HDC hdc = BeginPaint(hwnd_, &ps);

	RECT rc;
	GetClientRect(hwnd_, &rc);
	int width = rc.right - rc.left;
	int height = rc.bottom - rc.top;
	if (width <= 0 || height <= 0) {
		EndPaint(hwnd_, &ps);
		return;
	}

	if (gdiplusToken_)
	{
		// Offscreen GDI+ bitmap for double-buffered rendering
		Bitmap bitmap(width, height, PixelFormat32bppPARGB);
		Graphics memG(&bitmap);
		memG.SetSmoothingMode(SmoothingModeHighQuality);

		// Drawing operations use memG (offscreen)
		RectF clientF(0.0f, 0.0f, (REAL)width, (REAL)height);

		// Background
		SolidBrush bgBrush(Color(255, 30, 30, 30));
		memG.FillRectangle(&bgBrush, clientF);

		// Border
		Pen borderPen(Color(180, 180, 180), 1.0f);
		memG.DrawRectangle(&borderPen, clientF);

		// Text layout
		FontFamily fontFamily(L"Segoe UI");
		Font font(&fontFamily, 11.0f, FontStyleRegular, UnitPixel);
		SolidBrush textBrush(Color(230, 230, 230));
		SolidBrush accentBrush(Color(180, 200, 255));

		float margin = 8.0f;
		float x = clientF.X + margin;
		float y = clientF.Y + margin;

		// Name (accent)
		std::wstring name = Utf8ToWide(details_.Name);
		name.append(L" - Level ");
		name.append(std::to_wstring(details_.Level));
		name.append(L" ");
		name.append(Utf8ToWide(details_.Class));

		if (!name.empty())
		{
			Font nameFont(&fontFamily, 13.0f, FontStyleBold, UnitPixel);
			memG.DrawString(name.c_str(), -1, &nameFont, PointF(x, y), &accentBrush);
			y += 20.0f;
		}

		// Health bar area (with gradient)
		float barHeight = 16.0f;
		float barWidth = (clientF.Width - margin * 2)/2.0f;
		float barX = x;
		float barY = y;
		RectF barBg(barX, barY, barWidth, barHeight);

		// Draw background bar
		SolidBrush barBack(Color(150, 50, 50, 50));
		memG.FillRectangle(&barBack, barBg);

		// Compute health fraction
		float healthFraction = 0.0f;
		if (details_.MaxHealth > 0)
			healthFraction = (float)details_.Health / (float)details_.MaxHealth;
		if (healthFraction < 0.0f) healthFraction = 0.0f;
		if (healthFraction > 1.0f) healthFraction = 1.0f;

		// Gradient brush for the full bar (we will clip to fraction using a region)
		LinearGradientBrush gradBrush(
			PointF(barX, barY),
			PointF(barX + barWidth, barY),
			Color(255, 220, 20, 20), // start (red)
			Color(255, 20, 220, 20)  // end (green)
		);

		// Make a richer gradient (red -> yellow -> green)
		Gdiplus::Color colorsArr[3] = {
			Color(255, 220, 20, 20),
			Color(255, 240, 220, 20),
			Color(255, 20, 220, 20)
		};
		REAL positionsArr[3] = { 0.0f, 0.5f, 1.0f };
		gradBrush.SetInterpolationColors(colorsArr, positionsArr, 3);

		// Clip to fraction rect and fill with gradient
		RectF filledRect(barX, barY, barWidth * healthFraction, barHeight);
		Region oldClip;
		memG.GetClip(&oldClip);
		memG.SetClip(filledRect);
		memG.FillRectangle(&gradBrush, barBg);
		memG.SetClip(&oldClip);

		// Draw bar border
		Pen barPen(Color(200, 200, 200), 1.0f);
		memG.DrawRectangle(&barPen, barBg);

		// Health text overlay
		std::wostringstream hpText;
		hpText << details_.Health << L" / " << details_.MaxHealth;
		Font hpFont(&fontFamily, 10.0f, FontStyleBoldItalic, UnitPixel);
		Font sFont(&fontFamily, 12.0f, FontStyleBold, UnitPixel);

		RectF hpRect(barX, barY, barWidth, barHeight);
		StringFormat sf;
		sf.SetAlignment(StringAlignmentCenter);
		sf.SetLineAlignment(StringAlignmentCenter);
		SolidBrush hpBrush(Color(255, 32, 32, 32));
		SolidBrush hpNegBrush(Color(255, 128,16 , 16));
		if (details_.Health > 0) {
			memG.DrawString(hpText.str().c_str(), -1, &hpFont, hpRect, &sf, &hpBrush);
		}
		else
		{
			memG.DrawString(hpText.str().c_str(), -1, &hpFont, hpRect, &sf, &hpNegBrush);
		}
		// Move y below the bar (if more content is needed later)
		y += barHeight + 1.0f;
		barY = y;
		// mana text overlay
		std::wostringstream mpText;
		mpText << details_.Mana;// << L" / " << details_.MaxHealth;
		float mpPercent = details_.MaxMana / (float)details_.ManaPoolMax;
		if (mpPercent > 1.0f) mpPercent = 1.0f;
		float mpp = details_.Mana / (float)details_.MaxMana;
		float mpBarWidth = barWidth * mpPercent;
		float mpFilledWidth = mpBarWidth * mpp;
		RectF mpBgRect(barX, barY, barWidth, barHeight);
		RectF mpManaPoolRect(barX, barY, mpBarWidth, barHeight);
		RectF mpRect(barX, barY, mpFilledWidth, barHeight);
		//StringFormat sf;
		//sf.SetAlignment(StringAlignmentCenter);
		//sf.SetLineAlignment(StringAlignmentCenter);
		SolidBrush mpBackBrush(Color(255, 32, 32, 128));
		SolidBrush mpTextBrush(Color(255, 64, 64, 192));
		SolidBrush mpPoolBrush(Color(255, 96, 96, 255));
		SolidBrush mpBrush(Color(255, 128, 255, 255));
		memG.FillRectangle(&mpBackBrush, mpBgRect);
		memG.FillRectangle(&mpPoolBrush, mpManaPoolRect);
		memG.FillRectangle(&mpBrush, mpRect);

		memG.DrawString(mpText.str().c_str(), -1, &hpFont, mpManaPoolRect, &sf, &mpTextBrush);
		// Draw bar border
		Pen barMPen(Color(128, 128, 255), 1.0f);
		memG.DrawRectangle(&barMPen, mpBgRect);
		x = mpBgRect.GetRight() + 3.0f;
		y = 3;// filledRect.GetTop();
		SolidBrush sbAC(Color(255, 203, 182, 116));
		SolidBrush sbMR(Color(255, 255, 64,64));
		SolidBrush sbRR(Color(255, 255,255,64));
		SolidBrush sbSR(Color(255, 64, 64, 255));
		SolidBrush sbVR(Color(255, 64, 255, 64));
		RectF rr = { x, y, 100.0f, 15.0f };
		sf.SetAlignment(StringAlignmentNear);
		std::wostringstream txtAC;
		txtAC << L"AC:" << details_.AC;	
		memG.DrawString(txtAC.str().c_str(), -1, &sFont, rr, &sf, &sbAC);
		rr.Y += rr.Height+1.0f;
		std::wostringstream txtMR;
		txtMR << L"MR:" << details_.MeleeRating.c_str();
		memG.DrawString(txtMR.str().c_str(), -1, &sFont, rr, &sf, &sbMR);
		rr.Y += rr.Height+1.0f;
		std::wostringstream txtRR;
		txtRR << L"RR:" << details_.RangedRating.c_str();
		memG.DrawString(txtRR.str().c_str(), -1, &sFont, rr, &sf, &sbRR);
		rr.Y += rr.Height+1.0f;
		std::wostringstream txtSR;
		txtSR << L"SR:" << details_.SpellRating.c_str();
		memG.DrawString(txtSR.str().c_str(), -1, &sFont, rr, &sf, &sbSR);
		rr.Y += rr.Height+1.0f;
		std::wostringstream txtVR;
		txtVR << L"VR:" << details_.Vitality.c_str();
		memG.DrawString(txtVR.str().c_str(), -1, &sFont, rr, &sf, &sbVR);
		
		RectF conditionRect(clientF.X + margin, clientF.GetBottom() - margin - 35.0f, barWidth, 35.0f);
		std::wostringstream conditionText;
		conditionText << Utf8ToWide(details_.StatusFx);
		SolidBrush conditionBrus(Color(255, 255, 64, 64));
		SolidBrush goodConditionBrus(Color(255, 64, 255, 64));
		// (optional) Additional details rendering can go here...
		
		if(( details_.StatusFx.find("Good") != std::string::npos))
		{
			memG.DrawString(conditionText.str().c_str(), -1, &sFont, conditionRect, &sf, &goodConditionBrus);

		}
		else
		{
			memG.DrawString(conditionText.str().c_str(), -1, &sFont, conditionRect, &sf, &conditionBrus);
		}
		// Blit the offscreen bitmap to the window HDC using a screen Graphics
		Graphics screenG(hdc);
		screenG.SetSmoothingMode(SmoothingModeHighQuality);
		screenG.DrawImage(&bitmap, 0, 0, width, height);
	}
	else
	{
		// Fallback double-buffered GDI drawing to avoid flicker
		HDC memDC = CreateCompatibleDC(hdc);
		if (memDC)
		{
			HBITMAP hbm = CreateCompatibleBitmap(hdc, width, height);
			if (hbm)
			{
				HBITMAP old = (HBITMAP)SelectObject(memDC, hbm);

				// fill background
				RECT fillRc = { 0,0,width,height };
				HBRUSH hBr = CreateSolidBrush(RGB(240, 240, 240));
				FillRect(memDC, &fillRc, hBr);
				DeleteObject(hBr);

				// draw text payload as before using GDI
				std::wstring payload = Utf8ToWide(details_.Name) + L"\r\n" +
					L"Class: " + Utf8ToWide(details_.Class) + L"\r\n" +
					L"Level: " + std::to_wstring(details_.Level) + L"\r\n" +
					L"Health: " + std::to_wstring(details_.Health) + L" / " + std::to_wstring(details_.MaxHealth) + L"\r\n" +
					L"Mana: " + std::to_wstring(details_.Mana) + L" / " + std::to_wstring(details_.MaxMana) + L"\r\n" +
					L"Health Regen: " + std::to_wstring(details_.HealthRegen) + L"\r\n" +
					L"Mana Regen: " + std::to_wstring(details_.ManaRegen) + L"\r\n";

				SetBkMode(memDC, TRANSPARENT);
				SetTextColor(memDC, RGB(0, 0, 0));
				DrawTextW(memDC, payload.c_str(), (int)payload.size(), &fillRc, DT_LEFT | DT_TOP | DT_NOPREFIX);

				// blit to screen
				BitBlt(hdc, 0, 0, width, height, memDC, 0, 0, SRCCOPY);

				// cleanup
				SelectObject(memDC, old);
				DeleteObject(hbm);
			}
			DeleteDC(memDC);
		}
	}

	EndPaint(hwnd_, &ps);
}