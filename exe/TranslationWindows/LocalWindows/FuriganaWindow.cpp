#include <Shared/Shrink.h>
#include "FuriganaWindow.h"
#include "../../Config.h"
#include "../../Dialogs/MyToolTip.h"

#include "../../util/DictionaryUtil.h"
#ifdef SETSUMI_CHANGES
#include "exe/resource.h"
#endif

#define WORD_PADDING 4
#define HEIGHT_PADDING 2

const FuriganaPartOfSpeech FuriganaPartsOfSpeech[8] =
{
	{POS_DUNNO,    RGB(0,0,0),     L""},
	{POS_NOUN,     RGB(230,0,0),   L"名詞"},
	{POS_VERB,     RGB(0,0,255),   L"動詞"},
	{POS_AUX_VERB, RGB(0,64,255),  L"助動詞"},
	{POS_ADJ,      RGB(0,128,255), L"形容詞"},
	{POS_ADV,      RGB(0,200,0),   L"副詞"},
	{POS_PART,     RGB(255,0,255), L"助詞"},
	{POS_SYMBOL,   RGB(0,0,0),     L"記号"},
};

FuriganaWindow::FuriganaWindow(wchar_t *type, char remote, wchar_t *srcUrl, unsigned int flags, DWORD defaultCharType) :
	TranslationWindow(type, remote, srcUrl, flags | TWF_NO_EDIT)
{
	toolTipVisible = 0;
	tracking = 0;
	data = 0;
	hFontBig = 0;
	hFontSmall = 0;
	scrollVisible = 0;

	defaultBKColor = GetPrivateProfileIntW(windowType, L"BK Color", 0, config.ini);
	int furiganaBKDefault = 0xE0E0E0;
	if (!wcsicmp(type, L"JParser"))
		furiganaBKDefault = 0;
	furiganaBKColor = GetPrivateProfileIntW(windowType, L"Furigana BK Color", furiganaBKDefault, config.ini);
	particleBKColor = GetPrivateProfileIntW(windowType, L"Particle BK Color", 0xFFE0E0, config.ini);
	transBKColor = GetPrivateProfileIntW(windowType, L"Trans BK Color", 0xC8F0C8, config.ini);

	normalFontSize = GetPrivateProfileIntW(windowType, L"Normal Font Size", 13, config.ini);
	furiganaFontSize = GetPrivateProfileIntW(windowType, L"Furigana Font Size", 10, config.ini);

	characterType = (FuriganaCharacterType) GetPrivateProfileIntW(windowType, L"Character Set", defaultCharType, config.ini);
	if (characterType != KATAKANA && characterType != ROMAJI && characterType != NO_FURIGANA) characterType = HIRAGANA;

	error = 0;
}

FuriganaWindow::~FuriganaWindow()
{
	if (toolTipVisible) HideToolTip();
	CleanupResult();
	if (hFontBig) DeleteObject(hFontBig);
	if (hFontSmall) DeleteObject(hFontSmall);
	error = 0;
}

void FuriganaWindow::CleanupResult()
{
	if (data)
	{
		free(data->string);
		free(data->words);
		free(data);
		data = 0;
	}
	topWord = 0;
	error = 0;
}

void FuriganaWindow::SaveWindowTypeConfig()
{
	TranslationWindow::SaveWindowTypeConfig();
	WritePrivateProfileInt(windowType, L"BK Color", defaultBKColor, config.ini);
	WritePrivateProfileInt(windowType, L"Furigana BK Color", furiganaBKColor, config.ini);
	WritePrivateProfileInt(windowType, L"Particle BK Color", particleBKColor, config.ini);
	WritePrivateProfileInt(windowType, L"Trans BK Color", transBKColor, config.ini);

	WritePrivateProfileInt(windowType, L"Normal Font Size", normalFontSize, config.ini);
	WritePrivateProfileInt(windowType, L"Furigana Font Size", furiganaFontSize, config.ini);

	WritePrivateProfileInt(windowType, L"Character Set", characterType, config.ini);
}

int FuriganaWindow::GetTopLine()
{
	if (!data || !topWord || !heightFontDouble) return 0;
	RECT r;
	GetClientRect(hWndEdit, &r);
	r.bottom -= 4;
	int numLines = 0;
	if (data && data->numWords)
		numLines = data->words[data->numWords-1].line+1;
	int visibleLines = r.bottom / heightFontDouble;
	if (!visibleLines) visibleLines = 1;
	if (visibleLines >= numLines) return 0;
	int line = data->words[topWord].line;
	int lastLine = numLines - visibleLines;
	if (line <= lastLine) return line;
	return lastLine;
}

void FuriganaWindow::UpdateScrollbar()
{
	RECT r;
	GetClientRect(hWndEdit, &r);
	r.bottom -= 4;
	int numLines = 0;
	if (data && data->numWords)
		numLines = data->words[data->numWords-1].line+1;
	int visibleLines = 0;
	if (heightFontDouble) visibleLines = r.bottom / heightFontDouble;
	int showScrollBar = visibleLines < numLines;
	if (!visibleLines) visibleLines = 1;
	if (showScrollBar != scrollVisible)
	{
		scrollVisible = showScrollBar;
		ShowScrollBar(hWndEdit, SB_VERT, showScrollBar);
	}
	if (showScrollBar)
	{
		int line = GetTopLine();
		SCROLLINFO info;
		info.cbSize = sizeof(info);
		info.fMask = SIF_PAGE | SIF_POS | SIF_RANGE;
		info.nPos = info.nTrackPos = line;
		info.nPage = visibleLines;
		info.nMin = 0;
		info.nMax = numLines-1;
		SetScrollInfo(hWndEdit, SB_VERT, &info, 1);
	}
}

wchar_t *GetFuriganaText(FuriganaWord &word, FuriganaCharacterType type, wchar_t *temp)
{
	if (!word.word) return 0;
	if (type == KATAKANA)
	{
		if (!word.pro || wcslen(word.pro) >= 999) return 0;
		int i = 0;
		// Specialized Hiragana to Katakana code.
		// Don't use ToKatakana() because of the failure case.
		while (word.pro[i])
		{
			if (!IsHiragana(word.pro[i]))
				temp[i] = word.pro[i];
			// Characters that only exist in Katakana.
			else if (word.pro[i] >= 0x3097)
				return word.pro;
			else
				temp[i] = word.pro[i] + 0x60;
			i++;
		}
		temp[i] = 0;
		return temp;
	}
	else if (type == ROMAJI)
	{
		wchar_t *src = word.word;
		if (word.pro) src = word.pro;
		if (wcslen(src) >= 330) return 0;
		int len = ToRomaji(src, temp);
		if (word.pos == POS_PART)
			if (len > 1 && temp[len - 2] == 'h' && temp[len - 1] == 'a' && (len < 3 || temp[len - 3] != 'c'))
				temp[len - 2] = 'w';
			else if (len == 2 && temp[0] == 'h' && temp[1] == 'e')
				wcscpy(temp, L"e");
		return temp;
	}
	// HIRAGANA
	else
	{
		if (!word.pro || wcslen(word.pro) >= 999) return 0;
		int i = 0;
		while (word.pro[i])
		{
			// Specialized Hiragana to Katakana code.
			// Don't use ToKatakana() because of the failure case.
			// Failure case isn't as important as the Hiragana one,
			// though neither one matters all that much.
			if (!IsKatakana(word.pro[i]))
				temp[i] = word.pro[i];
			// Characters that only exist in Katakana.
			else if (word.pro[i] == 0x30A0 || word.pro[i] >= 0x30F7)
				return word.pro;
			else
				temp[i] = word.pro[i] - 0x60;
			i++;
		}
		temp[i] = 0;
		return temp;
	}
	return 0;
}

void FuriganaWindow::Reformat(int widthChangeOnly)
{
	RECT r;
	HideToolTip();
	GetClientRect(hWndEdit, &r);
	int width = r.right -= 4;
	r.bottom -= 4;
	int line = 0;
	int pos = 0;
	if (hFontBig && data)
	{
		if (!widthChangeOnly)
		{
			HDC hDC = GetDC(hWndEdit);
			if (hDC)
			{
				SIZE s;
				HGDIOBJ hOldFont = SelectObject(hDC, hFontBig);
				TEXTMETRIC tm;

				for (int i=0; i<data->numWords; i++)
				{
					FuriganaWord *word = &data->words[i];
					if (!word->word)
						continue;
					GetTextExtentPoint32(hDC, word->word, wcslen(word->word), &s);
					word->width = (unsigned short)s.cx;
				}
				GetTextMetrics(hDC, &tm);
				heightFontBig = tm.tmHeight;

				SelectObject(hDC, hFontSmall);

				for (int i=0; i<data->numWords; i++)
				{
					wchar_t temp[1000];
					wchar_t *furigana;
					FuriganaWord *word = &data->words[i];
					furigana = GetFuriganaText(data->words[i], characterType, temp);
					if (!furigana || !furigana[0] || characterType == NO_FURIGANA)
						word->furiganaWidth = 0;
					else
					{
						GetTextExtentPoint32(hDC, furigana, wcslen(furigana), &s);
						word->furiganaWidth = (unsigned short)s.cx;
					}
				}
				GetTextMetrics(hDC, &tm);
				heightFontSmall = tm.tmHeight;

				SelectObject(hDC, hOldFont);
				ReleaseDC(hWndEdit, hDC);

				if (characterType != NO_FURIGANA)
					heightFontDouble = heightFontSmall + heightFontBig + HEIGHT_PADDING;
				else
					heightFontDouble = heightFontBig + HEIGHT_PADDING;
			}
		}
		for (int i=0; i<data->numWords; i++)
		{
			if (pos) pos += WORD_PADDING;
			FuriganaWord *word = &data->words[i];
			if (!word->word)
			{
				word->line = line;
				word->hOffset = pos;
				if (word->pos == POS_LINEBREAK)
				{
					pos = 0;
					line++;
				}
				if (pos == WORD_PADDING) pos = 0;
				continue;
			}
			int wordWidth = word->width;
			if (wordWidth < word->furiganaWidth) wordWidth = word->furiganaWidth;
			if (pos && pos + wordWidth > width)
			{
				pos = 0;
				line++;
			}
			word->line = line;
			word->hOffset = pos;
			pos += wordWidth;
		}
	}
	formatWidth = width;
	if (pos) line++;
	UpdateScrollbar();
	InvalidateRect(hWndEdit, 0, 0);
}

int FuriganaWindow::GetStartWord()
{
	int tl = GetTopLine();
	int start = 0;
	int step = 1+data->numWords/2;
	while (1)
	{
		if (start + step < data->numWords && data->words[start+step].line < tl)
			start += step;
		if (step == 1) break;
		step = (step+1)/2;
	}
	return start;
}

void FuriganaWindow::Draw()
{
	PAINTSTRUCT ps;
	RECT client;
	HideToolTip();
	GetClientRect(hWndEdit, &client);
	HDC hDC = BeginPaint(hWndEdit, &ps);
	// Wine bugfix: clear the screen
	FillRect(hDC, &client, GetSysColorBrush(COLOR_WINDOW));  
	if (hDC && hFontBig)
	{
		HGDIOBJ hOldFont = SelectObject(hDC, hFontBig);

		unsigned int bkColor;
		unsigned int textColor = config.font.color;
		int enabled = IsWindowEnabled(hWndEdit);
		if (!enabled)
		{
			textColor = GetSysColor(COLOR_GRAYTEXT);
			bkColor = GetSysColor(COLOR_3DFACE);
		}
		else
		{
			if (!textColor) textColor = GetSysColor(COLOR_WINDOWTEXT);
			bkColor = GetSysColor(COLOR_WINDOW);
		}

		unsigned int oldTextColor = SetTextColor(hDC, textColor);
		unsigned int oldBKColor = SetBkColor(hDC, bkColor);

		int oldBKMode = SetBkMode(hDC, OPAQUE);
		if (error)
			TextOutW(hDC, 2, 2, error, wcslen(error));
		else if (data && data->numWords)
		{
			if (defaultBKColor && enabled)
			{
				SetBkColor(hDC, defaultBKColor);
				bkColor = defaultBKColor;
			}
			int tl = GetTopLine();

			int start = GetStartWord();

			for (int i=start; i<data->numWords; i++)
			{
				FuriganaWord *word = &data->words[i];
				if (word->line < tl || !word->word) continue;
				RECT r;
				r.top = 2+(word->line - tl)*(heightFontDouble);
				if (characterType != NO_FURIGANA)
					r.top += heightFontSmall;
				if (r.top > client.bottom) break;
				r.left = word->hOffset+2;

				int restoreBKColor = 0;

				int color = bkColor;
				if ((word->flags & WORD_HAS_TRANS) && transBKColor)
					color = transBKColor;
				if (characterType != ROMAJI && characterType != NO_FURIGANA && word->pro)
				{
					if (furiganaBKColor)
						color = furiganaBKColor;
				}
				if (word->pos == POS_PART && particleBKColor)
					color = particleBKColor;
				if (color!=bkColor)
				{
					SetBkColor(hDC, color);
					restoreBKColor = 1;
				}

				if (word->width < word->furiganaWidth)
				{
					int w = word->width;
					if (w < word->furiganaWidth) w = word->furiganaWidth;
					r.bottom = r.top + heightFontBig;
					r.right = r.left + w-1;
					if (restoreBKColor)
					{
						HBRUSH junkBrush = CreateSolidBrush(color);
						FillRect(hDC, &r, junkBrush);
						DeleteObject(junkBrush);
					}
					r.left += (word->furiganaWidth - word->width+1)/2;
				}

				DrawText(hDC, word->word, -1, &r, DT_NOCLIP);
				if (restoreBKColor)
					SetBkColor(hDC, bkColor);
			}

#ifdef SETSUMI_CHANGES
			if (enabled) {
				//hack - background color 2
				SetBkColor(hDC, defaultBKColor);
				//SetBkColor(hDC, GetSysColor(COLOR_WINDOW));
				//hackend
			}
#else
			if (enabled) SetBkColor(hDC, GetSysColor(COLOR_WINDOW));
#endif
			else SetBkColor(hDC, GetSysColor(COLOR_3DFACE));


			if (characterType != NO_FURIGANA)
			{
				SelectObject(hDC, hFontSmall);
				for (int i=start; i<data->numWords; i++)
				{
					wchar_t temp[1000];
					wchar_t *furigana;

					FuriganaWord *word = &data->words[i];
					if (word->line < tl) continue;

					furigana = GetFuriganaText(data->words[i], characterType, temp);
					if (!furigana || !furigana[0]) continue;

					RECT r;
					r.top = 2+(word->line - tl)*(heightFontDouble);
					if (r.top > client.bottom) break;
					r.left = word->hOffset+2;
					if (word->width > word->furiganaWidth)
						r.left += (word->width-word->furiganaWidth+1)/2;
#ifdef SETSUMI_CHANGES
					//hack - furigana color
					{
						COLORREF col = RGB(100,100,255); // words with translation
						wchar_t *wd = word->word;
						if (word->srcWord) {
							wd = word->srcWord;
						}
						if (wd && DictsLoaded()) {
							Match *matches;
							int numMatches;
							FindExactMatches(wd, wcslen(wd), matches, numMatches);
							if (!numMatches) {
								col = RGB(140,140,140); // words without translation
							} else {
								free(matches);
							}
						}
						SetTextColor(hDC, col);
					}
					//hackend
#endif
					DrawText(hDC, furigana, -1, &r, DT_NOCLIP);
				}
			}
		}
		SelectObject(hDC, hOldFont);
		SetBkMode(hDC, oldBKMode);
		SetBkColor(hDC, oldBKColor);
		SetTextColor(hDC, oldTextColor);
	}
	if (hDC) EndPaint(hWndEdit, &ps);
}

void FuriganaWindow::CheckToolTip(int x, int y)
{
	if (toolTipVisible)
		if (x < toolTipRect.left ||
			x > toolTipRect.right ||
			y < toolTipRect.top ||
			y > toolTipRect.bottom)
#ifdef SETSUMI_CHANGES
				//hack - keep tooltip if needed
				if (!hIsKeepToolTip()) HideToolTip();
				//HideToolTip();
				//hackend
#else
				HideToolTip();
#endif
}

void FuriganaWindow::HideToolTip()
{
	if (toolTipVisible)
	{
		ShowToolTip(0, 0, 0);
		toolTipVisible = 0;
	}
}

LRESULT CALLBACK FuriganaTextWindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	FuriganaWindow *window = (FuriganaWindow*)GetWindowLongPtr(hWnd, GWLP_USERDATA);
	if (window)
	{
		switch (uMsg)
		{
			// Not sure if this is necessary, but have had things mess up without it.
			case WM_MOUSELEAVE:
#ifdef SETSUMI_CHANGES
				//hack - fix tooltip flickering
				if (!hIsMouseInsideToolTip() && !hIsKeepToolTip())
				//hackend
#endif
				window->HideToolTip();
				window->tracking = 0;
				break;
			case WM_MOUSEHOVER:
			{
				if (!window->data) break;
				int tl = window->GetTopLine();
				int start = window->GetStartWord();
				Result *res = window->data;
				int delta = 2 - tl*window->heightFontDouble;
				int x = (short)lParam;
				int y = (short)(lParam>>16);
				RECT r;
				while (start < res->numWords)
				{
					r.top = res->words[start].line * window->heightFontDouble +delta;
					if (r.top >= y) break;
					r.bottom = r.top + window->heightFontDouble;
					if (r.bottom > y)
					{
						if (res->words[start].hOffset <= x)
						{
							r.left = res->words[start].hOffset;
							int width = res->words[start].width;
							if (res->words[start].furiganaWidth > width)
								width = res->words[start].furiganaWidth;
							r.right = r.left + width + WORD_PADDING;
							if (r.right > x)
							{
								RECT r2;
								POINT p;
								GetCursorPos(&p);
								r2.left = r.left + p.x - x;
								r2.right = r.right + p.x - x;
								r2.top = r.top + p.y - y;
								r2.bottom = r.bottom + p.y - y;
								window->WordHover(start, r, r2);
							}
						}
					}
					start++;
				}
				window->tracking = 0;
				break;
			}
			case WM_VSCROLL:
				if (window->data && window->data->numWords)
				{
					int line = window->data->words[window->topWord].line;
					if (wParam == SB_TOP)
						window->topWord = 0;
					else if (wParam == SB_BOTTOM)
						window->topWord = window->data->numWords-1;
					else
					{
						if (!window->heightFontDouble) break;
						int line = window->data->words[window->topWord].line;
						RECT r;
						GetClientRect(window->hWndEdit, &r);
						r.bottom -= 4;
						int visibleLines = r.bottom / window->heightFontDouble;
						if (!visibleLines) visibleLines = 1;
						int numLines = window->data->words[window->data->numWords-1].line+1;
						int cmd = LOWORD(wParam);
						if (cmd == SB_LINEUP || cmd == SB_PAGEUP)
						{
							if (cmd == SB_LINEUP)
								line--;
							else
								line -= visibleLines;
							if (line <= 0)
								window->topWord = 0;
							else
								while (window->data->words[window->topWord-1].line >= line)
									window->topWord--;
						}
						else if (cmd == SB_LINEDOWN || cmd == SB_PAGEDOWN)
						{
							if (cmd == SB_LINEDOWN)
								line++;
							else
								line += visibleLines;
							int max = numLines-visibleLines;
							if (max < line) line = max;
							while (window->topWord < window->data->numWords-1 &&
								window->data->words[window->topWord].line < line)
									window->topWord++;
						}
						else if (cmd == SB_THUMBTRACK)
						{
							int line = HIWORD(wParam);
							int pos = window->GetTopLine();
							if (line <= 0) window->topWord = 0;
							else if (line <= pos)
								while (window->data->words[window->topWord-1].line >= line)
									window->topWord--;
							else
								while (window->topWord < window->data->numWords-1 &&
									window->data->words[window->topWord].line < line)
										window->topWord++;
						}
					}
					window->UpdateScrollbar();
					InvalidateRect(hWnd, 0, 1);
				}
				return 0;
			case WM_KEYDOWN:
				if (wParam == VK_DOWN)
					FuriganaTextWindowProc(hWnd, WM_VSCROLL, SB_LINEDOWN, 0);
				else if (wParam == VK_NEXT)
					FuriganaTextWindowProc(hWnd, WM_VSCROLL, SB_PAGEDOWN, 0);
				else if (wParam == VK_UP)
					FuriganaTextWindowProc(hWnd, WM_VSCROLL, SB_LINEUP, 0);
				else if (wParam == VK_PRIOR)
					FuriganaTextWindowProc(hWnd, WM_VSCROLL, SB_PAGEUP, 0);
				else if (wParam == VK_HOME)
					FuriganaTextWindowProc(hWnd, WM_VSCROLL, SB_TOP, 0);
				else if (wParam == VK_END)
					FuriganaTextWindowProc(hWnd, WM_VSCROLL, SB_BOTTOM, 0);
				else
					break;
				return 0;
			case WM_LBUTTONDOWN:
				SetFocus(hWnd);
				return 0;
			case WM_MOUSEWHEEL:
				{
					int delta = -((short)HIWORD(wParam))/WHEEL_DELTA;
					int pos = window->GetTopLine()+delta;
					if (pos < 0) pos = 0;
					FuriganaTextWindowProc(hWnd, WM_VSCROLL, (pos<<16) + SB_THUMBTRACK, 0);
				}
				return 0;
			case WM_ERASEBKGND:
			{
				HDC hDC = (HDC) wParam;
				RECT r;
				GetClientRect(window->hWndEdit, &r);
				if (IsWindowEnabled(hWnd))
#ifdef SETSUMI_CHANGES
						//hack - background color 1
						SetDCBrushColor(hDC, window->defaultBKColor);
						FillRect(hDC, &r, (HBRUSH)GetStockObject(DC_BRUSH));
						//hackend
#else
					FillRect(hDC, &r, GetSysColorBrush(COLOR_WINDOW));
#endif
				else
					FillRect(hDC, &r, GetSysColorBrush(COLOR_3DFACE));
				return 0;
			}
			case WM_ENABLE:
				InvalidateRect(hWnd, 0, 0);
				return 0;
			case WM_SIZE:
			case WM_SIZING:
			{
				RECT r;
				GetClientRect(hWnd, &r);
				r.right -= 4;
				if (r.right != window->formatWidth)
					window->Reformat(1);
				else
					window->UpdateScrollbar();
				break;
			}
			case WM_PAINT:
				window->Draw();
				return 0;
			case EM_SETCHARFORMAT:
			{
				if (window->hFontBig) DeleteObject(window->hFontBig);
				if (window->hFontSmall) DeleteObject(window->hFontSmall);
				CHARFORMAT *format = (CHARFORMAT*)lParam;
				LOGFONT lf;
				memset(&lf, 0, sizeof(lf));
				//lf.lfHeight = -format->yHeight/15;
				if (window->normalFontSize < 1) window->normalFontSize = 1;
				else if (window->normalFontSize > 100) window->normalFontSize = 100;
				if (window->furiganaFontSize < 1) window->furiganaFontSize = 1;
				else if (window->furiganaFontSize > 100) window->furiganaFontSize = 100;
				lf.lfHeight = -window->normalFontSize;

				lf.lfWeight = FW_NORMAL;
				if (format->dwEffects & CFM_BOLD) lf.lfWeight = FW_BOLD;
				if (format->dwEffects & CFM_ITALIC) lf.lfItalic = 1;
				if (format->dwEffects & CFM_UNDERLINE) lf.lfUnderline = 1;
				if (format->dwEffects & CFM_STRIKEOUT) lf.lfStrikeOut = 1;
				lf.lfClipPrecision = CLIP_DEFAULT_PRECIS;
				lf.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
				lf.lfQuality = CLEARTYPE_QUALITY;
				format->bCharSet = DEFAULT_CHARSET;
				wcscpy(lf.lfFaceName, format->szFaceName);
				window->hFontBig = CreateFontIndirect(&lf);
				if (window->hFontBig)
				{
					lf.lfHeight = -window->furiganaFontSize;
					//lf.lfHeight = -1+(lf.lfHeight * 3-4)/5;
					window->hFontSmall = CreateFontIndirect(&lf);
					if (!window->hFontSmall)
					{
						DeleteObject(window->hFontBig);
						window->hFontBig = 0;
					}
				}
				window->Reformat();
				return 0;
			}
			case WM_MOUSEMOVE:
				if (!window->tracking)
				{
					window->CheckToolTip((short)lParam, (short)(lParam>>16));
					if (!window->toolTipVisible)
					{
						TRACKMOUSEEVENT me;
						me.cbSize = sizeof(me);
						me.dwHoverTime = 350;
						me.hwndTrack = hWnd;
						me.dwFlags = TME_HOVER | TME_LEAVE;
						TrackMouseEvent(&me);
						window->tracking = 1;
					}
				}
				break;
			default:
				break;
		}
	}
	else if (uMsg == WM_CREATE)
	{
		CREATESTRUCT *cs = (CREATESTRUCT *)lParam;
		SetWindowLongPtr(hWnd, GWLP_USERDATA, (ULONG_PTR)cs->lpCreateParams);
	}

	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

int FuriganaWindow::MakeWindow(int showToolbar, HWND hWndParent)
{
	static unsigned char registered = 0;
	if (!registered)
	{
		WNDCLASS FuriganaWindowClass =
		{
			CS_HREDRAW | CS_VREDRAW,
			FuriganaTextWindowProc,
			0, 0,
			ghInst,
			0,
#ifdef SETSUMI_CHANGES
			LoadCursor(ghInst, MAKEINTRESOURCE(IDC_POINTER)/*IDC_ARROW*/), //hack - set parser's cursor to not stand out
#else
			LoadCursor(NULL, IDC_ARROW),
#endif
			(HBRUSH)(COLOR_WINDOW+1),
			0,
			L"FURIGANA SUBWINDOW"
		};
		if (!RegisterClass(&FuriganaWindowClass)) return 0;
		registered = 1;
	}
	if (!hWnd)
	{
		TranslationWindow::MakeWindow(showToolbar, hWndParent);
#ifdef SETSUMI_CHANGES
		//hack - reduce border 1
		hWndEdit = CreateWindowEx(0/*WS_EX_STATICEDGE*/, L"FURIGANA SUBWINDOW", L"",
#else
		hWndEdit = CreateWindowEx(WS_EX_STATICEDGE, L"FURIGANA SUBWINDOW", L"",
#endif
			CCS_NODIVIDER  | ES_MULTILINE | WS_VISIBLE | WS_CHILD | WS_TABSTOP,
			CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
			hWnd, 0, ghInst, this);
		SetFont();
		UpdateEnabled();
	}
	else
		TranslationWindow::MakeWindow(showToolbar, hWndParent);
	return 1;
}

void FuriganaWindow::NukeWindow()
{
	TranslationWindow::NukeWindow();
	CleanupResult();
}

void FuriganaWindow::WordHover(int word, RECT &clientRect, RECT &globalRect)
{
	//if (data->words[p].flags & WORD_HAS_TRANS) {
	wchar_t *wd = data->words[word].word;
	if (data->words[word].srcWord)
		wd = data->words[word].srcWord;
	if (wd && PopupDef(wd, clientRect, globalRect, hWnd))
	{
		toolTipVisible = 1;
		toolTipRect = clientRect;
	}
}
