#include <Shared/Shrink.h>
#include "TranslationWindow.h"

#include "../resource.h"

#define NUM_ICONS 16

TranslationWindow::TranslationWindow(wchar_t *type, char remote, wchar_t *srcUrl, unsigned int flags) :
	queued_string_history_id_(-1),
	queuedString(NULL),
	srcUrl(srcUrl),
	remote(remote),
	flags(flags),
	hWnd(0),
	hWndParent(0),
	hWndEdit(0),
	hWndRebar(0),
	hWndToolbar(0),
	busy(0),
	windowType(type)
{
	autoClipboard = GetPrivateProfileIntW(windowType, L"Auto Clipboard", !remote, config.ini);
}

void TranslationWindow::SaveWindowTypeConfig()
{
	WritePrivateProfileInt(windowType, L"Auto Clipboard", autoClipboard, config.ini);
}

void TranslationWindow::ClearQueuedTask()
{
	if (queuedString)
	{
		queuedString->Release();
		queuedString = NULL;
		queued_string_history_id_ = -1;
	}
}

TranslationWindow::~TranslationWindow()
{
	ClearQueuedTask();
}

HWND CreateToolbar(HWND hParent)
{
	HWND hWndToolbar = CreateWindowEx(0, TOOLBARCLASSNAME, NULL,
		WS_VISIBLE | WS_CHILD | TBSTYLE_LIST | TBSTYLE_FLAT | CCS_NODIVIDER | CCS_NORESIZE | WS_CLIPSIBLINGS
		| TBSTYLE_TOOLTIPS,
		0, 0, 0, 0,
		hParent, NULL, ghInst, NULL);

	SendMessage(hWndToolbar, TB_SETEXTENDEDSTYLE, 0, TBSTYLE_EX_MIXEDBUTTONS);

	TBADDBITMAP bmp;
	bmp.hInst = ghInst;
	bmp.nID = IDR_TOOLBAR_TRANSLATION;
	SendMessage(hWndToolbar, TB_SETBITMAPSIZE, 0, (LPARAM) MAKELONG (19, 19));
	SendMessage(hWndToolbar, TB_ADDBITMAP, (WPARAM)NUM_ICONS, (LPARAM)&bmp);
	SendMessage(hWndToolbar,TB_SETPADDING,0, MAKELPARAM(4,4));
	SendMessage(hWndToolbar, TB_SETBUTTONSIZE, 0, (LPARAM) MAKELONG (21, 21));
	SendMessage(hWndToolbar, TB_BUTTONSTRUCTSIZE, (WPARAM)sizeof(TBBUTTON), 0);
	return hWndToolbar;
}

void TranslationWindow::SetFont()
{
	if (hWndEdit)
	{
		CHARFORMAT format;
		format.cbSize = sizeof(format);
		format.bPitchAndFamily = FF_DONTCARE | VARIABLE_PITCH;
		format.bCharSet = ANSI_CHARSET;
		format.dwMask = CFM_COLOR | CFM_BOLD | CFM_FACE | CFM_SIZE | CFM_BOLD | CFM_FACE | CFM_ITALIC | CFM_UNDERLINE | CFM_STRIKEOUT;
		format.dwEffects = 0;
		if (config.font.bold) format.dwEffects |= CFE_BOLD;
		if (config.font.italic) format.dwEffects |= CFE_ITALIC;
		if (config.font.underline) format.dwEffects |= CFE_UNDERLINE;
		if (config.font.strikeout) format.dwEffects |= CFE_STRIKEOUT;
		if (config.font.color)
			format.crTextColor = config.font.color;
		else
			format.dwEffects |= CFE_AUTOCOLOR;

		format.yHeight = config.font.height;

		wcscpy(format.szFaceName, config.font.face);

		SendMessage(hWndEdit, EM_SETCHARFORMAT, SCF_ALL|SCF_DEFAULT, (LPARAM) &format);
	}
}

void TranslationWindow::ShowToolbar(int show)
{
	if (!hWnd) return;
	if (!show)
	{
		if (!hWndRebar) return;
		DestroyWindow(hWndRebar);
		hWndRebar = 0;
		hWndToolbar = 0;
	}
	else
	{
		if (hWndRebar) return;

		hWndRebar = CreateWindowEx(0, REBARCLASSNAME, 0,
			RBS_FIXEDORDER | RBS_AUTOSIZE | WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | CCS_NODIVIDER,
			CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
			hWnd, 0, ghInst, 0);

		hWndToolbar = CreateToolbar(hWndRebar);
		TBBUTTON tbButtons[] =
		{
			//{0, ID_TRANSLATE, TBSTATE_ENABLED, BTNS_AUTOSIZE | BTNS_SHOWTEXT | TBSTYLE_BUTTON, {0}, 0, (INT_PTR)windowType},
			{0, ID_TRANSLATE, TBSTATE_ENABLED, BTNS_AUTOSIZE | BTNS_SHOWTEXT | TBSTYLE_BUTTON, {0}, 0, (INT_PTR)windowType},
			{3, ID_AUTO_CLIPBOARD, TBSTATE_ENABLED | (TBSTATE_CHECKED * (autoClipboard != 0)), BTNS_CHECK, {0}, 0, (INT_PTR)L"Automatic Clipboard Translation"},
		};

		// Add buttons.
		SendMessage(hWndToolbar, TB_ADDBUTTONS, (WPARAM)sizeof(tbButtons)/sizeof(tbButtons[0]), (LPARAM)&tbButtons);
		SendMessage(hWndToolbar, WM_SETFONT, (WPARAM)config.hIdFont, 0);

		if (flags & TWF_SEPARATOR)
		{
			TBBUTTON separator = {SEPARATOR_WIDTH, 0,   0, BTNS_SEP, {0}, 0, 0};
			SendMessage(hWndToolbar, TB_ADDBUTTONS, 1, (LPARAM)&separator);
		}
		AddClassButtons();
		if (flags & TWF_CONFIG_BUTTON)
		{
			TBBUTTON cfg =
				{6, ID_CONFIG,   TBSTATE_ENABLED, TBSTYLE_BUTTON, {0}, 0, (INT_PTR)L"Configure"};
			SendMessage(hWndToolbar, TB_ADDBUTTONS, 1, (LPARAM)&cfg);
		}

		// Just has the close button, don't need to keep a pointer to it around.
		// Only toolbar because I don't know how to make a button that acts right
		// otherwise.
		HWND hWndToolbar2 = CreateToolbar(hWndRebar);
		TBBUTTON tbButtons2[] =
		{
			//{0, ID_TRANSLATE, TBSTATE_ENABLED, BTNS_AUTOSIZE | BTNS_SHOWTEXT | TBSTYLE_BUTTON, {0}, 0, (INT_PTR)windowType},
			{4, ID_CLOSE_TRANSLATION, TBSTATE_ENABLED, TBSTYLE_BUTTON, {0}, 0, 0},
		};
		SendMessage(hWndToolbar2, TB_ADDBUTTONS, (WPARAM)sizeof(tbButtons2)/sizeof(tbButtons2[0]), (LPARAM)&tbButtons2);


		REBARBANDINFO rbBand;
		memset(&rbBand, 0, sizeof(rbBand));
		rbBand.cbSize = sizeof(REBARBANDINFO);
		rbBand.fMask  =
				RBBIM_STYLE       // fStyle is valid.
			| RBBIM_TEXT        // lpText is valid.
			| RBBIM_CHILD       // hwndChild is valid.
			| RBBIM_CHILDSIZE   // child size members are valid.
			| RBBIM_SIZE;       // cx is valid
		rbBand.fStyle = RBBS_NOVERT | RBBS_NOGRIPPER | RBBS_FIXEDSIZE;//RBBS_CHILDEDGE | RBBS_GRIPPERALWAYS;  //RBBS_FIXEDSIZE |

		// Get the height of the toolbar.
		// SendMessage(hWndToolbar, TB_SETBUTTONSIZE, 0, (LPARAM) MAKELONG (21, 21));
		DWORD dwBtnSize = SendMessage(hWndToolbar, TB_GETBUTTONSIZE, 0,0);

		// Set values unique to the band with the toolbar.
		rbBand.lpText = L"";
		rbBand.hwndChild = hWndToolbar;
		rbBand.cyChild = HIWORD(dwBtnSize);
		rbBand.cxMinChild = 70;
		rbBand.cyMinChild = HIWORD(dwBtnSize);
		// The default width is the width of the buttons.
		rbBand.cx = 0;

		// Add the band that has the toolbar.
		SendMessage(hWndRebar, RB_INSERTBAND, (WPARAM)-1, (LPARAM)&rbBand);

		RECT r;
		SendMessage(hWndToolbar2, TB_GETITEMRECT, 0, (LPARAM)&r);
		rbBand.hwndChild = hWndToolbar2;
		rbBand.cxMinChild = r.right;
		rbBand.cx = r.right;
		SendMessage(hWndRebar, RB_INSERTBAND, -1, (LPARAM)&rbBand);
		SendMessage(hWndRebar, RB_SHOWBAND, 0, 1);
		SendMessage(hWndRebar, RB_SHOWBAND, 1, 1);
	}
	PlaceWindows();
	UpdateWindow(hWnd);
}

void TranslationWindow::PlaceWindows()
{
	RECT r, r2;
	GetClientRect(hWnd, &r);
	if (hWndRebar)
	{
		MoveWindow(hWndRebar, 0, 0, r.right, 3, 1);
		GetClientRect(hWndRebar, &r2);
		r.top = r2.bottom;
		r.bottom -= r2.bottom;
	}
	MoveWindow(hWndEdit, 0, r.top, r.right, r.bottom, 1);
}

int TranslationWindow::MakeWindow(int showToolbar, HWND hWndParent)
{
	if (!hWnd)
	{
		this->hWndParent = hWndParent;
		hWnd = CreateWindowEx(WS_EX_STATICEDGE, TRANSLATION_WINDOW_CLASS, L"",
			WS_VISIBLE | WS_CHILD | WS_TABSTOP,
			CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
			hWndParent, 0, ghInst, this);

		if (!(flags & TWF_NO_EDIT))
		{
			hWndEdit = CreateWindowEx(WS_EX_STATICEDGE, MSFTEDIT_CLASS, L"",
				CCS_NODIVIDER  | WS_VSCROLL | ES_AUTOVSCROLL | ES_MULTILINE | WS_VISIBLE | WS_CHILD | WS_TABSTOP,
				CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
				hWnd, 0, ghInst, 0);
			if (!(flags & TWF_RICH_TEXT)) SendMessage(hWndEdit, EM_SETTEXTMODE, TM_PLAINTEXT, 0);
			SendMessage(hWndEdit, EM_SETEVENTMASK, 0, ENM_MOUSEEVENTS);
			SendMessage(hWndEdit, EM_SETLIMITTEXT, -1, 0);
			SetFont();
			UpdateEnabled();
		}
	}
	ShowToolbar(showToolbar);
	return 1;
}

void TranslationWindow::NukeWindow()
{
	DestroyWindow(hWnd);
	hWnd = 0;
	hWndEdit = 0;
	hWndToolbar = 0;
	hWndRebar = 0;


	Halt();
}

void CopyMenuState(HMENU dst, HMENU src)
{
	int i = 0;
	MENUITEMINFOW info;
	MENUITEMINFOW info2;
	memset(&info, 0, sizeof(info));
	memset(&info2, 0, sizeof(info2));
	info2.cbSize = info.cbSize = sizeof(info);
	while (1)
	{
		info.fMask = MIIM_STATE | MIIM_SUBMENU;
		if (!GetMenuItemInfoW(src, i, 1, &info)) break;
		info.fMask = MIIM_STATE;
		SetMenuItemInfoW(dst, i, 1, &info);
		info2.fMask = MIIM_SUBMENU;
		if (info.hSubMenu && GetMenuItemInfoW(dst, i, 1, &info2) && info2.hSubMenu)
			CopyMenuState(info2.hSubMenu, info.hSubMenu);
		i++;
	}
}

TranslationWindow *GetObjectFronHandle(HWND hWnd)
{
	return (TranslationWindow *)GetWindowLongPtr(hWnd, GWLP_USERDATA);
}


LRESULT CALLBACK TranslationWindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	TranslationWindow *window = GetObjectFronHandle(hWnd);
	/*
	if (uMsg ==  WM_NOTIFY)
	{
		NMHDR *hdr = (NMHDR*)lParam;
		if (hdr->code == EN_SAVECLIPBOARD)
		{
			ENSAVECLIPBOARD *en = (ENSAVECLIPBOARD *)hdr;
			en = en;
		}
	}
	//*/
	if (window)
	{
		switch (uMsg)
		{
			case WM_DESTROY:
				window->hWndParent = 0;
				window->hWnd = 0;
				window->hWndEdit = 0;
				window->hWndToolbar = 0;
				window->hWndRebar = 0;
				break;
			case WM_SIZING:
			case WM_SIZE:
				window->PlaceWindows();
				break;
			case WM_MOUSEMOVE:
				return 0;
			case WM_LBUTTONUP:
				ReleaseCapture();
				return 0;
			case WM_ERASEBKGND:
				return 0;
			case WM_CONTEXTMENU:
			{
				if (wParam != (WPARAM) window->hWndEdit) break;
				HMENU hMenu = LoadMenu(ghInst, MAKEINTRESOURCE(IDR_MAIN_MENU));
				if (hMenu)
				{
					HMENU hMenuSub = GetSubMenu(hMenu, 2);
					if (hMenuSub)
					{
						// Doesn't work when frame hidden, as there's no menu.  :(
						HMENU hMenuSubMain = GetSubMenu(GetMenu(GetParent(hWnd)), 2);
						if (hMenuSubMain)
							CopyMenuState(hMenuSub, hMenuSubMain);
						MENUITEMINFO info;
						info.cbSize = sizeof(info);
						info.fMask = MIIM_STRING | MIIM_ID | MIIM_STATE;
						info.cch = 0;
						info.fState = 0;

						if (window->flags & TWF_CONFIG_BUTTON)
						{
							info.dwTypeData = L"Configure";
							info.wID = ID_CONFIG;
							InsertMenuItem(hMenuSub, 0, 1, &info);

							info.fType = MFT_SEPARATOR;
							info.wID = 0;
							info.fMask = MIIM_TYPE;
							InsertMenuItem(hMenuSub, 1, 1, &info);
							info.fMask = MIIM_STRING | MIIM_ID | MIIM_STATE;
						}
						wchar_t className[MAX_PATH];
						if (wParam && wParam == (WPARAM)window->hWndEdit && GetClassName(window->hWndEdit, className, sizeof(className)/sizeof(wchar_t)) && wcsnicmp(className, L"Furigana", 8))
						{
							info.fType = MFT_SEPARATOR;
							info.wID = 0;
							info.fMask = MIIM_TYPE;
							InsertMenuItem(hMenuSub, 0, 1, &info);
							info.fMask = MIIM_STRING | MIIM_ID | MIIM_STATE;

							info.dwTypeData = L"Hex to UTF16";
							info.wID = 10009;
							InsertMenuItem(hMenuSub, 0, 1, &info);

							info.dwTypeData = L"Hex to SJIS";
							info.wID = 10008;
							InsertMenuItem(hMenuSub, 0, 1, &info);

							info.dwTypeData = L"UTF16 to Hex";
							info.wID = 10007;
							InsertMenuItem(hMenuSub, 0, 1, &info);

							info.dwTypeData = L"SJIS to Hex";
							info.wID = 10006;
							InsertMenuItem(hMenuSub, 0, 1, &info);

							info.fType = MFT_SEPARATOR;
							info.wID = 0;
							info.fMask = MIIM_TYPE;
							InsertMenuItem(hMenuSub, 0, 1, &info);
							info.fMask = MIIM_STRING | MIIM_ID | MIIM_STATE;

							info.dwTypeData = L"To &Romaji\tCtrl-R";
							info.wID = 10005;
							InsertMenuItem(hMenuSub, 0, 1, &info);
							info.dwTypeData = L"To &Katakana\tCtrl-K";
							info.wID = 10003;
							InsertMenuItem(hMenuSub, 0, 1, &info);
							info.dwTypeData = L"To &Hiragana\tCtrl-H";
							info.wID = 10004;
							InsertMenuItem(hMenuSub, 0, 1, &info);

							CHARRANGE range;
							SendMessage(window->hWndEdit, EM_EXGETSEL, 0, (LPARAM)&range);
							if (range.cpMin >= range.cpMax)
								info.fState = MFS_DISABLED;

							info.dwTypeData = L"Cu&t";
							info.wID = 10000;
							InsertMenuItem(hMenuSub, 0, 1, &info);

							info.dwTypeData = L"&Copy";
							info.wID = 10001;
							InsertMenuItem(hMenuSub, 1, 1, &info);

							info.fState = 0;

							info.dwTypeData = L"&Paste";
							info.wID = 10002;
							InsertMenuItem(hMenuSub, 2, 1, &info);

							info.fType = MFT_SEPARATOR;
							info.wID = 0;
							info.fMask = MIIM_TYPE;

							InsertMenuItem(hMenuSub, 3, 1, &info);
						}

						POINT p;
						GetCursorPos(&p);
						int res = TrackPopupMenu(hMenuSub, TPM_NONOTIFY|TPM_RETURNCMD, p.x, p.y, 0, hWnd, 0);
						if (res)
							if (res == 10000)
								PostMessage(window->hWndEdit, WM_CUT, 0, 0);
							else if (res == 10001)
								PostMessage(window->hWndEdit, WM_COPY, 0, 0);
							else if (res == 10002)
								PostMessage(window->hWndEdit, WM_PASTE, 0, 0);
							else if (res == 10003)
								PostMessage(hWnd, WM_COMMAND, ID_TO_KATAKANA | (BN_CLICKED<<16), 0);
							else if (res == 10004)
								PostMessage(hWnd, WM_COMMAND, ID_TO_HIRAGANA | (BN_CLICKED<<16), 0);
							else if (res == 10005)
								PostMessage(hWnd, WM_COMMAND, ID_TO_ROMAJI | (BN_CLICKED<<16), 0);
							else if (res == 10006)
								PostMessage(hWnd, WM_COMMAND, ID_SJIS_TO_HEX | (BN_CLICKED<<16), 0);
							else if (res == 10007)
								PostMessage(hWnd, WM_COMMAND, ID_UTF16_TO_HEX | (BN_CLICKED<<16), 0);
							else if (res == 10008)
								PostMessage(hWnd, WM_COMMAND, ID_HEX_TO_SJIS | (BN_CLICKED<<16), 0);
							else if (res == 10009)
								PostMessage(hWnd, WM_COMMAND, ID_HEX_TO_UTF16 | (BN_CLICKED<<16), 0);
							else if (res == ID_CONFIG)
								PostMessage(hWnd, WM_COMMAND, res, 0);
							else
								PostMessage(window->hWndParent, WM_COMMAND, res, 0);
					}
					DestroyMenu(hMenu);
				}
				return 0;
			}
			case WM_NOTIFY:
			{
				NMHDR *hdr = (NMHDR*)lParam;
				if (hdr->hwndFrom == window->hWndToolbar)
				{
					if (hdr->code == NM_LDOWN)
					{
						RECT r, r2;
						POINT pt;
						int i;
						GetCursorPos(&pt);
						GetWindowRect(window->hWndToolbar, &r);
						for (i=0; i<100; i++)
						{
							if (!SendMessage(window->hWndToolbar, TB_GETITEMRECT, i, (LPARAM) & r2)) break;
							if (r.left + r2.left <= pt.x && pt.x < r.left + r2.right &&
								r.top + r2.top <= pt.y && pt.y < r.top + r2.right)
								{
									TBBUTTON tb;
									if (SendMessage(window->hWndToolbar, TB_GETBUTTON, i, (LPARAM)&tb) && tb.fsStyle != BTNS_SEP)
										return 0;
							}
						}
						if (DragDetect(hWnd, pt))
							SendMessage(window->hWndParent, WMA_DRAGSTART, (WPARAM)window, 0);
					}
				}
				break;
			}
			case WM_COMMAND:
			{
				int id = LOWORD(wParam);
				if (id >= ID_TO_HIRAGANA && id <= ID_HEX_TO_UTF16)
				{
					CHARRANGE range;
					SendMessage(window->hWndEdit, EM_EXGETSEL, 0, (LPARAM)&range);
					SendMessage(window->hWndEdit, WM_SETREDRAW, 0, 0);
					int highlight = 1;
					if (range.cpMin >= range.cpMax)
					{
						highlight = 0;
						range.cpMin = 0;
						range.cpMax = -1;
						SendMessage(window->hWndEdit, EM_EXSETSEL, 0, (LPARAM)&range);
						SendMessage(window->hWndEdit, EM_EXGETSEL, 0, (LPARAM)&range);
					}
					if (range.cpMin < range.cpMax)
					{
						int memMul = 1;
						wchar_t *textBuff = (wchar_t*) malloc(sizeof(wchar_t*) * (range.cpMax-range.cpMin+1));
						int len = SendMessage(window->hWndEdit, EM_GETSELTEXT, 0, (LPARAM)textBuff);
						if (len > 0)
						{
							int mul = 1;
							if (id == ID_TO_ROMAJI || id == ID_SJIS_TO_HEX || id == ID_UTF16_TO_HEX)
								mul = 3;
							wchar_t *textBuff2 = (wchar_t*) malloc(sizeof(wchar_t*) * (len*mul + 4));
							if (id == ID_TO_HIRAGANA)
								ToHiragana(textBuff, textBuff2);
							else if (id == ID_TO_KATAKANA)
								ToKatakana(textBuff, textBuff2);
							else if (id == ID_TO_ROMAJI)
								ToRomaji(textBuff, textBuff2);
							else if (id == ID_SJIS_TO_HEX || id == ID_UTF16_TO_HEX)
							{
								int len = wcslen(textBuff) * sizeof(wchar_t);
								if (id == ID_SJIS_TO_HEX)
								{
									len ++;
									char *temp = (char*) malloc(len * sizeof(char));
									len = WideCharToMultiByte(932, 0, textBuff, -1, temp, len, 0, 0) - 1;
									free(textBuff);
									textBuff = (wchar_t*) temp;
								}
								unsigned char *src = (unsigned char *)textBuff;
								for (int i=0; i<len; i++)
									wsprintf(textBuff2 + 2*i, L"%02X", src[i]);
								textBuff2[2*len] = 0;
							}
							else if (id == ID_HEX_TO_SJIS || id == ID_HEX_TO_UTF16)
							{
								int len = wcslen(textBuff);
								char *s = (char*) textBuff2;
								int happy = 1;
								int out = 0;
								for (int i=0; i<len; i++)
								{
									if (iswspace(textBuff[i])) continue;
									wchar_t *end;
									wchar_t c = 0;
									if (i + 2 < len)
									{
										c = textBuff[i+2];
										textBuff[i+2] = 0;
									}
									unsigned int val = wcstoul(textBuff+i, &end, 16);
									textBuff[i+2] = c;
									if (val > 0xFFFF || !end || end-textBuff != i+2)
									{
										happy = 0;
										break;
									}
									s[out++] = val;
									i++;
								}
								if (happy)
								{
									for (int i=0; i<4; i++)
										s[out+i] = 0;
									if (id == ID_HEX_TO_UTF16)
										textBuff2 = (wchar_t *)s;
									else if (id == ID_HEX_TO_SJIS)
									{
										int len = out+1;
										textBuff2 = (wchar_t *) malloc(sizeof(wchar_t) * len);
										len = MultiByteToWideChar(932, 0, s, out+1, textBuff2, len);
										if (len < 0)
										{
											happy = 0;
											free(textBuff2);
										}
									}
								}
								if (!happy)
									wcscpy(textBuff2, textBuff);
							}
							SendMessage(window->hWndEdit, EM_REPLACESEL, 1, (LPARAM)textBuff2);
							if (highlight)
							{
								range.cpMax = range.cpMin + wcslen(textBuff2);
								SendMessage(window->hWndEdit, EM_EXSETSEL, 0, (LPARAM)&range);
							}
							free(textBuff2);
						}
						free(textBuff);
					}
					SendMessage(window->hWndEdit, WM_SETREDRAW, 1, 0);
					InvalidateRect(window->hWndEdit, 0, 0);
					return 0;
				}
				else if (HIWORD(wParam) == BN_CLICKED)
				{
					if (id == ID_TRANSLATE)
					{
						if (window->busy && window->remote)
							window->Halt();
						else SendMessage(hWndSuperMaster, WMA_TRANSLATE, 0, (LPARAM)window);
						return 0;
					}
					else if (id == ID_AUTO_CLIPBOARD)
					{
						int w = SendMessage(window->hWndToolbar, TB_GETSTATE, ID_AUTO_CLIPBOARD, 0);
						window->autoClipboard = ((w & TBSTATE_CHECKED) != 0);
						SaveConfig();
						return 0;
					}
					else if (id == ID_CLOSE_TRANSLATION)
					{
						SendMessage(window->hWndParent, WM_COMMAND, window->menuIndex, 0);
						return 0;
					}
				}
				if (!lParam && HIWORD(wParam) == 1)
					SendMessage(window->hWndParent, uMsg, wParam, lParam);
				break;
			}
			default:
				break;
		}
		LRESULT out = 0;
		if (window->WndProc(&out, uMsg, wParam, lParam))
			return out;
	}
	else if (uMsg == WM_CREATE)
	{
		CREATESTRUCT *cs = (CREATESTRUCT *)lParam;
		SetWindowLongPtr(hWnd, GWLP_USERDATA, (ULONG_PTR)cs->lpCreateParams);
	}
	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

int TranslationWindow::WndProc (LRESULT *output, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	return 0;
}

void TranslationWindow::Stopped()
{
	if (busy)
	{
		busy = 0;
		Animate();
	}
}

void TranslationWindow::Animate()
{
	int icon = 0;
	if (busy)
	{
		busy = (busy)%7+1;
		icon = busy + NUM_ICONS-8;
	}
	TBBUTTONINFO info =
	{
		sizeof(info), TBIF_IMAGE,
		ID_TRANSLATE_ALL, icon, 0, 0, 0, 0, 0, 0
	};
	SendMessage(hWndToolbar, TB_SETBUTTONINFO, ID_TRANSLATE, (LPARAM)&info);
	RECT r;
	SendMessage(hWndToolbar, TB_GETITEMRECT, 0, (LPARAM)&r);
	RedrawWindow(hWndToolbar, &r, 0, RDW_ERASE|RDW_INVALIDATE);
}

void TranslationWindow::Translate(SharedString *orig, SharedString *&mod, int history_id, bool only_use_history)
{
	if (!hWndEdit) return;
	Translate(mod, history_id, only_use_history);
}

void TranslationWindow::UpdateEnabled()
{
	if (!hWndEdit) return;
	int canTrans = CanTranslate(config.langSrc, config.langDst);
	EnableWindow(hWndEdit, canTrans);
}
