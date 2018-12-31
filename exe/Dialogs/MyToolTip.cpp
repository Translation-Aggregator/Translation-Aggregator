#include <Shared/Shrink.h>
#include "MyToolTip.h"
#include <Shared/StringUtil.h>
#include "../Config.h"

#define MY_TOOLTIP_CLASS L"Custom ToolTip Class"

wchar_t *tags[] =
{
	L"adj-i",
	L"adj-na",
	L"adj-no",
	L"adj-pn",
	L"adj-t",
	L"adj-f",
	L"adj",
	L"adv",
	L"adv-n",
	L"adv-to",
	L"aux",
	L"aux-v",
	L"aux-adj",

	L"conj",
	L"ctr",
	L"exp",
	L"id",
	L"int",

	L"iv",
	L"n",
	L"n-adv",
	L"n-pref",
	L"n-suf",
	L"n-t",

	L"num",
	L"pn",
	L"pref",
	L"prt",
	L"suf",

	L"v1",
	L"v5",
	L"v5aru",
	L"v5b",
	L"v5g",
	L"v5k",
	L"v5k-s",
	L"v5m",
	L"v5n",
	L"v5r",
	L"v5r-i",
	L"v5s",
	L"v5t",
	L"v5u",
	L"v5u-s",
	L"v5uru",
	L"v5z",
	L"vz",
	L"vi",
	L"vk",
	L"vn",
	L"vs",
	L"vs-i",
	L"vs-s",
	L"vt"
};

static HWND hWndTip = 0;
static HFONT hFont = 0;
static wchar_t *ToolTipText = 0;

wchar_t *FindCloseBrace(wchar_t *s)
{
	int pos = 1;
	int depth = 1;
	while (s[pos] && s[pos] != '\n')
	{
		if (s[pos] == '(')
			depth++;
		else if (s[pos] == ')')
		{
			depth --;
			if (!depth) break;
		}
		s++;
	}
	return s+pos;
}

inline int IsWideAscii(wchar_t c)
{
	if (c >= 0xFF10 && c <= 0xFF19) return 1;
	else if (c >= 0xFF21 && c <= 0xFF21+25) return 1;
	return 0;
}

inline int IsJapPlus(wchar_t c)
{
	return IsJap(c) || IsWideAscii(c);
}

int MyDrawText(HDC hDC, wchar_t *text, RECT *rect, unsigned int format)
{
	TEXTMETRIC tm;
	wchar_t *start = text;
	GetTextMetrics(hDC, &tm);
	int height = tm.tmHeight + tm.tmExternalLeading;
	int bottom = rect->top;
	int right = rect->left;
	int reallyDraw = !(format & DT_CALCRECT);
	DWORD defaultTextColor;
	if (reallyDraw)
	{
		// SetBkColor(hDC, GetSysColor(COLOR_INFOBK));
		//defaultTextColor = GetSysColor(COLOR_INFOTEXT);
		defaultTextColor = config.toolTipFont.color;
		SetBkMode(hDC, TRANSPARENT);
	}
	RECT rect2;
	int maxWidth = rect->right - rect->left;
	while (*text)
	{
		int len = wcscspn(text, L"\n");
		rect2 = *rect;
		rect2.top = bottom;
		if (text[0] == 1 || !len)
		{
			text++;
			len--;
			DrawText(hDC, text, len, &rect2, format | DT_CALCRECT | DT_WORDBREAK);
			if (reallyDraw)
			{
				SetTextColor(hDC, config.toolTipConj);
				DrawText(hDC, text, len, &rect2, format | DT_WORDBREAK);
			}
			bottom = rect2.bottom;
			if (right < rect2.right) right = rect2.right;
		}
		else
		{
			if (reallyDraw)
				SetTextColor(hDC, defaultTextColor);
			wchar_t *wordStart = text;
			int wordLen = 0;
			int xOffset = 0;
			wchar_t *closeBracePos = 0;
			while (len > 0)
			{
				len--;
				text++;
				wordLen++;

				DrawText(hDC, wordStart, text-wordStart, &rect2, format | DT_CALCRECT);
				int width = rect2.right - rect2.left;
				int wordBreak = 0;

				if (text != start && text[-1] == '\r')
				{
					wordStart = text;
					wordLen = 0;
					// Indent.
					xOffset = 10;
					rect2.bottom = bottom += height;
					rect2.left = rect->left;
					continue;
				}
				if (xOffset + width > maxWidth)
				{
					if (xOffset < 50)
						wordBreak = 1;
					else
					{
						// Indent.
						xOffset = 10;
						rect2.bottom = bottom += height;
						rect2.left = rect->left;
						while (wordStart < text && iswspace(wordStart[0]))
						{
							wordStart++;
							wordLen--;
							continue;
						}
					}
				}

				if (!text[0] || iswspace(text[0]))
					wordBreak = 1;
				else if (text != start && ((text[-1] == '-' && text[0] != '-') || text[-1] == '/' || text[-1] == ',' || text[-1] == ';' || (text[-1] == ')' && text[0] != ')') || (IsJapPlus(text[0]) != IsJapPlus(text[-1]) && text[0] != ')' && text[0] != 0x3010)))
					wordBreak = 1;
				else if (text[0] == '(' || text[0] == 0x3010)
					wordBreak = 1;
				if (wordBreak)
				{
					rect2.left = xOffset + rect->left;
					rect2.top = bottom;
					if (wordStart[0] == '(' && !closeBracePos)
						closeBracePos = FindCloseBrace(wordStart);
					if (reallyDraw)
					{
						if (closeBracePos)
							SetTextColor(hDC, config.toolTipParen);
						//else if (closeBraces == 0x3011 && wordStart[0] != ';' && wordStart[0] != 0x3010 && wordStart[0] != 0x3011) {
						//  SetTextColor(hDC, config.toolTipHiragana);
						//}
						else if (IsJapPlus(wordStart[0]))
						{
							unsigned int color = config.toolTipHiragana;
							for (wchar_t *s = wordStart; s < text; s++)
							{
								if (!IsHiragana(*s) && !IsKatakana(*s))
								{
									color = config.toolTipKanji;
									break;
								}
							}
							SetTextColor(hDC, color);
						}
						else
							SetTextColor(hDC, defaultTextColor);
						DrawText(hDC, wordStart, text-wordStart, &rect2, format);
					}
					xOffset += width;
					if (right < xOffset + rect->left)
						right = xOffset + rect->left;
					if (closeBracePos < text)
						closeBracePos = 0;
					wordStart = text;
				}
			}
			DrawText(hDC, text, len, &rect2, format | DT_CALCRECT);
			if (reallyDraw)
			{
				SetTextColor(hDC, defaultTextColor);
				DrawText(hDC, text, len, &rect2, format);
			}
			bottom += height;
		}
		text += len;
		if (*text) text++;
	}
	rect->right = right;
	rect->bottom = bottom;
	return 1;
}

LRESULT CALLBACK ToolTipWindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (uMsg == WM_DESTROY)
	{
		free(ToolTipText);
		ToolTipText = 0;
	}
	static int width;
	if (uMsg == WM_CREATE)
	{
		width = 400;
		HDC hDC = GetDC(hWnd);
		if (hDC && ToolTipText)
		{
			HFONT hOldFont = 0;
			if (hFont) SelectObject(hDC, hFont);
			RECT rAvoid = *(RECT*)((CREATESTRUCT*)lParam)->lpCreateParams;
			POINT p, pout;
			GetCursorPos(&p);
			pout.x = p.x;
			pout.y = p.y+24;
			if (pout.y >= rAvoid.bottom)
				rAvoid.bottom = pout.y;
			else
				pout.y = rAvoid.bottom;
			RECT rSize;
			rSize.top = 4;
			rSize.left = 4;
			rSize.right = 404;
			if (MyDrawText(hDC, ToolTipText, &rSize, DT_CALCRECT | DT_NOPREFIX | DT_NOCLIP))
			{
				if (rSize.bottom >= 120)
				{
					width = 450;
					if (rSize.bottom >= 200)
					{
						width = 500;
						if (rSize.bottom >= 400)
						{
							width = 550;
							if (rSize.bottom >= 500)
								width = 600;
						}
					}
					rSize.right = width+4;
					MyDrawText(hDC, ToolTipText, &rSize, DT_CALCRECT | DT_NOPREFIX | DT_NOCLIP);
				}
				rSize.left = rSize.top = 0;
				AdjustWindowRectEx(&rSize, WS_POPUP | WS_BORDER, 0, WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE | WS_EX_TOPMOST);
				rSize.right -= rSize.left;
				rSize.bottom -= rSize.top;
				rSize.left = rSize.top = 0;
				MONITORINFO info;
				memset(&info, 0, sizeof(info));
				info.cbSize = sizeof(info);
				HMONITOR hMonitor;
				if ((hMonitor = MonitorFromPoint(p, MONITOR_DEFAULTTOPRIMARY)) && GetMonitorInfo(hMonitor, &info))
				{
					if (pout.y + rSize.bottom > info.rcWork.bottom)
					{
						if (rSize.bottom <= rAvoid.top-2)
							pout.y = rAvoid.top-2-rSize.bottom;
						else
						{
							pout.y = info.rcWork.bottom - rSize.bottom;
							if (info.rcWork.right >= rSize.right + rAvoid.right+2)
								pout.x = rAvoid.right+2;
							else if (rSize.right <= rAvoid.left-2)
								pout.x = rAvoid.left-2-rSize.right;
						}
					}
					if (pout.x+rSize.right > info.rcWork.right)
						pout.x = info.rcWork.right - rSize.right;
				}
				SetWindowPos(hWnd, HWND_TOP, pout.x, pout.y, rSize.right, rSize.bottom, SWP_NOACTIVATE|SWP_HIDEWINDOW);
			}
			if (hOldFont) SelectObject(hDC, hOldFont);
			ReleaseDC(hWnd, hDC);
		}
	}
	if (uMsg == WM_PAINT)
	{
		PAINTSTRUCT psPaint;
		HDC hDC = BeginPaint(hWnd, &psPaint);
		if (hDC && ToolTipText)
		{
			HFONT hOldFont = 0;
			if (hFont) SelectObject(hDC, hFont);
			RECT rSize;
			rSize.left = 2;
			rSize.top = 2;
			rSize.right = width + 2;
			MyDrawText(hDC, ToolTipText, &rSize, DT_NOPREFIX | DT_NOCLIP);
			if (hOldFont) SelectObject(hDC, hOldFont);
		}
		EndPaint(hWnd, &psPaint);
	}
	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

void ShowToolTip(wchar_t *text, HWND hWnd, RECT *r)
{
	static char registeredTipClass = 0;
	if (hWndTip)
	{
		DestroyWindow(hWndTip);
		hWndTip = 0;
	}

	if (hFont)
	{
		DeleteObject(hFont);
		hFont = 0;
	}

	// Probably no real need to do this...
	if (!text || !r)
	{
		if (registeredTipClass)
		{
			UnregisterClass(MY_TOOLTIP_CLASS, ghInst);
			registeredTipClass = 0;
		}
		return;
	}

	{
		// Handle reformatting stuff, other than colors.
		int out = 0;
		int in = 0;
		int lastSlash = -1;
		while (text[in])
		{
			if (text[in] == '(')
			{
				int end = FindCloseBrace(&text[in]) - text;
				// Include close parens.
				if (text[end]) end++;
				// Include trailing space, if there is one.
				if (text[end] == ' ') end++;
				int hide = 0;
				if (config.jParserHideCrossRefs && !wcsnicmp(text+in+1, L"See ", 4))
					hide = 1;
				else if (config.jParserHideUsage)
					if (!wcsnicmp(text+in, L"(P)", 3))
					{
						hide = 1;
						if (in && text[in-1] == '/' && (text[in+3] == '\n' || !text[in+3]))
							out--;
					}
				if (!hide && config.jParserHidePos)
					for (int i=0; i<sizeof(tags)/sizeof(tags[0]); i++)
					{
						int len = wcslen(tags[i]);
						if (!wcsnicmp(text+in+1, tags[i], len) && (text[in+1+len]==')' || text[in+1+len]==','))
						{
							hide = 1;
							break;
						}
					}
				if (!hide && text[in+1] != ')')
				{
					int p = in+1;
					while (text[p] >= '0' && text[p] <= '9') p++;
					if (text[p] == ')')
					{
						if (config.jParserDefinitionLines)
						{
							if (lastSlash >= 0)
							{
								int ls = lastSlash;
								text[lastSlash] = '\r';
								lastSlash = -1;
							}
						}
						if (config.jParserReformatNumbers)
						{
							in++;
							if (out && (text[out-1] == '/' || text[out-1] == ' ')) text[out-1] = 0x3000;
							while (text[in] >= '0' && text[in] <= '9')
								text[out++] = text[in++];
							text[out++] = '.';
							in++;
							text[out++] = ' ';
							in++;
							continue;
						}
					}
				}
				if (!hide)
				{
					memmove(text+out, text+in, (end-in)*sizeof(wchar_t));
					out += end - in;
				}
				in = end;
			}
			else
			{
				if (text[in] == '/' || text[in] == '\r') lastSlash = out;
				if (text[in] == '\n') lastSlash = -1;
				if (config.jParserNoKanaBrackets && (text[in] == 0x3010 || text[in] == 0x3011))
				{
					if (out && text[out-1] == ' ') out--;
					in++;
					if (text[in] == '\r')
						continue;
					text[out++] = 0x3000;
					continue;
				}
				text[out++] = text[in++];
			}
		}
		text[out] = 0;
	}

	ToolTipText = text;

	if (!registeredTipClass)
	{
		const WNDCLASS ToolTipWindowClass =
		{
			0,
			ToolTipWindowProc,
			0, 0,
			ghInst,
			0,
			LoadCursor(NULL, IDC_ARROW),
			GetSysColorBrush(COLOR_INFOBK),
			0,
			MY_TOOLTIP_CLASS
		};
		if (!RegisterClass(&ToolTipWindowClass)) return;
		registeredTipClass = 1;
	}
	//NONCLIENTMETRICS ncm;
	//memset(&ncm, 0, sizeof(ncm));
	//ncm.cbSize = sizeof(NONCLIENTMETRICS);
	//if (SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(NONCLIENTMETRICS), &ncm, 0)) {
	//  hFont = CreateFontIndirect(&ncm.lfStatusFont);
	//}
	LOGFONT lf;
	InitLogFont(&lf, &config.toolTipFont);
	hFont = CreateFontIndirect(&lf);
	hWndTip = CreateWindowEx(WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE | WS_EX_TOPMOST, MY_TOOLTIP_CLASS, 0,
		WS_POPUP | WS_BORDER,
		CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, hWnd, 0, ghInst, r);
	if (hWndTip) ShowWindow(hWndTip, SW_SHOWNOACTIVATE);
	return;
}
