#include <Shared/Shrink.h>
#include "JdicWindow.h"
#include <Shared/StringUtil.h>
#include "../../util/HttpUtil.h"
#include "../../Config.h"

#include "../../resource.h"

#define WWWJDIC_DEFAULT L"http://wwwjdic.se/cgi-bin/wwwjdic.cgi"

void JdicWindow::LoadConfig()
{
	free(host);
	free(path);
	wchar_t url[1000];
	GetPrivateProfileStringW(L"WWWJDIC", L"Mirror", WWWJDIC_DEFAULT, url, sizeof(url)/sizeof(*url) - 5, config.ini);
	int base;
	if (!wcsnicmp(url, L"https://", 8))
	{
		port = 443;
		base = 8;
	}
	else
	{
		port = 80;
		if (!wcsnicmp(url, L"http://", 7))
			base = 7;
		else
			base = 0;
	}
	if (wchar_t *p = wcschr(url+base, '/'))
	{
		if (wchar_t *p2 = wcschr(p, '?'))
			*p2 = 0;
		wcscat(p, L"?9ZIG");
		path = wcsdup(p);
		*p = 0;
	}
	else
		path = wcsdup(L"/?9ZIG");
	host = wcsdup(url+base);
}

JdicWindow::JdicWindow() : HttpWindow(L"WWWJDIC", L"http://wwwjdic.se/cgi-bin/wwwjdic.cgi", TWF_RICH_TEXT | TWF_SEPARATOR | TWF_CONFIG_BUTTON)
{
	autoClipboard = 0;
	requestHeaders = L"";
	host = 0;
	path = 0;

	LoadConfig();

	//host = L"www.csse.monash.edu.au";
	//path = L"/~jwb/cgi-bin/wwwjdic.cgi";
}

wchar_t *JdicWindow::GetTranslationPath(Language src, Language dst, const wchar_t *text)
{
	if (src != LANGUAGE_Japanese || dst != LANGUAGE_English)
		return NULL;
	if (!text)
		return _wcsdup(L"");
	wchar_t *data = (wchar_t*)malloc((wcslen(path) + wcslen(text) + 1)*sizeof(wchar_t));
	wcscpy(data, path);
	wcscat(data, text);
	return data;
}

JdicWindow::~JdicWindow()
{
	free(host);
	free(path);
}

wchar_t *JdicWindow::FindTranslatedText(wchar_t* html)
{
	int slen;
	wchar_t *start = GetSubstring(html, L"<BODY>", L"</BODY>", &slen);
	if (!slen)
		return 0;

	start[slen] = 0;
	return start;
}

void JdicWindow::StripResponse(wchar_t* html, std::wstring* text) const
{
	wchar_t *pos=html;
	int inFont = 0;
	wchar_t *color = (wchar_t*)calloc(2, sizeof(wchar_t));
	int colorLen = 0;

	COLORREF last = 0;
	while (*pos)
	{
		if (!wcsnicmp(pos, L"<font", 5))
		{
			COLORREF c = 0;
			if (!wcsnicmp(pos+5, L" color=\"blue\">", 14))
			{
				pos += 19;
				c = RGB(0,0,255);
				if (last == c) c ^= RGB(0,0x80,0);
			}
			else if (!wcsnicmp(pos+5, L" color=\"red\">", 13))
			{
				pos += 18;
				c = RGB(255,0,0);
				if (last == c) c = RGB(255,128,0);
			}
			last = c;
			wchar_t *start = pos;
			if (c)
			{
				while (*pos && pos[0] != '<')
					pos++;
				if (wcsnicmp(pos, L"</font>", 7) || pos == start) continue;
				color = (wchar_t*) realloc(color, sizeof(wchar_t) * (pos - start + 4 + colorLen));
				wcsncpy(color+colorLen, start, pos-start);
				colorLen += pos-start;
				color[colorLen++] = 0;
				*(COLORREF*)(color + colorLen) = c;
				colorLen += 2;
				color[colorLen] = 0;
			}
		}
		pos++;
	}
	UnescapeHtml(html, 0);
	SpiffyUp(html);
	int len = wcslen(html);

	text->reserve(len+1 + colorLen);
	text->assign(html, len + 1);
	text->append(color, colorLen);
	free(color);
	/*
	wchar_t *out = (wchar_t*)malloc((len+1 + colorLen+1)*sizeof(wchar_t));
	wcscpy(out, html);
	memcpy(out+len+1, color, sizeof(wchar_t)*(colorLen+1));
	return out;
	//*/
}


void JdicWindow::DisplayResponse(const wchar_t* text)
{
	if (hWndEdit)
	{
		SetWindowTextW(hWndEdit, text);
		const wchar_t *blockStart = text;
		const wchar_t *color = wcschr(text, 0) + 1;

		CHARFORMAT cf;
		memset(&cf, 0, sizeof(cf));
		cf.cbSize = sizeof(cf);
		cf.dwMask = CFM_COLOR;
		cf.dwEffects = 0;
		while (1)
		{
			while (*blockStart==' ' || *blockStart == '\n' || *blockStart == '\t') blockStart++;
			const wchar_t *lineStart = blockStart;
			const wchar_t *lineEnd = wcsstr(blockStart, L"\n\n");
			if (!lineEnd)
			{
				lineEnd = wcschr(blockStart, '\n');
				if (!lineEnd) break;
			}
			lineEnd += 2;
			const wchar_t *colorDefStart = lineEnd;
			const wchar_t *pos = lineStart;
			while (*color && lineEnd)
			{
				pos = wcsstr(pos, color);
				if (!pos || pos >= lineEnd) break;
				int len = wcslen(color);
				const wchar_t *color2 = color+len+1;
				cf.crTextColor = *(COLORREF*)color2;
				SendMessage(hWndEdit, EM_SETSEL, pos-text, pos-text + len);
				SendMessage(hWndEdit, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
				//break;
				while (*colorDefStart && *colorDefStart < 0x100) colorDefStart++;
				const wchar_t *colorDefStop = colorDefStart;
				while (*colorDefStop >= 0x100) colorDefStop++;
				SendMessage(hWndEdit, EM_SETSEL, colorDefStart-text, colorDefStop-text);
				SendMessage(hWndEdit, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
				while(*colorDefStart && *colorDefStart != '\n') colorDefStart++;
				if (!*colorDefStart) break;

				pos += len;
				color = color2+2;
			}
			if (!pos || !*color) break;
			blockStart = wcsstr(lineEnd, L"\n\n");
			if (!blockStart) break;
		}//*/

		const wchar_t *pos = text;
		while (pos = wcsstr(pos+1, L"[Partial Match!]"))
		{
			cf.crTextColor = RGB(255,0,0);
			SendMessage(hWndEdit, EM_SETSEL, pos-text, pos-text + 16);
			SendMessage(hWndEdit, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
		}

		pos = wcschr(text,  0);
		const wchar_t *lastParen = 0;
		while (pos >= text)
		{
			if (*pos == ')') lastParen = pos+1;
			if (!wcsnicmp(pos, L"\n Possible ", 11) && lastParen)
			{
				SendMessage(hWndEdit, EM_SETSEL, lastParen-text, lastParen-text);
				SendMessage(hWndEdit, EM_REPLACESEL, 0, (LPARAM)L"\n\t");
			}
			if (*pos == '\n') lastParen = 0;
			pos --;
		}
		SendMessage(hWndEdit, EM_SETSEL, 0, 0);
	}
}

INT_PTR CALLBACK JdicDialogProc(HWND hWndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	HWND hWndCombo = GetDlgItem(hWndDlg, IDC_WWWJDICT_MIRROR_COMBO);
	switch (uMsg)
	{
		case WM_INITDIALOG:
			{
				const wchar_t *mirrors[] =
				{
					L"http://www.csse.monash.edu.au/~jwb/cgi-bin/wwwjdic.cgi",
					L"http://wwwjdic.biz/cgi-bin/wwwjdic",
					L"http://wwwjdic.se/cgi-bin/wwwjdic.cgi",
					L"http://www.edrdg.org/cgi-bin/wwwjdic/wwwjdic",
					L"https://gengo.com/wwwjdic/cgi-data/wwwjdic",
				};
				for (int i=0; i<sizeof(mirrors)/sizeof(mirrors[0]); i++)
					SendMessage(hWndCombo, CB_ADDSTRING, 0, (LPARAM)mirrors[i]);
				JdicWindow * jdic = (JdicWindow*)lParam;
				wchar_t temp[1014];
				wsprintf(temp, L"http%s://%s%s", jdic->port == 443 ? L"s" : L"", jdic->host, jdic->path);
				if (wchar_t *p = wcschr(temp, '?'))
					*p = 0;
				int w = SendMessage(hWndCombo, CB_FINDSTRINGEXACT, -1, (LPARAM) temp);
				if (w < 0)
					SendMessage(hWndCombo, CB_ADDSTRING, 0, (LPARAM)temp);
				SendMessage(hWndCombo, CB_SELECTSTRING, 0, (LPARAM)temp);
			}
			break;
		case WM_COMMAND:
			if (HIWORD(wParam) == BN_CLICKED)
			{
				int i = LOWORD(wParam);
				if (i == IDOK)
				{
					wchar_t temp[1000];
					int len = GetWindowTextW(hWndCombo, temp, sizeof(temp)/sizeof(*temp));
					if (len > 0 && len < sizeof(temp)/sizeof(*temp))
						WritePrivateProfileStringW(L"WWWJDIC", L"Mirror", temp, config.ini);
					EndDialog(hWndDlg, 0);
				}
				else if (i==IDCANCEL)
					EndDialog(hWndDlg, 0);
			}
			break;
			/*
			if (HIWORD(wParam) == BN_CLICKED)
			{
				i = LOWORD(wParam);
				if (i == IDOK || i == IDC_APPLY)
				{
					for (int j=0; j<3; j++)
					{
						if (GetDlgItemText(hWndDlg, IDC_DEFAULT_COLOR + 2*j, str, 30))
						{
							wchar_t *end = 0;
							unsigned int color = wcstoul(str, &end, 16);
							mecabWindow->colors[j] = RGBFlip(color);
						}
					}
					mecabWindow->normalFontSize = GetDlgItemInt(hWndDlg, IDC_NORMAL_FONT_SIZE, 0, 1);
					mecabWindow->furiganaFontSize = GetDlgItemInt(hWndDlg, IDC_FURIGANA_FONT_SIZE, 0, 1);
					mecabWindow->characterType = HIRAGANA;
					if (BST_CHECKED == IsDlgButtonChecked(hWndDlg, IDC_KATAKANA))
						mecabWindow->characterType = KATAKANA;
					else if (BST_CHECKED == IsDlgButtonChecked(hWndDlg, IDC_ROMAJI))
						mecabWindow->characterType = ROMAJI;
					mecabWindow->SetFont();
					mecabWindow->SaveWindowTypeConfig();
					if (i == IDOK)
						EndDialog(hWndDlg, 1);
				}
				else if (i == IDCANCEL)
					EndDialog(hWndDlg, 0);
				else if (i >= IDC_DEFAULT_COLOR && i < IDC_DEFAULT_COLOR+6 && ((i-IDC_DEFAULT_COLOR)&1))
				{
					int index = (i-IDC_DEFAULT_COLOR)/2;
					unsigned int color = 0;
					if (GetDlgItemText(hWndDlg, i-1, str, 30))
						color = wcstoul(str, 0, 16);
					CHOOSECOLOR cc;
					cc.lStructSize = sizeof(cc);
					cc.hwndOwner = hWndDlg;
					cc.rgbResult = color;
					static COLORREF customColors[16] =
					{
						0xFFFFFF,0xFFFFFF,0xFFFFFF,0xFFFFFF,
						0xFFFFFF,0xFFFFFF,0xFFFFFF,0xFFFFFF,
						0xFFFFFF,0xFFFFFF,0xFFFFFF,0xFFFFFF,
						0xFFFFFF,0xFFFFFF,0xFFFFFF,0xFFFFFF,
					};
					cc.lpCustColors = customColors;
					cc.Flags = CC_ANYCOLOR | CC_RGBINIT | CC_FULLOPEN;
					if (ChooseColor(&cc))
					{
						wsprintfW(str, L"%06X", RGBFlip(cc.rgbResult));
						SetDlgItemText(hWndDlg, i-1, str);
					}
				}
			}
			break;
			//*/
		default:
			break;
	}
	return 0;
}

int JdicWindow::WndProc(LRESULT *output, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (uMsg == WM_COMMAND)
	{
		if (HIWORD(wParam) == BN_CLICKED)
		{
			if (LOWORD(wParam) == ID_CONFIG)
			{
				DialogBoxParam(ghInst, MAKEINTRESOURCE(IDD_WWWJDIC_CONFIG), hWnd, JdicDialogProc, (LPARAM)this);
				LoadConfig();
				return 1;
			}
		}
	}
	return HttpWindow::WndProc(output, uMsg, wParam, lParam);;
}
