/*
 * Chrome Token Signing Native Host
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "BatchPinDialog.h"
#include "BatchPinDialogResource.h"
#include "Labels.h"

#include <Windows.h>
#include <gdiplus.h>

#include <memory>
#include <vector>

#pragma comment(lib, "gdiplus.lib")

namespace {

// Same colours as the Linux and macOS dialogs
const COLORREF TEXT_COLOR = RGB(26, 26, 26);
const COLORREF SECONDARY_COLOR = RGB(96, 96, 96);
const COLORREF ERROR_COLOR = RGB(196, 43, 28);
const COLORREF BODY_COLOR = RGB(255, 255, 255);
const COLORREF FOOTER_COLOR = RGB(240, 240, 240);
const COLORREF SEPARATOR_COLOR = RGB(222, 222, 222);
const Gdiplus::Color PRIMARY(0, 103, 192), PRIMARY_PRESSED(0, 84, 158);
const Gdiplus::Color DISABLED(224, 224, 224), DISABLED_TEXT(160, 160, 160);
const Gdiplus::Color BUTTON(255, 255, 255), BUTTON_PRESSED(235, 235, 235), BUTTON_BORDER(192, 192, 192);

// SetThreadDpiAwarenessContext of Windows 10 1703+, the dialogs stay sharp on scaled displays
typedef HANDLE (WINAPI *SetThreadDpiAwarenessContextFn)(HANDLE);
const HANDLE PER_MONITOR_AWARE_V2 = HANDLE(-4);

struct State {
	const BatchPinRequest *request;
	bool twoFields;
	bool same = false;
	std::string pin, pin2;
	HFONT headingFont = nullptr, boldFont = nullptr;
	std::unique_ptr<Gdiplus::Bitmap> logo;
	HBRUSH body = CreateSolidBrush(BODY_COLOR);
	HBRUSH footer = CreateSolidBrush(FOOTER_COLOR);
};

// Text of the PIN field as UTF-8, PKCS#11 PINs are UTF-8
std::string readUtf8(HWND dialog, int id)
{
	std::wstring text(size_t(GetWindowTextLengthW(GetDlgItem(dialog, id))) + 1, 0);
	text.resize(GetDlgItemTextW(dialog, id, &text[0], int(text.size())));
	int size = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), int(text.size()), nullptr, 0, nullptr, nullptr);
	std::string result(size_t(size), 0);
	WideCharToMultiByte(CP_UTF8, 0, text.c_str(), int(text.size()), &result[0], size, nullptr, nullptr);
	if (!text.empty())
		SecureZeroMemory(&text[0], text.size() * sizeof(wchar_t));
	return result;
}

int fieldLength(HWND dialog, int id)
{
	HWND field = GetDlgItem(dialog, id);
	return field ? GetWindowTextLengthW(field) : -1;
}

HFONT derivedFont(HWND dialog, int numerator, int denominator, LONG weight)
{
	LOGFONTW font;
	if (!GetObjectW(HFONT(SendMessage(dialog, WM_GETFONT, 0, 0)), sizeof(font), &font))
		return nullptr;
	font.lfHeight = font.lfHeight * numerator / denominator;
	font.lfWeight = weight;
	return CreateFontIndirectW(&font);
}

// The application icon is the logo with a wide empty border, crop it to the drawing
std::unique_ptr<Gdiplus::Bitmap> loadLogo()
{
	const int size = 256;
	HICON icon = HICON(LoadImageW(GetModuleHandleW(nullptr), L"IDI_ICON1", IMAGE_ICON, size, size, LR_DEFAULTCOLOR));
	if (!icon)
		return nullptr;
	ICONINFO info;
	BOOL hasInfo = GetIconInfo(icon, &info);
	DestroyIcon(icon);
	if (!hasInfo)
		return nullptr;
	BITMAPINFO bitmapInfo = {};
	bitmapInfo.bmiHeader = { sizeof(BITMAPINFOHEADER), size, -size, 1, 32, BI_RGB };
	std::vector<DWORD> pixels(size * size);
	HDC screen = GetDC(nullptr);
	int lines = GetDIBits(screen, info.hbmColor, 0, size, pixels.data(), &bitmapInfo, DIB_RGB_COLORS);
	ReleaseDC(nullptr, screen);
	DeleteObject(info.hbmColor);
	DeleteObject(info.hbmMask);
	if (lines != size)
		return nullptr;

	int left = size, top = size, right = -1, bottom = -1;
	for (int y = 0; y < size; ++y) {
		for (int x = 0; x < size; ++x) {
			DWORD pixel = pixels[y * size + x];
			BYTE alpha = BYTE(pixel >> 24), darkest = min(min(BYTE(pixel >> 16), BYTE(pixel >> 8)), BYTE(pixel));
			if (alpha > 32 && darkest < 224) {
				left = min(left, x);
				right = max(right, x);
				top = min(top, y);
				bottom = max(bottom, y);
			}
		}
	}
	if (right < left)
		return nullptr;
	int width = right - left + 1, height = bottom - top + 1;
	std::unique_ptr<Gdiplus::Bitmap> logo(new Gdiplus::Bitmap(width, height, PixelFormat32bppARGB));
	Gdiplus::Rect area(0, 0, width, height);
	Gdiplus::BitmapData data;
	if (logo->LockBits(&area, Gdiplus::ImageLockModeWrite, PixelFormat32bppARGB, &data) != Gdiplus::Ok)
		return nullptr;
	for (int y = 0; y < height; ++y)
		memcpy(static_cast<BYTE*>(data.Scan0) + y * data.Stride, &pixels[(top + y) * size + left], width * sizeof(DWORD));
	logo->UnlockBits(&data);
	return logo;
}

// Hides an empty line and moves the controls below it up, like a hidden label on Linux
void collapse(HWND dialog, int id)
{
	HWND control = GetDlgItem(dialog, id);
	RECT removed;
	GetWindowRect(control, &removed);
	MapWindowPoints(nullptr, dialog, LPPOINT(&removed), 2);
	RECT line = { 0, 0, 0, 11 };
	MapDialogRect(dialog, &line);
	ShowWindow(control, SW_HIDE);
	for (HWND child = GetWindow(dialog, GW_CHILD); child; child = GetWindow(child, GW_HWNDNEXT)) {
		RECT rect;
		GetWindowRect(child, &rect);
		MapWindowPoints(nullptr, dialog, LPPOINT(&rect), 2);
		if (rect.top > removed.top)
			SetWindowPos(child, nullptr, rect.left, rect.top - line.bottom, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
	}
	RECT window;
	GetWindowRect(dialog, &window);
	SetWindowPos(dialog, nullptr, 0, 0, window.right - window.left, window.bottom - window.top - line.bottom,
		SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
}

bool sameChecked(HWND dialog)
{
	HWND box = GetDlgItem(dialog, IDC_BATCH_SAME);
	return box && IsWindowVisible(box) && IsDlgButtonChecked(dialog, IDC_BATCH_SAME) == BST_CHECKED;
}

// OK needs PINs of the minimal length, the second one only without the box that they are the same
void updateOk(HWND dialog, const State &state)
{
	int minLength = int(state.request->minPinLen > 0 ? state.request->minPinLen : 4);
	int length = fieldLength(dialog, IDC_BATCH_PIN);
	if (state.twoFields && !sameChecked(dialog))
		length = min(length, fieldLength(dialog, IDC_BATCH_PIN2));
	EnableWindow(GetDlgItem(dialog, IDOK), length >= minLength);
}

void applySame(HWND dialog)
{
	bool same = sameChecked(dialog);
	EnableWindow(GetDlgItem(dialog, IDC_BATCH_LABEL2), !same);
	EnableWindow(GetDlgItem(dialog, IDC_BATCH_PIN2), !same);
}

void roundedRect(Gdiplus::GraphicsPath &path, float x, float y, float width, float height, float radius)
{
	float d = radius * 2;
	path.AddArc(x, y, d, d, 180, 90);
	path.AddArc(x + width - d, y, d, d, 270, 90);
	path.AddArc(x + width - d, y + height - d, d, d, 0, 90);
	path.AddArc(x, y + height - d, d, d, 90, 90);
	path.CloseFigure();
}

// OK is the main action in the accent colour, Cancel is white with a border
void drawButton(const DRAWITEMSTRUCT &item, const State &state)
{
	RECT rect = item.rcItem;
	FillRect(item.hDC, &rect, state.footer);
	bool primary = item.CtlID == IDOK, disabled = (item.itemState & ODS_DISABLED) != 0,
		pressed = (item.itemState & ODS_SELECTED) != 0;
	Gdiplus::Color fill = primary ? (disabled ? DISABLED : pressed ? PRIMARY_PRESSED : PRIMARY) : (pressed ? BUTTON_PRESSED : BUTTON);
	Gdiplus::Color border = primary ? fill : BUTTON_BORDER;
	{
		Gdiplus::Graphics graphics(item.hDC);
		graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
		Gdiplus::GraphicsPath path;
		float radius = float(rect.bottom - rect.top) / 7;
		roundedRect(path, rect.left + 0.5f, rect.top + 0.5f, float(rect.right - rect.left - 1), float(rect.bottom - rect.top - 1), radius);
		Gdiplus::SolidBrush brush(fill);
		Gdiplus::Pen pen(border);
		graphics.FillPath(&brush, &path);
		graphics.DrawPath(&pen, &path);
	}
	wchar_t text[64];
	GetWindowTextW(item.hwndItem, text, 64);
	Gdiplus::Color textColor = primary ? (disabled ? DISABLED_TEXT : Gdiplus::Color(255, 255, 255)) : Gdiplus::Color(26, 26, 26);
	SetBkMode(item.hDC, TRANSPARENT);
	SetTextColor(item.hDC, textColor.ToCOLORREF());
	DrawTextW(item.hDC, text, -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
	if ((item.itemState & ODS_FOCUS) && !(item.itemState & ODS_NOFOCUSRECT)) {
		InflateRect(&rect, -3, -3);
		DrawFocusRect(item.hDC, &rect);
	}
}

void drawLogo(const DRAWITEMSTRUCT &item, const State &state)
{
	RECT rect = item.rcItem;
	FillRect(item.hDC, &rect, state.body);
	if (!state.logo)
		return;
	float height = float(rect.bottom - rect.top);
	float width = height * state.logo->GetWidth() / state.logo->GetHeight();
	Gdiplus::Graphics graphics(item.hDC);
	graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
	graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);
	graphics.DrawImage(state.logo.get(), Gdiplus::RectF(rect.right - width, float(rect.top), width, height));
}

INT_PTR CALLBACK dialogProc(HWND dialog, UINT message, WPARAM wParam, LPARAM lParam)
{
	State *state = reinterpret_cast<State*>(GetWindowLongPtr(dialog, GWLP_USERDATA));
	switch (message)
	{
	case WM_INITDIALOG:
	{
		state = reinterpret_cast<State*>(lParam);
		SetWindowLongPtr(dialog, GWLP_USERDATA, lParam);
		const BatchPinRequest &request = *state->request;
		SetWindowTextW(dialog, request.title.c_str());
		SetDlgItemTextW(dialog, IDC_BATCH_HEADING, request.heading.c_str());
		SetDlgItemTextW(dialog, IDC_BATCH_NAME, request.name.c_str());
		SetDlgItemTextW(dialog, IDC_BATCH_DETAILS, request.details.c_str());
		SetDlgItemTextW(dialog, IDC_BATCH_ERROR, request.error.c_str());
		SetDlgItemTextW(dialog, IDC_BATCH_LABEL, request.label.c_str());
		SetDlgItemTextW(dialog, IDC_BATCH_LABEL2, request.label2.c_str());
		SetDlgItemTextW(dialog, IDCANCEL, Labels::l10n.get("cancel").c_str());

		if ((state->headingFont = derivedFont(dialog, 3, 2, FW_SEMIBOLD)))
			SendDlgItemMessage(dialog, IDC_BATCH_HEADING, WM_SETFONT, WPARAM(state->headingFont), FALSE);
		if ((state->boldFont = derivedFont(dialog, 1, 1, FW_SEMIBOLD))) {
			SendDlgItemMessage(dialog, IDC_BATCH_ERROR, WM_SETFONT, WPARAM(state->boldFont), FALSE);
			SendDlgItemMessage(dialog, IDC_BATCH_LABEL2, WM_SETFONT, WPARAM(state->boldFont), FALSE);
		}
		state->logo = loadLogo();
		if (request.error.empty())
			collapse(dialog, IDC_BATCH_ERROR);
		if (request.details.empty())
			collapse(dialog, IDC_BATCH_DETAILS);
		if (GetDlgItem(dialog, IDC_BATCH_SAME)) {
			if (request.sameLabel.empty()) {
				collapse(dialog, IDC_BATCH_SAME);
			} else {
				SetDlgItemTextW(dialog, IDC_BATCH_SAME, request.sameLabel.c_str());
				CheckDlgButton(dialog, IDC_BATCH_SAME, request.same ? BST_CHECKED : BST_UNCHECKED);
				applySame(dialog);
			}
		}

		SetFocus(GetDlgItem(dialog, IDC_BATCH_PIN));
		return FALSE;
	}
	case WM_CTLCOLORDLG:
		return INT_PTR(state->body);
	case WM_CTLCOLORSTATIC:
	{
		HDC dc = HDC(wParam);
		switch (GetDlgCtrlID(HWND(lParam)))
		{
		case IDC_BATCH_ERROR: SetTextColor(dc, ERROR_COLOR); break;
		case IDC_BATCH_NAME:
		case IDC_BATCH_DETAILS: SetTextColor(dc, SECONDARY_COLOR); break;
		default: SetTextColor(dc, TEXT_COLOR); break;
		}
		SetBkColor(dc, BODY_COLOR);
		return INT_PTR(state->body);
	}
	case WM_DRAWITEM:
	{
		const DRAWITEMSTRUCT &item = *reinterpret_cast<const DRAWITEMSTRUCT*>(lParam);
		if (item.CtlID == IDC_BATCH_ICON)
			drawLogo(item, *state);
		else
			drawButton(item, *state);
		return TRUE;
	}
	case WM_PAINT:
	{
		// Footer with the buttons below a separator line
		PAINTSTRUCT paint;
		HDC dc = BeginPaint(dialog, &paint);
		RECT client, button, padding = { 0, 0, 0, 9 };
		GetClientRect(dialog, &client);
		GetWindowRect(GetDlgItem(dialog, IDOK), &button);
		MapWindowPoints(nullptr, dialog, LPPOINT(&button), 2);
		MapDialogRect(dialog, &padding);
		RECT footer = client;
		footer.top = button.top - padding.bottom;
		FillRect(dc, &footer, state->footer);
		RECT line = footer;
		line.bottom = line.top + 1;
		HBRUSH separator = CreateSolidBrush(SEPARATOR_COLOR);
		FillRect(dc, &line, separator);
		DeleteObject(separator);
		EndPaint(dialog, &paint);
		return TRUE;
	}
	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDC_BATCH_PIN:
		case IDC_BATCH_PIN2:
			if (HIWORD(wParam) == EN_CHANGE)
				updateOk(dialog, *state);
			break;
		case IDC_BATCH_SAME:
			if (HIWORD(wParam) == BN_CLICKED)
			{
				applySame(dialog);
				updateOk(dialog, *state);
			}
			break;
		case IDOK:
			// Enter comes here also while the PIN is too short
			if (!IsWindowEnabled(GetDlgItem(dialog, IDOK)))
				return TRUE;
			state->same = sameChecked(dialog);
			state->pin = readUtf8(dialog, IDC_BATCH_PIN);
			if (state->twoFields && !state->same)
				state->pin2 = readUtf8(dialog, IDC_BATCH_PIN2);
			EndDialog(dialog, IDOK);
			return TRUE;
		case IDCANCEL:
			EndDialog(dialog, IDCANCEL);
			return TRUE;
		}
		return FALSE;
	case WM_DESTROY:
		if (state->headingFont)
			DeleteObject(state->headingFont);
		if (state->boldFont)
			DeleteObject(state->boldFont);
		DeleteObject(state->body);
		DeleteObject(state->footer);
		return FALSE;
	}
	return FALSE;
}

}

std::string BatchPinDialog::show(const BatchPinRequest &request, std::string *signaturePin, bool *same)
{
	Gdiplus::GdiplusStartupInput gdiplusInput;
	ULONG_PTR gdiplus = 0;
	Gdiplus::GdiplusStartup(&gdiplus, &gdiplusInput, nullptr);
	INT_PTR result;
	bool sameBox = false;
	std::string pin, pin2;
	{
		// The logo bitmap must be gone before GDI+ shuts down
		State state;
		state.request = &request;
		state.twoFields = signaturePin != nullptr;

		SetThreadDpiAwarenessContextFn setDpiAwareness = SetThreadDpiAwarenessContextFn(
			GetProcAddress(GetModuleHandleW(L"user32.dll"), "SetThreadDpiAwarenessContext"));
		HANDLE previous = setDpiAwareness ? setDpiAwareness(PER_MONITOR_AWARE_V2) : nullptr;
		int dialog = state.twoFields ? IDD_BATCH_PIN2 : request.signaturePin ? IDD_BATCH_SIGNPIN : IDD_BATCH_PIN;
		result = DialogBoxParamW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(dialog), nullptr, dialogProc, LPARAM(&state));
		if (setDpiAwareness && previous)
			setDpiAwareness(previous);
		pin.swap(state.pin);
		pin2.swap(state.pin2);
		sameBox = state.same;
	}
	Gdiplus::GdiplusShutdown(gdiplus);

	if (result != IDOK) {
		SecureZeroMemory(&pin[0], pin.size());
		SecureZeroMemory(&pin2[0], pin2.size());
		return std::string();
	}
	if (same)
		*same = sameBox;
	if (signaturePin)
		*signaturePin = pin2;
	if (!pin2.empty())
		SecureZeroMemory(&pin2[0], pin2.size());
	return pin;
}
