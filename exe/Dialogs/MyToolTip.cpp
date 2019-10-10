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
#ifdef SETSUMI_CHANGES
//hack - tooltip global variables
#include <math.h>
#define SCROLLER_W 5
#define SCROLLER_STEP 4
#define SCROLLER_TIMER 666
#define hIsScrollable() (ActualH < MemBmpSize.cy)
static HDC hMemDC = 0;
static HBITMAP hMemBmp = 0;
static SIZE MemBmpSize={0,0}; // full text area size
static int ActualH = 0; // visible text area height
static int PosY = 0; // "copy from" position on memory surface
static HHOOK hMouseHook = 0;
static int LineHeight = 0;
static bool MouseInside = false;
static POINT MousePos={0,0};
static bool KeepToolTip = false;
bool hIsKeepToolTip() { return KeepToolTip; }
static int TimerStage = 0;
//hackend
#endif

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
#ifdef SETSUMI_CHANGES
	LineHeight = height; //hack - store tooltip line height
#endif
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

#ifdef SETSUMI_CHANGES
//hack - tooltip functions
void hDrawScroller(HDC hdc) {
	RECT r;
	GetClientRect(hWndTip, &r);
	r.right -= r.left;
	r.bottom -= r.top;
	r.left = r.top = 0;
	float ratio = (float)(ActualH + 4/*include top and bottom borders*/) / (float)MemBmpSize.cy;
	int pos = (int)((float)PosY * ratio);
	int len = (int)((float)(r.bottom + 1) * ratio);
	SelectObject(hdc, GetStockObject(DC_BRUSH));
	SetDCBrushColor(hdc, GetSysColor(COLOR_INFOBK));
	PatBlt(hdc, r.right - SCROLLER_W, 0, SCROLLER_W, r.bottom, PATCOPY);
	SetDCBrushColor(hdc, RGB(0, 128, 255));
	PatBlt(hdc, r.right - SCROLLER_W, pos, SCROLLER_W, len, PATCOPY);
}
void hScroll(int delta) {
	PosY -= delta * LineHeight;
	if (PosY > MemBmpSize.cy - ActualH)
		PosY = MemBmpSize.cy - ActualH;
	if (PosY < 0)
		PosY = 0;
	RECT client;
	GetClientRect(hWndTip, &client);
	InvalidateRect(hWndTip, &client, FALSE);
	UpdateWindow(hWndTip);
}
bool hIsMouseInsideToolTip(bool CareVisible) {
	RECT wndrect;
	GetWindowRect(hWndTip, &wndrect);
	POINT mouse;
	GetCursorPos(&mouse);
	bool inside = (mouse.x >= wndrect.left && mouse.x <= wndrect.right && mouse.y >= wndrect.top && mouse.y <= wndrect.bottom);
	if (CareVisible) {
		inside = IsWindowVisible(hWndTip) && inside;
	}
	return inside;
}
float hDistance(int x1, int y1, int x2, int y2) {
	return sqrtf(pow(((float)x2 - (float)x1), 2) + pow(((float)y2 - (float)y1), 2));
}
void hHideToolTip() {
	ShowToolTip(0, 0, 0);
	KeepToolTip = false;
}
LRESULT CALLBACK hHookMouseProc(int nCode,	WPARAM wParam, LPARAM lParam) {
	if(!(nCode < 0)) {
		MSLLHOOKSTRUCT *hs = (MSLLHOOKSTRUCT*)lParam;
		RECT wr;
		GetWindowRect(hWndTip, &wr);
		if (wParam == WM_MOUSEWHEEL) {
			if (hIsScrollable()) {
				float delta = ((float)((signed short)HIWORD(hs->mouseData))) / WHEEL_DELTA;
				if (fabs(delta) < 1.0f) { // Windows7 tiny delta fix
					delta = delta<0 ? -1.0f : 1.0f;
				}
				hScroll((int)delta * SCROLLER_STEP);
				return -1; // eat message
			}
		}
		if (wParam == WM_MOUSEMOVE) {
			bool inside = hIsMouseInsideToolTip();
			if (MouseInside && !inside) { // close tooltip on mouse leave
				hHideToolTip();
			} else if (hIsScrollable() && !inside) {
				if (hs->pt.x >= wr.left && hs->pt.x <= wr.right) { // mouse above or below
					if (hs->pt.y < wr.top) { // above
						KeepToolTip = (hs->pt.y > MousePos.y);
					} else { // below
						KeepToolTip = (hs->pt.y < MousePos.y);
					}
				} else if (hs->pt.y >= wr.top && hs->pt.y <= wr.bottom) { // mouse to the left or right
					if (hs->pt.x < wr.left) { // left
						KeepToolTip = (hs->pt.x > MousePos.x);
					} else { // right
						KeepToolTip = (hs->pt.x < MousePos.x);
					}
				} else { // mouse placed diagonally
					POINT center;
					center.x = wr.left + (wr.right-wr.left)/2;
					center.y = wr.top + (wr.bottom-wr.top)/2;
					KeepToolTip = (hDistance(hs->pt.x, hs->pt.y, center.x, center.y) < hDistance(MousePos.x, MousePos.y, center.x, center.y));
				}
			}
			MouseInside = inside;
			MousePos = hs->pt;
		}
		if (wParam == WM_LBUTTONDOWN) {
			if (hIsScrollable() && hIsMouseInsideToolTip()) {
				if (hs->pt.y > wr.top + (wr.bottom-wr.top)/2) { // scroll down
					TimerStage = -1;
				} else { // scroll up
					TimerStage = 1;
				}
				hScroll(TimerStage * SCROLLER_STEP);
				SetTimer(hWndTip, SCROLLER_TIMER, 400, NULL);
				return -1; // eat message
			}
		}
		if (wParam == WM_LBUTTONUP) {
			if (TimerStage) KillTimer(hWndTip, SCROLLER_TIMER);
			TimerStage = 0;
		}
		if (wParam == WM_RBUTTONDOWN) {
			if (hIsMouseInsideToolTip()) {
				return -1; // eat message
			}
		}
		if (wParam == WM_RBUTTONUP) {
			if (hIsMouseInsideToolTip()) {
				hHideToolTip();
				return -1; // eat message
			}
		}
	}
	return CallNextHookEx(hMouseHook, nCode, wParam, lParam);
}
//hackend
#endif

LRESULT CALLBACK ToolTipWindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
#ifdef SETSUMI_CHANGES
	//hack - tooltip scroll timer
	if (uMsg == WM_TIMER) {
		if (abs(TimerStage) == 1) { // first scroll with slight pause
			KillTimer(hWndTip, SCROLLER_TIMER);
			if (TimerStage > 0) {
				TimerStage = 2;
			}
			if (TimerStage < 0) {
				TimerStage = -2;
			}
			SetTimer(hWndTip, SCROLLER_TIMER, 10, NULL);
		} else if (abs(TimerStage) == 2) { // rest of scrolls fast
			hScroll(TimerStage>0? 1:-1);
		} else { // paranoid protection
			KillTimer(hWndTip, SCROLLER_TIMER);
			TimerStage = 0;
		}
	}
	//hackend
#endif
	if (uMsg == WM_DESTROY)
	{
		free(ToolTipText);
		ToolTipText = 0;
#ifdef SETSUMI_CHANGES
		//hack - tooltip cleanup
		if (hMemBmp) DeleteObject(hMemBmp);
		hMemBmp = 0;
		if (hMemDC) DeleteDC(hMemDC);
		hMemDC = 0;
		if (hMouseHook) UnhookWindowsHookEx(hMouseHook);
		hMouseHook = 0;
		if (TimerStage) KillTimer(hWndTip, SCROLLER_TIMER);
		TimerStage = 0;
		//hackend
#endif
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
#ifdef SETSUMI_CHANGES
				//hack - tooltip preliminary memory surface size
				RECT recmem = rSize;
				recmem.right -= recmem.left;
				recmem.bottom -= recmem.top;
				recmem.left = recmem.top = 0;
				//hackend
#endif
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
#ifdef SETSUMI_CHANGES
					//hack - limit tooltip height and draw fullsize tooltip to memory surface
					int maxH = info.rcWork.bottom - info.rcWork.top - 5;
					if (rSize.bottom > maxH) {
						rSize.bottom = maxH;
						recmem.right -= SCROLLER_W + 2;
						MyDrawText(hDC, ToolTipText, &recmem, DT_CALCRECT | DT_NOPREFIX | DT_NOCLIP);
					}
					ActualH = rSize.bottom - 5; // lazily adjusted this :)
					MemBmpSize.cx = recmem.right + 1;
					MemBmpSize.cy = recmem.bottom + 1;
					if (hMemBmp) DeleteObject(hMemBmp);
					hMemBmp = CreateCompatibleBitmap(hDC, MemBmpSize.cx, MemBmpSize.cy);
					if (hMemDC) DeleteDC(hMemDC);
					hMemDC = CreateCompatibleDC(hDC);
					SelectObject(hMemDC, hMemBmp);
					SelectObject(hMemDC, GetStockObject(DC_BRUSH));
					SetDCBrushColor(hMemDC, GetSysColor(COLOR_INFOBK));
					PatBlt(hMemDC, 0, 0, MemBmpSize.cx, MemBmpSize.cy, PATCOPY);
					if (hFont) SelectObject(hMemDC, hFont);
					MyDrawText(hMemDC, ToolTipText, &recmem, DT_NOPREFIX | DT_NOCLIP);
					//hackend
#endif
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
#ifdef SETSUMI_CHANGES
			//hack - tooltip copy from memory surface
			BitBlt(hDC, 2, 2, MemBmpSize.cx, ActualH, hMemDC, 0, PosY, SRCCOPY);
			if (hIsScrollable()) hDrawScroller(hDC);
#else
			HFONT hOldFont = 0;
			if (hFont) SelectObject(hDC, hFont);
			RECT rSize;
			rSize.left = 2;
			rSize.top = 2;
			rSize.right = width + 2;
			MyDrawText(hDC, ToolTipText, &rSize, DT_NOPREFIX | DT_NOCLIP);
			if (hOldFont) SelectObject(hDC, hOldFont);
			//hackend
#endif
		}
		EndPaint(hWnd, &psPaint);
	}
	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

#ifdef SETSUMI_CHANGES
//hack - format definition clones (1)
bool hIsConj(wchar_t *line)
{
	if (wcslen(line) && !IsJap(line[0])) return true;
	return false;
}
void hDeleteLine(int lineIndex, wchar_t **lines, int &linesCnt)
{
	linesCnt--;
	for (int i=lineIndex; i<linesCnt; i++) {
		lines[i] = lines[i+1]; // shift up
	}
}

int hGetLines(wchar_t *text, wchar_t **&lines, wchar_t *&linesText)
{
	linesText = new wchar_t[wcslen(text)+1];
	wcscpy(linesText,text);

	wchar_t *buffer=linesText;
	wchar_t **larr = new wchar_t*[500];
	int lineCnt=0;
	wchar_t *lb=buffer;
	while (wchar_t *lnbr = wcsstr(buffer, L"\n")) {
		larr[lineCnt++] = lb;
		*lnbr = 0;
		buffer = lnbr + 1;
		lb = buffer;
		if (!wcslen(buffer)) break;
	}
	larr[lineCnt++] = lb;
	lines = new wchar_t*[lineCnt];
	for (int i=0; i<lineCnt; i++) lines[i] = larr[i];
	delete []larr;
	return lineCnt;
}

wchar_t *hGetDefinition(wchar_t *line)
{
	wchar_t *rv=0;
	int len = wcslen(line);
	if (len && IsJap(line[0])) {
		for (int i=0; i<len; i++) {
			if (line[i]!=L';' && line[i]!=L' ' && line[i]!=0x3000 && line[i]!=0x3010 && line[i]!=0x3011 && !IsJap(line[i])) {
				rv = line+i;
				break;
			}
		}
	}
	return rv;
}

void hMoveLine(wchar_t **&lines, int count, int indFrom, int indTo)
{
	if (indFrom == indTo) return;
	wchar_t *line = lines[indFrom];
	bool scope=false;
	for (int i=count-1; i>=indTo; i--) {
		if (i==indFrom) {
			scope = true;
			continue;
		}
		if (scope) lines[i+1] = lines[i]; // shift down
	}
	lines[indTo] = line;
}
//hackend
#endif

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

#ifdef SETSUMI_CHANGES
	//hack - format definition clones (2)
	BYTE ks[256]; GetKeyboardState(ks);
	// disable this feature on incompatible option(s) or if Shift key is pressed (debug)
	if (!((config.jParserFlags & JPARSER_JAPANESE_OWN_LINE) || config.jParserDefinitionLines) && (ks[VK_SHIFT] == 0 || ks[VK_SHIFT] == 1))
	{
		// format lines
		wchar_t **lines=0;
		wchar_t *linesText=0;
		int linesCnt = hGetLines(text, lines, linesText);
		for (int i=0; i<linesCnt; i++) {
			wchar_t *def=hGetDefinition(lines[i]);
			if (def) {
				int to = i+1;
				for (int j=i+1; j<linesCnt; j++) {
					wchar_t *d = hGetDefinition(lines[j]);
					if (d && wcscmp(def,d)==0) {
						if (ks[VK_MENU] == 0 || ks[VK_MENU] == 1) { // do not delete conjugations if Alt is pressed (debug)
							while (hIsConj(lines[j-1])) {
								j--;
								hDeleteLine(j, lines, linesCnt);
							}
						}
						wcscpy(d, L"↑");
						hMoveLine(lines, linesCnt, j, to);
						i = to;
						to++;
					}
				}
			}
		}
		// replace text
		wchar_t *textEnd = text+wcslen(text);
		wchar_t *seek=text;
		for (int i=0; i<linesCnt; i++) {
			wcscpy(seek, lines[i]);
			seek += wcslen(lines[i]);
			if (seek < textEnd) {
				wcscpy(seek, L"\n");
				seek += wcslen(L"\n");
			}
		}
		delete []lines;
		delete []linesText;
	}
	//hackend
#endif

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
#ifdef SETSUMI_CHANGES
	//hack - tooltip final init
	PosY = 0;
	MouseInside = hIsMouseInsideToolTip(false);
	GetCursorPos(&MousePos);
	KeepToolTip = false;
	if (hMouseHook) UnhookWindowsHookEx(hMouseHook);
	hMouseHook = SetWindowsHookEx(WH_MOUSE_LL, (HOOKPROC)hHookMouseProc, GetModuleHandle(NULL), NULL/*GetCurrentThreadId()*/);
	//hackend
#endif
	return;
}
