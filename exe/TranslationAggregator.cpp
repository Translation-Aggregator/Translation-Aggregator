// Handles main windows and basically everything that involves
// communication between multipl subwindows.
// #include <vld.h>
/*
#define _CRTDBG_MAPALLOC
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
//*/

#include <Shared/Shrink.h>
#include "util/Dictionary.h"
#include "TranslationWindows/TranslationWindow.h"
#include "TranslationWindows/TranslationWindowFactory.h"
#ifdef SETSUMI_CHANGES
#include "TranslationWindows/LocalWindows/FuriganaWindow.h"
#endif

#include "Config.h"
#include "util/Injector.h"

#include <Shared/DllInjection.h>
#include <Shared/Atlas.h>

#include "Dialogs/MyToolTip.h"

#include "Dialogs/InjectionDialog.h"

#include "resource.h"

#include "Context.h"
#include "BufferedSocket.h"

// Needed for launching dictionary search.
#include "TranslationWindows/LocalWindows/AtlasWindow.h"

#include "exe/History/History.h"

#define TIMER_INTERVAL  200


// The light blue dragging window.  Keep it around so it can handle my timers
// and monitoring the clipboard when hidden, and other windows can come and
// go at will.  Basically handles all translation task management tasks.
// "MasterWindows" handle tasks related primarily to GUI stuff for each main
// window, and TranslationWindows handle the translating and display of text.
HWND hWndSuperMaster;

#define ID_VIEWS_OFFSET (ID_VIEW_TWOCOLUMN + 2000)
#define ID_WEBSITES_OFFSET (ID_VIEW_TWOCOLUMN + 2100)
#define ID_SRC_LANGUAGE_OFFSET (ID_VIEW_TWOCOLUMN + 2200)
#define ID_DST_LANGUAGE_OFFSET (ID_VIEW_TWOCOLUMN + 2300)

TranslationWindow *srcWindow;
TranslationWindow **windows;
int numWindows;

HINSTANCE ghInst;

struct HotKeyInfo
{
	UINT modifiers;
	UINT virtual_key;
	// Command the hotkey is equivalent to, so no need to duplicate message handling code.
	UINT command;
};

const HotKeyInfo hotkey_info[] =
{
	{ MOD_ALT | MOD_CONTROL, VK_UP, ID_HISTORY_BACK },
	{ MOD_ALT | MOD_CONTROL, VK_PRIOR, ID_HISTORY_BACK },
	{ MOD_ALT | MOD_CONTROL, VK_DOWN, ID_HISTORY_FORWARD },
	{ MOD_ALT | MOD_CONTROL, VK_NEXT, ID_HISTORY_FORWARD },
	{ MOD_ALT | MOD_CONTROL, 'A', ID_TOPMOST },
	{ MOD_ALT | MOD_CONTROL, VK_SUBTRACT, ID_VIEW_LESSSOLID },
	{ MOD_ALT | MOD_CONTROL, VK_ADD, ID_VIEW_MORESOLID },
	//{ MOD_ALT | MOD_CONTROL, 'L', ID_LOCK_CURSOR },
};

void UnregisterHotKeys(HWND window)
{
	for (int i = 0; i < sizeof(hotkey_info)/sizeof(hotkey_info[0]); ++i)
		UnregisterHotKey(window, i);
}

void RegisterHotKeys(HWND window)
{
	for (int i = 0; i < sizeof(hotkey_info)/sizeof(hotkey_info[0]); ++i)
		RegisterHotKey(window, i, hotkey_info[i].modifiers, hotkey_info[i].virtual_key);
}

UINT GetCommandForHotKey(WPARAM wParam, LPARAM lParam)
{
	UINT virtual_key = HIWORD(lParam);
	UINT mofifiers = LOWORD(lParam);
	for (int i = 0; i < sizeof(hotkey_info)/sizeof(hotkey_info[0]); ++i)
	{
		if (hotkey_info[i].virtual_key == virtual_key && hotkey_info[i].modifiers == mofifiers)
			return hotkey_info[i].command;
	}
	return 0;
}

int CommandLineInject()
{
	int res = 0;
	int argc;
	wchar_t * command = GetCommandLineW();
	wchar_t **argv = CommandLineToArgvW(command, &argc);
	if (argv)
	{
		for (int i=1; i<argc; i++)
		{
			if (argv[i][0] == '/')
			{
				if (argv[i][1] == 'I' && argv[i][2] == ':')
				{
					Inject(argv[i]+3);
					res++;
				}
			}
		}
		LocalFree(argv);
	}
	return res;
}

SharedString *GetSourceText()
{
	if (!srcWindow->hWndEdit) return 0;
	int len2 = GetWindowTextLengthW(srcWindow->hWndEdit);
	SharedString *out = CreateSharedString(len2+1);
	out->length = GetWindowTextW(srcWindow->hWndEdit, out->string, len2+1);
	if (!out->length)
	{
		out->Release();
		return 0;
	}
	SpiffyUp(out->string);
	out->length = wcslen(out->string);
	return out;
}

SharedString *ProcessSubstitutions(SharedString *orig)
{
	SharedString *mod;
	// Modify original, for simplicity.
	if (config.autoHalfWidthToFull)
	{
		LCID lcidJap = MAKELCID(MAKELANGID(LANG_JAPANESE, SUBLANG_JAPANESE_JAPAN), SORT_JAPANESE_XJIS);
		for (int i=0; i<orig->length; i++)
		{
			wchar_t c = orig->string[i];
			if (c >= 0xFF61 && c <= 0xFFEF)
				LCMapStringW(lcidJap, LCMAP_FULLWIDTH, &c, 1, &orig->string[i], 1);
		}
	}
	mod = orig;

	if ((config.flags & CONFIG_ENABLE_AUTO_HIRAGANA) && config.langSrc == LANGUAGE_Japanese && !HasJap(orig->string))
	{
		SharedString *temp = CreateSharedString(mod->length);
		int len = ToHiragana(mod->string, temp->string);
		if (wcsijcmp(mod->string, temp->string))
		{
			ResizeSharedString(temp, len);
			mod = temp;
		}
		else
			free(temp);
	}

	wchar_t **activeLists = 0;
	int numActiveLists = -1;
	if ((config.flags & CONFIG_ENABLE_SUBSTITUTIONS) && config.replace.numLists)
		config.replace.FindActive();
	else
		numActiveLists = 0;

	if (config.numPlugins)
	{
		wchar_t *string = mod->string;
		int changed = 0;
		TAPluginFreeType *PluginFree = 0;
		for (int i=0; i<config.numPlugins; i++)
		{
			if (config.plugins[i].ActiveProfileList)
			{
				if (numActiveLists == -1)
				{
					numActiveLists = 0;
					for (int i=0; i<config.replace.numLists; i++)
					{
						if (config.replace.lists[i].active) numActiveLists++;
						else break;
					}
					if (numActiveLists)
					{
						activeLists = (wchar_t**) malloc(sizeof(wchar_t*) * numActiveLists);
						for (int i=0; i<config.replace.numLists; i++)
						{
							if (config.replace.lists[i].active) activeLists[i] = config.replace.lists[i].profile;
							else break;
						}
					}
				}
				config.plugins[i].ActiveProfileList(numActiveLists, (const wchar_t **)activeLists);
			}
			if (config.plugins[i].ModifyStringPreSubstitution)
			{
				wchar_t *res = config.plugins[i].ModifyStringPreSubstitution(string);
				if (res)
				{
					if (!changed && mod != orig) free(mod);
					else if (PluginFree) PluginFree(string);
					PluginFree = config.plugins[i].Free;
					string = res;
					changed = 1;
				}
			}
		}
		if (changed)
		{
			mod = CreateSharedString(wcslen(string));
			wcscpy(mod->string, string);
			if (PluginFree) PluginFree(string);
		}
	}

	if ((config.flags & CONFIG_ENABLE_SUBSTITUTIONS) && config.replace.numLists)
	{
		SharedString *current = CreateSharedString(mod->length);
		int po = 0, pm = 0;
		int changed = 0;
		int currentLen = mod->length;
		while (po<mod->length)
		{
			ReplaceString *replace = config.replace.FindReplacement(mod->string+po);
			if (!replace)
				current->string[pm++] = mod->string[po++];
			else
			{
				int L1 = wcslen(replace->old);
				int L2 = wcslen(replace->rep);
				currentLen += L2 - L1;
				if (currentLen >= current->length)
					ResizeSharedString(current, currentLen + 5000);
				changed = 1;
				wcscpy(current->string + pm, replace->rep);
				pm += L2;
				po += L1;
			}
		}
		ResizeSharedString(current, currentLen);
		if (!changed)
			free(current);
		else
		{
			if (mod != orig)
				free(mod);
			mod = current;
		}
	}

	if (config.numPlugins)
	{
		wchar_t *string = mod->string;
		int changed = 0;
		TAPluginFreeType *PluginFree = 0;
		for (int i=0; i<config.numPlugins; i++)
		{
			if (config.plugins[i].ModifyStringPostSubstitution)
			{
				wchar_t *res = config.plugins[i].ModifyStringPostSubstitution(string);
				if (res)
				{
					if (!changed && mod != orig) free(mod);
					else if (PluginFree) PluginFree(string);
					PluginFree = config.plugins[i].Free;
					string = res;
					changed = 1;
				}
			}
		}
		if (changed)
		{
			mod = CreateSharedString(wcslen(string));
			wcscpy(mod->string, string);
			if (PluginFree) PluginFree(string);
		}
	}

	if (mod == orig)
		orig->AddRef();
	if (activeLists) free(activeLists);
	return mod;
}

void TranslateAll()
{
	SharedString *text = GetSourceText();
	if (!text) return;
	int id = history.AddEntry(text->string);
	SharedString *mod = ProcessSubstitutions(text);
	for (int i=0; i<numWindows; i++)
		if (windows[i]->hWnd)
			windows[i]->Translate(text, mod, id, false);
	mod->Release();
	text->Release();
}

void Translate(TranslationWindow *w)
{
	SharedString *text = GetSourceText();
	if (!text) return;
	int id = history.AddEntry(text->string);
	SharedString *mod = ProcessSubstitutions(text);
	w->Translate(text, mod, id, false);
	text->Release();
	mod->Release();
}

HWND nextClipboardListener = 0;

SharedString *GetClipboard(int spiffyUp)
{
	SharedString * string = 0;
	if (IsClipboardFormatAvailable(CF_UNICODETEXT))
	{
		for (int retry = 0; retry < 10; retry++)
			if (OpenClipboard(hWndSuperMaster))
			{
				HANDLE hGlobal = GetClipboardData(CF_UNICODETEXT);
				if (hGlobal)
				{
					wchar_t *data = (wchar_t*)GlobalLock(hGlobal);
					if (data)
					{
						int w = (int) GlobalSize(data)/2;
						int len = 0;
						while (len < w && data[len]) len++;
						if (len && len != w)
						{
							string = CreateSharedString(len);
							wcscpy(string->string, data);
							if (spiffyUp) SpiffyUp(string->string);
							string->length = wcslen(string->string);
						}
						GlobalUnlock(data);
					}
				}
				CloseClipboard();
				break;
			}
			else
				Sleep(15);
	}
	return string;
}

void HistoryTranslate(SharedString *string)
{
	if (string->length)
	{
		int id = history.AddEntry(string->string);
		int hasJap = HasJap(string->string);
		SharedString *mod = ProcessSubstitutions(string);
		if (mod->length)
		{
			for (int i=0; i<numWindows; i++)
			{
				if (!windows[i]->hWnd)
					continue;
				windows[i]->Translate(string, mod, id, true);
			}
			// Do last, because it's so incredibly slow, might as well start everything first.
			if (srcWindow->hWndEdit)
				SetWindowTextW(srcWindow->hWndEdit, string->string);
		}
		mod->Release();
	}
	string->Release();
}

void AutoTranslate(SharedString *string, bool replace_untrans)
{
	if (string->length)
	{
		int id = history.AddEntry(string->string);
		int hasJap = HasJap(string->string);
		SharedString *mod = ProcessSubstitutions(string);
		if (mod->length)
		{
			for (int i=0; i<numWindows; i++)
			{
				if (!windows[i]->hWnd || !windows[i]->autoClipboard)
					continue;
				if (!windows[i]->remote)
					windows[i]->Translate(string, mod, id, false);
				else if (string->length >= 500)
					SetWindowTextA(windows[i]->hWndEdit, "String over 500 characters, not automatically sent.");
				else
					// Queue normally.
					windows[i]->Translate(string, mod, id, false);
			}
			// Do last, because it's so incredibly slow, might as well start everything first.
			if (replace_untrans && srcWindow->hWndEdit)
				SetWindowTextW(srcWindow->hWndEdit, string->string);
		}
		mod->Release();
	}
	string->Release();
}

static int DragRow = 0;
static int DragCol = 0;

static char dragging = 0;

void SetCursor()
{
	const wchar_t *cursors[] = {IDC_ARROW, IDC_SIZEWE, IDC_SIZENS, IDC_SIZEALL};
	SetCursor(LoadCursor(0, cursors[(DragCol != 0) + 2*(DragRow != 0)]));
}

BOOL CALLBACK PlaceWindowFromMonitorEnum(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData)
{
	RECT *rects = (RECT*) dwData;
	if (rects[0].left+20 < lprcMonitor->left || rects[0].left >= lprcMonitor->right-20 ||
		rects[0].top+20 < lprcMonitor->top || rects[0].top >= lprcMonitor->bottom-20)
			return 1;
	int dx = GetSystemMetrics(SM_CXSIZEFRAME);
	int dy = GetSystemMetrics(SM_CYSIZEFRAME);
	rects[1] = rects[0];
	if (rects[1].left < lprcMonitor->left - dx || rects[1].left >= lprcMonitor->right)
		rects[1].left = lprcMonitor->left;
	if (rects[1].top < lprcMonitor->top - dy || rects[1].top >= lprcMonitor->bottom)
		rects[1].top = lprcMonitor->top;
	if (rects[1].left + rects[1].right > lprcMonitor->right + dx)
	{
		if (rects[1].right < lprcMonitor->right - lprcMonitor->left)
			rects[1].right = lprcMonitor->right - lprcMonitor->left;
		rects[1].left = lprcMonitor->right - rects[1].right;
	}
	if (rects[1].top + rects[1].bottom > lprcMonitor->bottom + dy)
	{
		if (rects[1].bottom < lprcMonitor->bottom - lprcMonitor->top)
			rects[1].bottom = lprcMonitor->bottom - lprcMonitor->top;
		rects[1].top = lprcMonitor->bottom - rects[1].bottom;
	}
	return 0;
}

struct MasterWindow
{
	static int numMasterWindows;
	static MasterWindow *masterWindows[20];
	HMENU hFromMenu, hToMenu;

	static MasterWindow *GetMasterWindow(HWND hWnd)
	{
		for (int i=0; i<numMasterWindows; i++)
		{
			if (masterWindows[i]->hWnd == hWnd)
				return masterWindows[i];
		}
		return 0;
	}

	HWND hWnd;
	int numChildren;
	int numRows;
	int colPlacement;
	unsigned char lockWindows;
	unsigned char numCols;
	unsigned char topmost;
	unsigned char showWindowFrame;
#ifdef SETSUMI_CHANGES
	unsigned char borderlessWindow;
#endif
	unsigned char showToolbars;
	unsigned char alpha;
	TranslationWindow *children[20];
	int rowPlacements[20];
	RECT windowPlacements[20];

	void Destroy()
	{
		for (int i=0; i<numMasterWindows; i++)
		{
			if (masterWindows[i] == this)
			{
				DestroyWindow(masterWindows[i]->hWnd);
				masterWindows[i] = masterWindows[--numMasterWindows];
				free(this);
				// to update the clipboard listener.
				if (!i && numMasterWindows) masterWindows[i]->LayoutWindows();
				break;
			}
		}
	}

	void SetToolbars(int val)
	{
		showToolbars = val;
		for (int i=0; i<numChildren; i++)
			children[i]->ShowToolbar(showToolbars);
		SetChecks();
	}

	void SetTopmost(int val)
	{
		topmost = val;
		HWND top = HWND_NOTOPMOST;
		if (topmost) top = HWND_TOPMOST;
		SetWindowPos(hWnd, top, 0, 0, 0, 0, SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE);
		SetChecks();
	}

	void SetNumCols(int val)
	{
		if (val < 1 || val > 2) val = 2;
		numCols = val;
		SetChecks();
		LayoutWindows();
	}

	void SetAlpha(int val)
	{
		if (val < 20) val = 20;
		else if (val > 255) val = 255;
		alpha = (unsigned char) val;
		LONG_PTR style = GetWindowLongPtr(hWnd, GWL_EXSTYLE);
		if (alpha != 255)
			style |= WS_EX_LAYERED;
		else
			style &= ~WS_EX_LAYERED;
		SetWindowLongPtr(hWnd, GWL_EXSTYLE, style);
		if (alpha)
			SetLayeredWindowAttributes(hWnd, 0, alpha, LWA_ALPHA);
	}

	void MakeWindow()
	{
		DWORD styleEx = WS_EX_COMPOSITED;
		DWORD style = WS_CLIPSIBLINGS;
#ifdef SETSUMI_CHANGES
		//hack - borderless window
		if (borderlessWindow) {
			showWindowFrame = 0;
			style |= WS_POPUP;
		} else {
			if (showWindowFrame)
				style |= WS_OVERLAPPEDWINDOW;
			else
				style |= WS_POPUP | WS_THICKFRAME;
		}
#else
		if (showWindowFrame)
			style |= WS_OVERLAPPEDWINDOW;
		else
			style |= WS_POPUP | WS_THICKFRAME;
#endif
		HWND hWndOld = hWnd;
		RECT r;
		if (hWnd)
			GetWindowRect(hWnd, &r);
		else
		{
			r.left = r.top = CW_USEDEFAULT;
			r.right = r.bottom = (int)(2*(__int64)CW_USEDEFAULT);
		}
		hWnd = CreateWindowEx(styleEx, MAIN_WINDOW_CLASS, MAIN_WINDOW_TITLE, style, r.left, r.top, r.right-r.left, r.bottom-r.top, 0, 0, ghInst, 0);
		if (hWnd)
		{
			DragAcceptFiles(hWnd, 1);
			SetAlpha(alpha);
			for (int i=0; i<numChildren; i++)
				children[i]->SetWindowParent(hWnd);
			SetWindowLongPtr(hWnd, GWLP_USERDATA, (ULONG_PTR)this);
			SetTopmost(topmost);
			HMENU hMenu = GetMenu(hWnd);
			if (hMenu)
			{
				if (!showWindowFrame)
				{
					SetMenu(hWnd, 0);
					DestroyMenu(hMenu);
				}
				else
				{
					HMENU hMenu2 = GetSubMenu(hMenu, 3);
					if (hMenu2)
					{
						for (int i=0; i<numWindows; i++)
						{
							windows[i]->id = i;
							windows[i]->menuIndex = i + ID_VIEWS_OFFSET;
							AppendMenu(hMenu2, MF_STRING | MF_ENABLED, windows[i]->menuIndex, windows[i]->windowType);
						}
					}
					hMenu2 = CreatePopupMenu();
					if (hMenu2)
					{
						AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hMenu2, L"Web&sites");
						for (int i=0; i<numWindows; i++)
							if (windows[i]->srcUrl && windows[i]->srcUrl[0])
								AppendMenu(hMenu2, MF_STRING | MF_ENABLED, i + ID_WEBSITES_OFFSET, windows[i]->windowType);
					}
					hFromMenu = CreatePopupMenu();
					hToMenu = CreatePopupMenu();
					if (hFromMenu || hToMenu)
					{
						hMenu2 = GetSubMenu(hMenu, 1);
						for (int i=0; i<LANGUAGE_NONE; i++)
						{
							wchar_t *text = GetLanguageString((Language) i);
							if (hFromMenu) AppendMenu(hFromMenu, MF_STRING | MF_ENABLED, i + ID_SRC_LANGUAGE_OFFSET, text);
							if (hToMenu && i != LANGUAGE_AUTO) AppendMenu(hToMenu, MF_STRING | MF_ENABLED, i + ID_DST_LANGUAGE_OFFSET, text);
							if (i == LANGUAGE_Japanese || i == LANGUAGE_Russian || i == LANGUAGE_AUTO)
							{
								AppendMenu(hFromMenu, MF_SEPARATOR, 0, 0);
								if (i != LANGUAGE_AUTO) AppendMenu(hToMenu, MF_SEPARATOR, 0, 0);
							}
						}
						AppendMenuW(hMenu2, MF_SEPARATOR, 0, 0);
						if (hFromMenu) AppendMenuW(hMenu2, MF_POPUP, (UINT_PTR)hFromMenu, L"Translate &From");
						if (hToMenu) AppendMenuW(hMenu2, MF_POPUP, (UINT_PTR)hToMenu, L"Translate &To");
					}
					SetChecks();
				}
			}
			if (hWndOld)
			{
				ShowWindow(hWnd, 1);
				SetWindowLongPtr(hWndOld, GWLP_USERDATA, 0);
				DestroyWindow(hWndOld);
			}
		}
	}

#ifdef SETSUMI_CHANGES
	void SetWindowFrame(int val, int bord = 0)
#else
	void SetWindowFrame(int val)
#endif
	{
		showWindowFrame = val;
#ifdef SETSUMI_CHANGES
		borderlessWindow = bord;
#endif
		MakeWindow();
		SetChecks();
	}

	int RemoveChild(TranslationWindow *w, int nukeWindow = 0)
	{
		for (int i=0; i<numWindows; i++)
		{
			if (children[i] == w)
			{
				if (nukeWindow) w->NukeWindow();
				memmove(children+i, children+i+1, sizeof(TranslationWindow*) * (numChildren-i-1));
				numChildren--;
				break;
			}
		}
		if (!numChildren)
		{
			Destroy();
			return 0;
		}
		else
		{
			SetChecks();
			LayoutWindows();
		}
		return 1;
	}

	void LayoutWindows()
	{
		// Simplest way to make sure I always have a clipboard listener.
		RECT r;

		GetClientRect(hWnd, &r);
		if (r.right - r.left <= 10 || r.bottom-r.top <= 10) return;

		// +3 makes first/last rows work out simpler.
		int height = r.bottom - r.top+3;
		int width = r.right - r.left;

		int rows = numChildren;
		int cols = numCols;

		int middle = colPlacement;
		if (cols == 2)
			rows = (1+numChildren)/2;
		else
			cols = numCols = 1;

		// Overflow's unlikely, but better safe than sorry.
		middle = (int) (middle * (__int64)width / RANGE_MAX);
		if (middle < 3) middle = 3;
		if (middle > width-4) middle = width-4;

		for (int m=0; m<10; m++)
		{
			int top = -1;
			int row;
			for (row=0; row<rows; row++)
			{
				if (rows != numRows)
					rowPlacements[row] = (int) (RANGE_MAX * (__int64)(row+1) / rows);
				if (rowPlacements[row] < top)
					rowPlacements[row] = top;
				if (rowPlacements[row] > RANGE_MAX) rowPlacements[row] = RANGE_MAX;

				int oldTop = top;
				top = (int)((rowPlacements[row] * (__int64)height/RANGE_MAX));
				if (top-oldTop<6)
				{
					if (oldTop > 1 && oldTop > 6 * (row+1))
					{
						oldTop-=2;
						rowPlacements[row-1] = (int) (oldTop * (__int64) RANGE_MAX / height);
					}
					if (top < height-2)
					{
						top += 3;
						if (top < 6 * (row+1)) top = 6*(row+1);
						rowPlacements[row] = (int) ((top * (__int64) RANGE_MAX) / height);
					}
				}
			}
			numRows = rows;
		}
		int p = 0;
		int row;
		int top = 0;
		for (row=0; row<rows; row++)
		{
			windowPlacements[p].left = 0;
			windowPlacements[p].top = top;
			top = (int)((rowPlacements[row] * (__int64)height/RANGE_MAX));
			windowPlacements[p].bottom = top - 3;
			if (cols == 1 || (!row && (numChildren&1)))
			{
				windowPlacements[p].right = width;
				p++;
			}
			else
			{
				windowPlacements[p].right = middle-1;
				windowPlacements[p+1].left = middle+2;
				windowPlacements[p+1].right = width;
				windowPlacements[p+1].top = windowPlacements[p].top;
				windowPlacements[p+1].bottom = windowPlacements[p].bottom;
				p+=2;
			}
		}
		p = 0;
		int w = 0;
		HDWP hDwp = BeginDeferWindowPos(numChildren);
		for (w=0; w<numChildren; w++)
		{
			// Minor optimization: Only move windows that need it.  Doesn't help much.
			RECT r;
			GetWindowRect(children[w]->hWnd, &r);
			MapWindowPoints(0, hWnd, (POINT*)&r, 2);
			if (r.left != windowPlacements[p].left ||
				r.right != windowPlacements[p].right ||
				r.top != windowPlacements[p].top ||
				r.bottom != windowPlacements[p].bottom)
				{
					if (hDwp)
					{
						hDwp = DeferWindowPos(hDwp, children[w]->hWnd, 0,
							windowPlacements[p].left,
							windowPlacements[p].top,
							windowPlacements[p].right - windowPlacements[p].left,
							windowPlacements[p].bottom - windowPlacements[p].top,
							SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
					}
			}
			p++;
		}
		if (hDwp) EndDeferWindowPos(hDwp);
	}

	void AddChild(TranslationWindow *w, int index = -1)
	{
		if (!w->hWnd)
			w->MakeWindow(showToolbars, hWnd);
		else
		{
			HWND hWndOldParent = GetParent(w->hWnd);
			if (hWndOldParent == hWnd) return;
			w->SetWindowParent(hWnd);
			GetMasterWindow(hWndOldParent)->RemoveChild(w);
			w->ShowToolbar(showToolbars);
		}
		if (index == -1)
			children[numChildren] = w;
		else
		{
			memmove(children+index+1, children+index, sizeof(TranslationWindow*)*(numChildren-index));
			children[index] = w;
		}
		numChildren++;
		LayoutWindows();
		SetChecks();
	}


#ifdef SETSUMI_CHANGES
	static MasterWindow *CreateMasterWindow(TranslationWindow *w, int numCols=2, int colPlacement=RANGE_MAX/2, int topmost = 0, int frame = 1, int toolbars = 1, int alpha = 255, int lockWindows = 0, int borderless = 0)
#else
	static MasterWindow *CreateMasterWindow(TranslationWindow *w, int numCols=2, int colPlacement=RANGE_MAX/2, int topmost = 0, int frame = 1, int toolbars = 1, int alpha = 255, int lockWindows = 0)
#endif
	{
		if (colPlacement < 0 || colPlacement > RANGE_MAX) colPlacement = RANGE_MAX/2;
		MasterWindow *master = (MasterWindow*) calloc(1, sizeof(MasterWindow));

		master->lockWindows = lockWindows;
		master->alpha = (unsigned char) alpha;
		master->numCols = 1 + (1!=numCols);
		master->colPlacement = colPlacement;
		master->topmost = topmost;
		master->showWindowFrame = frame;
#ifdef SETSUMI_CHANGES
		master->borderlessWindow = borderless;
#endif
		master->showToolbars = toolbars;

		master->MakeWindow();
		master->AddChild(w);
		masterWindows[numMasterWindows++] = master;
		return master;
	}


	static int LoadAndCreateWindows()
	{
		numWindows = MakeTranslationWindows(windows, srcWindow);
		LoadLayout(0);
		return 1;
	}

	struct SearchResult
	{
		POINT pt;
		MasterWindow *master;
		MasterWindow *exclude;
	};

	static BOOL CALLBACK FindInsertionWindow(HWND hWnd, LPARAM lParam)
	{
		SearchResult *search = (SearchResult*) lParam;
		RECT rect;
		GetWindowRect(hWnd, &rect);
		if (rect.left <= search->pt.x && search->pt.x <= rect.right &&
			rect.top <= search->pt.y && search->pt.y <= rect.bottom &&
			IsWindowVisible(hWnd) && IsWindowEnabled(hWnd))
			{
				search->master = GetMasterWindow(hWnd);
				if (search->master != search->exclude &&
					hWnd != hWndSuperMaster)
						return 0;
		}
		return 1;
	}

	MasterWindow *FindInsertionRect(int *index, RECT *r)
	{
		SearchResult search;
		GetCursorPos(&search.pt);
		search.exclude = this;
		search.master = 0;
		EnumWindows(MasterWindow::FindInsertionWindow, (LPARAM) &search);
		if (search.master)
		{
			if (search.master->lockWindows) return 0;
			MasterWindow *m = search.master;
			int i;
			for (i=0; i<m->numChildren; i++)
			{
				RECT r2;
				GetWindowRect(m->children[i]->hWnd, &r2);
				if (r2.left <= search.pt.x &&
					r2.right >= search.pt.x &&
					r2.top <= search.pt.y &&
					r2.bottom >= search.pt.y)
					{
						*r = r2;
						index[0] = i;
						if (m->numCols == 2)
						{
							if (!(m->numChildren&1))
							{
								if (i<=1)
								{
									RECT r3, r4;
									GetWindowRect(m->children[0]->hWnd, &r3);
									GetWindowRect(m->children[1]->hWnd, &r4);
									int mid = (r3.top + r3.bottom)/2;
									if (search.pt.y < mid)
									{
										r->left = r3.left;
										r->right = r4.right;
										r->bottom = mid;
										index[0] = 0;
									}
									else
									{
										r->top = mid;
										index[0]++;
									}
								}
								else
									index[0]++;
							}
							else
							{
								if (i)
									index[0] ++;
								else
								{
									int width = r2.right - r2.left;
									int middle = r2.left + (int) (m->colPlacement * (__int64)width / RANGE_MAX);
									if (search.pt.x < middle)
										r->right = middle;
									else
									{
										r->left = middle;
										index[0]++;
									}
								}
							}
						}
						else
							if (i >= m->numChildren/2) index[0]++;
						break;
				}
			}
			if (i == m->numChildren) return 0;
		}
		return search.master;
	}

	static void SetChecksAllWindows()
	{
		for (int i=0; i<numMasterWindows; i++)
			masterWindows[i]->SetChecks();
	}

	void SetChecks()
	{
		HMENU hMainMenu = GetMenu(hWnd);
		if (hMainMenu)
		{
			HMENU hMenu;
			UINT check;
			if (hMenu = GetSubMenu(hMainMenu, 0))
			{
				check = MF_UNCHECKED;
				if (config.global_hotkeys) check = MF_CHECKED;
				CheckMenuItem(hMenu, ID_FILE_GLOBALHOTKEYS, MF_BYCOMMAND | check);
			}

			if (hMenu = GetSubMenu(hMainMenu, 1))
			{
				check = MF_UNCHECKED;
				if (config.autoHalfWidthToFull) check = MF_CHECKED;
				CheckMenuItem(hMenu, ID_TOOLS_AUTOCONVERT, MF_BYCOMMAND | check);

				check = MF_UNCHECKED;
				if (config.flags & CONFIG_ENABLE_SUBSTITUTIONS) check = MF_CHECKED;
				CheckMenuItem(hMenu, ID_TOOLS_ENABLESUBSTITUTIONS, MF_BYCOMMAND | check);

				check = MF_UNCHECKED;
				if (config.flags & CONFIG_ENABLE_AUTO_HIRAGANA) check = MF_CHECKED;
				CheckMenuItem(hMenu, ID_TOOLS_AUTOCONVERT_HIRAGANA, MF_BYCOMMAND | check);
			}
			if (hMenu = GetSubMenu(hMainMenu, 2))
			{
				check = MF_UNCHECKED;
				if (!showToolbars) check = MF_CHECKED;
				CheckMenuItem(hMenu, ID_VIEW_HIDETOOLBAR, MF_BYCOMMAND | check);

				check = MF_UNCHECKED;
				if (topmost) check = MF_CHECKED;
				CheckMenuItem(hMenu, ID_TOPMOST, MF_BYCOMMAND | check);
			}
			if (hMenu = GetSubMenu(hMainMenu, 3))
			{
				check = MF_UNCHECKED;
				if (numCols == 2) check = MF_CHECKED;
				CheckMenuItem(hMenu, ID_VIEW_TWOCOLUMN, MF_BYCOMMAND | check);

				check = MF_UNCHECKED;
				if (lockWindows) check = MF_CHECKED;
				CheckMenuItem(hMenu, ID_VIEW_LOCKORDER, MF_BYCOMMAND | check);

				for (int i=0; i<numWindows; i++)
				{
					UINT check = MF_UNCHECKED;
					if (windows[i]->hWndParent == hWnd) check = MF_CHECKED;
					// if (windows[i]->hWnd) check = MF_CHECKED;
					CheckMenuItem(hMenu, windows[i]->menuIndex, MF_BYCOMMAND | check);
				}
			}
			for (int i=0; i<LANGUAGE_NONE; i++)
			{
				UINT check1 = MF_UNCHECKED;
				if (config.langSrc == i) check1 = MF_CHECKED;
				UINT check2 = MF_UNCHECKED;
				if (config.langDst == i) check2 = MF_CHECKED;
				if (hFromMenu) CheckMenuItem(hFromMenu,  i+ID_SRC_LANGUAGE_OFFSET, MF_BYCOMMAND | check1);
				if (hToMenu) CheckMenuItem(hToMenu, i+ID_DST_LANGUAGE_OFFSET, MF_BYCOMMAND | check2);
			}
		}
	}

	static void SaveLayout(int id)
	{
		wchar_t prefix[40];
		wsprintf(prefix, L"Layout %i", id);
		if (numMasterWindows)
		{
			int i;
			WritePrivateProfileInt(prefix, L"Count", numMasterWindows, config.ini);
			for (i=0; i<numMasterWindows; i++)
			{
				wchar_t temp[6000];
				wchar_t temp2[40];
				wchar_t *end = temp;
				*end = 0;
				MasterWindow *win = masterWindows[i];
#ifdef SETSUMI_CHANGES
				FuriganaWindow *pjparser = NULL, *pmecab = NULL; //hack - save JParser and Mecab background color into layout
#endif
				for (int j=0; j<win->numChildren; j++)
				{
					if (j)
						wcscat(end, L",");
					wcscat(end, win->children[j]->windowType);
					end = wcschr(end, 0);

#ifdef SETSUMI_CHANGES
					if (!wcsnicmp(L"JParser", win->children[j]->windowType, wcslen(win->children[j]->windowType)))
						pjparser = (FuriganaWindow*)win->children[j];
					else if (!wcsnicmp(L"Mecab", win->children[j]->windowType, wcslen(win->children[j]->windowType)))
						pmecab = (FuriganaWindow*)win->children[j];
#endif
				}
				RECT r;
				GetWindowRect(win->hWnd, &r);
#ifdef SETSUMI_CHANGES
				wsprintf(end, L"; %i, %i, %i, %i, %i, %i, %i, %i, %i, %i, %i, %i, %i, %i, %i, %i, %i, %i, %i, %i; ", win->numCols, win->topmost, win->showWindowFrame, win->showToolbars, win->alpha, win->colPlacement, r.left, r.top, r.right-r.left, r.bottom-r.top, win->lockWindows, win->borderlessWindow,
					pjparser?pjparser->colors[0]:0, pjparser?pjparser->colors[1]:0,
					pjparser?pjparser->colors[2]:0, pjparser?pjparser->colors[3]:0,
					pmecab?pmecab->colors[0]:0, pmecab?pmecab->colors[1]:0,
					pmecab?pmecab->colors[2]:0, pmecab?pmecab->colors[3]:0);
#else
				wsprintf(end, L"; %i, %i, %i, %i, %i, %i, %i, %i, %i, %i, %i; ", win->numCols, win->topmost, win->showWindowFrame, win->showToolbars, win->alpha, win->colPlacement, r.left, r.top, r.right-r.left, r.bottom-r.top, win->lockWindows);
#endif
				for (int j=0; j<win->numRows; j++)
				{
					if (j)
						wcscat(end, L", ");
					end = wcschr(end, 0);
					wsprintf(end, L"%i", win->rowPlacements[j]);
				}
				wsprintf(temp2, L"Window %i", i);
				WritePrivateProfileStringW(prefix, temp2, temp, config.ini);
			}
		}
	}

	static void LoadLayout(int id)
	{
		int numMasterWindowsOld = numMasterWindows;
		MasterWindow *masterWindowsOld[20];
		for (int i=0; i<numMasterWindows; i++)
		{
			masterWindowsOld[i] = masterWindows[i];
			ShowWindow(masterWindows[i]->hWnd, 0);
		}
		int changed = 0;
		wchar_t prefix[40];
		wsprintf(prefix, L"Layout %i", id);
		int count = GetPrivateProfileInt(prefix, L"Count", 0, config.ini);
		if (count > 30) count = 0;
		for (int i=0; i<count; i++)
		{
			wchar_t temp[6000];
			wchar_t temp2[40];
			wsprintf(temp2, L"Window %i", i);
			if (!GetPrivateProfileStringW(prefix, temp2, L"", temp, sizeof(temp)/sizeof(temp[0]), config.ini))
				continue;
			wchar_t *windowString = mywcstok(temp, L";");
			if (!windowString) continue;
			wchar_t *valString = mywcstok(0, L";");
			if (!valString) continue;
			wchar_t *placement = mywcstok(0, L";");
			if (!placement) continue;

			int numVals = 0;
			int vals[100];
			wchar_t *string = mywcstok(valString, L", ");
			do
				vals[numVals++] = _wtoi(string);
			while ((string = mywcstok(0, L", ")) && numVals < 100);


			wchar_t *name = mywcstok(windowString, L",");
			MasterWindow *master = 0;
			bool OriginalTextPresent = false;
			do
			{
				TranslationWindow *win = 0;
				for (int j=0; j<numWindows; j++)
				{
					if (!wcsicmp(windows[j]->windowType, name))
					{
						win = windows[j];
#ifdef SETSUMI_CHANGES
						//hack - load JParser and Mecab background color from layout
						if (!wcsnicmp(L"JParser", name, wcslen(name))) {
							((FuriganaWindow*)win)->colors[0] = vals[12];
							((FuriganaWindow*)win)->colors[1] = vals[13];
							((FuriganaWindow*)win)->colors[2] = vals[14];
							((FuriganaWindow*)win)->colors[3] = vals[15];
						} else if (!wcsnicmp(L"Mecab", name, wcslen(name))) {
							((FuriganaWindow*)win)->colors[0] = vals[16];
							((FuriganaWindow*)win)->colors[1] = vals[17];
							((FuriganaWindow*)win)->colors[2] = vals[18];
							((FuriganaWindow*)win)->colors[3] = vals[19];
						}
#endif
						if (!j)
							OriginalTextPresent = true;
						break;
					}
				}
				if (win)
				{
					if (!master)
#ifdef SETSUMI_CHANGES
						master = MasterWindow::CreateMasterWindow(win, vals[0], vals[5], vals[1], vals[2], vals[3], vals[4], vals[10], vals[11]);
#else
						master = MasterWindow::CreateMasterWindow(win, vals[0], vals[5], vals[1], vals[2], vals[3], vals[4], vals[10]);
#endif
					else
						master->AddChild(win);
				}
			}
			while (name = mywcstok(0, L","));
			if (!master) continue;
			changed = 1;

			// Not real rects - use height and width instead of bottom/right.
			RECT rects[2] = {{vals[6], vals[7], vals[8], vals[9]}, {0,0,0,0}};
#ifdef SETSUMI_CHANGES
			//hack - no reposition if beyond screen
			rects[1] = rects[0];
#else
			if (rects[0].right < 50) rects[0].right = 50;
			if (rects[0].bottom < 50) rects[0].bottom = 50;
			EnumDisplayMonitors(0, 0, PlaceWindowFromMonitorEnum, (LPARAM) &rects);
			if (rects[1].left == rects[1].right)
			{
				rects[0].left = rects[0].top = 0;
				EnumDisplayMonitors(0, 0, PlaceWindowFromMonitorEnum, (LPARAM) &rects);
				if (rects[1].left == rects[1].right)
				{
					rects[1].top = rects[1].left = 0;
					rects[1].bottom = rects[1].right = 400;
				}
			}
#endif
			MoveWindow(master->hWnd, rects[1].left, rects[1].top, rects[1].right, rects[1].bottom, 0);

			int index = 0;
			string = mywcstok(placement, L", ");
			do
				master->rowPlacements[index++] = _wtoi(string);
			while ((string = mywcstok(0, L", ")) && index < numWindows);
			master->numRows = index;
			master->LayoutWindows();
			if (OriginalTextPresent && !numMasterWindowsOld)
				SetFocus(windows[0]->hWndEdit);
		}
		if (!changed)
		{
			MasterWindow *master = MasterWindow::CreateMasterWindow(windows[0]);
			for (int i=1; i<numWindows; i++)
				master->AddChild(windows[i]);
		}
		for (int i=0; i<numMasterWindows; i++)
		{
			int w;
			for (w=0; w<numMasterWindowsOld; w++)
			{
				if (masterWindowsOld[w] == masterWindows[i])
					break;
			}
			if (w < numMasterWindowsOld)
			{
				while (masterWindows[i]->RemoveChild(masterWindows[i]->children[0]));
				i--;
				continue;
			}
			ShowWindow(masterWindows[i]->hWnd, 1);
		}
	}
};

int MasterWindow::numMasterWindows = 0;
MasterWindow *MasterWindow::masterWindows[20];

int timerActive = 0;

LRESULT CALLBACK TwigDragWindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch(uMsg)
	{
		case WMA_SOCKET_MESSAGE:
			HandleSocketMessage((SOCKET)wParam, LOWORD(lParam), HIWORD(lParam));
			// Don't actually have to do for all socket messages.
			// Connect/disconnect/error/don't have a full message yet don't need it.
			// but doesn't really hurt.
			if (!timerActive)
			{
				SetTimer(hWnd, 1, TIMER_INTERVAL, 0);
				timerActive = 1;
			}
			return 0;
		case WM_CLOSE:
			DestroyWindow(hWnd);
			break;
		case WM_CREATE:
			hWndSuperMaster = hWnd;
			TryStartListen(0);
			nextClipboardListener = SetClipboardViewer(hWnd);
			// Must be done after start listening.
			CommandLineInject();
			break;
		case WM_TIMER:
			if (wParam == 1)
			{
				int done = 1;
				for (int i=0; i<numWindows; i++)
				{
					if (windows[i]->busy)
					{
						windows[i]->Animate();
						done = 0;
					}
				}
				if (!Contexts.CheckDone())
					done = 0;
				if (done)
				{
					timerActive = 0;
					KillTimer(hWnd, 1);
				}
				return 0;
			}
			break;
		case WM_HOTKEY:
		{
			UINT command = GetCommandForHotKey(wParam, lParam);
			if (command && MasterWindow::numMasterWindows > 0)
			{
				PostMessage(MasterWindow::masterWindows[0]->hWnd, WM_COMMAND, command, 0);
				return 0;
			}
			break;
		}
		case WM_DESTROY:
			ChangeClipboardChain(hWnd, nextClipboardListener);
			nextClipboardListener = 0;
			KillTimer(hWnd, 1);
			CleanupSockets();
			PostQuitMessage(0);
			break;
		case WM_CHANGECBCHAIN:
			if ((HWND)wParam == nextClipboardListener)
				nextClipboardListener = (HWND)lParam;
			else if (nextClipboardListener)
				SendMessage(nextClipboardListener, WM_CHANGECBCHAIN, wParam, lParam);
			return 0;
		case WMA_TRANSLATE_HIGHLIGHTED:
			if (srcWindow->hWndEdit)
			{
				DWORD start, end;
				SendMessage(srcWindow->hWndEdit, EM_GETSEL, (WPARAM)&start, (WPARAM)&end);
				if (start != end)
				{
					if (start > end)
					{
						DWORD temp = end;
						end = start;
						start = temp;
					}
					SharedString *s = CreateSharedString(end-start);
					s->length = SendMessage(srcWindow->hWndEdit, EM_GETSELTEXT, 0, (LPARAM)s->string);
					AutoTranslate(s, false);
				}
			}
			return 0;
		case WMA_AUTO_COPY:
		{
			SharedString *s = GetClipboard(0);
			if (s)
			{
				if (s->length && s->length < 10000)
				{
					HGLOBAL hGlobal;
					hGlobal = GlobalAlloc(GMEM_DDESHARE | GMEM_MOVEABLE, sizeof(wchar_t)*(s->length+1));
					if (hGlobal)
					{
						void *data = GlobalLock(hGlobal);
						if (data)
						{
							memcpy(data, s->string, (s->length+1) * sizeof(wchar_t));
							GlobalUnlock(hGlobal);
							if (OpenClipboard(hWnd))
							{
								EmptyClipboard();
								SetClipboardData(CF_UNICODETEXT, hGlobal);
								CloseClipboard();
							}
							else data = 0;
						}
						if (!data)
							GlobalFree(hGlobal);
					}
				}
				s->Release();
			}
			return 0;
		}
		case WMA_AUTO_TRANSLATE_CONTEXT:
			if (srcWindow->autoClipboard)
			{
				SharedString *string = (SharedString *)wParam;
				AutoTranslate(string, true);
				if (!timerActive)
				{
					SetTimer(hWnd, 1, TIMER_INTERVAL, 0);
					PostMessage(hWnd, WM_TIMER, 1, 0);
					timerActive = 1;
				}
				return 0;
			}
		case WMA_AUTO_TRANSLATE_CLIPBOARD:
			if (srcWindow->autoClipboard)
			{
				SharedString *string;
				// Getting the string here seems to work better.
				if (string = GetClipboard(1))
				{
					Contexts.AddText(L"::System", 0, L"Clipboard", string->string);
					string->Release();
					/*AutoTranslate(string, 1, 1);
					//*/
					if (!timerActive)
					{
						SetTimer(hWnd, 1, TIMER_INTERVAL, 0);
						PostMessage(hWnd, WM_TIMER, 1, 0);
						timerActive = 1;
					}
				}
			}
			return 0;
		case WMA_BACK:
		case WMA_FORWARD:
		{
			TranslationHistoryEntry* entry;
			if (uMsg == WMA_BACK)
				entry = history.Back();
			else
				entry = history.Forward();
			if (entry)
			{
				SharedString *string = CreateSharedString(entry->original_string.c_str(), entry->original_string.length());
				HistoryTranslate(string);
				if (!timerActive)
				{
					SetTimer(hWnd, 1, TIMER_INTERVAL, 0);
					PostMessage(hWnd, WM_TIMER, 1, 0);
					timerActive = 1;
				}
			}
			return 0;
		}
		case WM_DRAWCLIPBOARD:
		{
			if (nextClipboardListener) SendMessage(nextClipboardListener, uMsg, wParam, lParam);
			int sameAncestor = 0;
			HWND hWndOwner = GetClipboardOwner();
			DWORD pid = 0;
			if (GetWindowThreadProcessId(hWndOwner, &pid) && pid == GetCurrentProcessId())
				return 0;
			MasterWindow *master = MasterWindow::GetMasterWindow(GetAncestor(hWndOwner, GA_ROOT));
			// If source is one of the editors, take control of clipboard later.
			// Note:  Under no circumstances can I read the clipboard in this function
			// at that case, or else I get a crash under 32-bit OSes.
			if (master)
				PostMessage(hWnd, WMA_AUTO_COPY, 0, 0);
			else
				PostMessage(hWnd, WMA_AUTO_TRANSLATE_CLIPBOARD, 0, 0);
			return 0;
		}
		case WMA_TRANSLATE_ALL:
			TranslateAll();
			if (!timerActive)
			{
				SetTimer(hWnd, 1, 300, 0);
				PostMessage(hWnd, WM_TIMER, 1, 0);
				timerActive = 1;
			}
			return 0;
		case WMA_TRANSLATE:
			Translate((TranslationWindow*)lParam);
			SetTimer(hWnd, 1, 300, 0);
			PostMessage(hWnd, WM_TIMER, 1, 0);
			timerActive = 1;
			return 0;
		default:
			break;
	}
	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

void UpdateEnabledTranslationWindows()
{
	for (int i=0; i<numWindows; i++)
		windows[i]->UpdateEnabled();
}

LRESULT CALLBACK TwigMainWindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	MasterWindow *win = (MasterWindow*) GetWindowLongPtr(hWnd, GWLP_USERDATA);
	if (win)
	{
		switch(uMsg)
		{
#ifdef SETSUMI_CHANGES
			//hack - splitter color (main window background)
			case WM_PAINT:
				{
					PAINTSTRUCT ps;
					HDC hdc = BeginPaint(hWnd, &ps);

					HBRUSH brush = CreateSolidBrush(RGB(150,150,150));
					HBRUSH oldbrush = (HBRUSH)SelectObject(hdc, brush);
					FillRect(hdc, &ps.rcPaint, brush);
					SelectObject(hdc, oldbrush);
					DeleteObject(brush);

					EndPaint(hWnd, &ps);
				}
				break;
#endif
			case WM_CLOSE:
				MasterWindow::SaveLayout(0);
				while (win->RemoveChild(win->children[0]));
				SetWindowLongPtr(hWnd, GWLP_USERDATA, 0);
				DestroyWindow(hWnd);
				// Close is called by user, destroy by system.
				// Can end up with 0 listed windows by destroy but
				// not want to quit.  Can't with close.
				if (!MasterWindow::numMasterWindows)
					DestroyWindow(hWndSuperMaster);
				break;
			case WM_SIZE:
			case WM_SIZING:
				win->LayoutWindows();
				break;
			case WMA_DRAGSTART:
				if (win->lockWindows) break;
				if (win->numChildren > 1)
				{
					TranslationWindow *dragging = (TranslationWindow*)wParam;
					RECT rect, rect2, rect3;
					GetClientRect(dragging->hWnd, &rect);
					ClientToScreen(dragging->hWnd, (POINT*)&rect);
					ClientToScreen(dragging->hWnd, ((POINT*)&rect)+1);
					MasterWindow *master = MasterWindow::CreateMasterWindow(dragging);
					if (master)
					{
						// Lovely way to have to calculate this...MS is evil.
						GetClientRect(master->hWnd, &rect2);
						ClientToScreen(master->hWnd, (POINT*)&rect2);
						ClientToScreen(master->hWnd, ((POINT*)&rect2)+1);
						GetWindowRect(master->hWnd, &rect3);
						rect.left -= rect2.left - rect3.left;
						rect.right += rect3.right - rect2.right;
						rect.top -= rect2.top - rect3.top;
						rect.bottom += rect3.bottom - rect2.bottom;
						MoveWindow(master->hWnd, rect.left, rect.top, rect.right-rect.left, rect.bottom-rect.top, 0);
						ShowWindow(master->hWnd, 1);
						PostMessage(master->hWnd, WM_SYSCOMMAND, HTCAPTION+SC_MOVE, 0);
						//DefWindowProc(master->hWnd, WM_SYSCOMMAND, HTCAPTION+SC_MOVE, 0);
						TwigMainWindowProc(master->hWnd, WM_MOVING, 0, 0);
					}
					return 0;
				}
				DefWindowProc(win->hWnd, WM_SYSCOMMAND, HTCAPTION+SC_MOVE, 0);
				// Don't want to break here.
			case WM_ENTERSIZEMOVE:
				SetWindowPos(hWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOACTIVATE| SWP_NOMOVE | SWP_NOSIZE);
				break;
			case WM_MOVING:
				if (!win->lockWindows)
				{
					int index;
					RECT r, r2;
					MasterWindow *master = win->FindInsertionRect(&index, &r);
					if (!master)
						ShowWindow(hWndSuperMaster, 0);
					else
					{
						if (!IsWindowVisible(hWndSuperMaster))
						{
							SetWindowPos(hWndSuperMaster, GetWindow(master->hWnd, GW_HWNDPREV), r.left, r.top, r.right-r.left, r.bottom-r.top, SWP_SHOWWINDOW | SWP_NOACTIVATE);
							ShowWindow(hWndSuperMaster, 1);
						}
						GetWindowRect(hWndSuperMaster, &r2);
						if (memcmp(&r, &r2, sizeof(r)))
						{
							// No clue why both are needed.  Second alone tends to mess up Z-order.
							// First alone doesn't always seem to move the window, even when I remove the NO_MOVE flags.
							SetWindowPos(hWndSuperMaster, GetWindow(master->hWnd, GW_HWNDPREV), r.left, r.top, r.right-r.left, r.bottom-r.top, SWP_SHOWWINDOW | SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOREDRAW | SWP_NOSIZE);
							MoveWindow(hWndSuperMaster, r.left, r.top, r.right-r.left, r.bottom-r.top, 1);
						}
					}
				}
				return 0;
			case WM_EXITSIZEMOVE:
				win->SetTopmost(win->topmost);
				if (IsWindowVisible(hWndSuperMaster))
				{
					ShowWindow(hWndSuperMaster, 0);
					int index;
					RECT r;
					MasterWindow *master = win->FindInsertionRect(&index, &r);
					if (master)
					{
						for (int i=win->numChildren-1; i>=0; i--)
							master->AddChild(win->children[i], index);
					}
				}
				break;
			case WM_COMMAND:
				// From menu or accelerator.
				if ((unsigned int) HIWORD(wParam) <= 1)
				{
					int cmd = LOWORD(wParam);
					if (cmd == ID_FILE_QUIT)
					{
						MasterWindow::SaveLayout(0);
						DestroyWindow(hWndSuperMaster);
						return 0;
					}
					else if (cmd == ID_HISTORY_BACK)
					{
						PostMessage(hWndSuperMaster, WMA_BACK, 0, 0);
						return 0;
					}
					else if (cmd == ID_HISTORY_FORWARD)
					{
						PostMessage(hWndSuperMaster, WMA_FORWARD, 0, 0);
						return 0;
					}
					else if (cmd == ID_TOPMOST)
					{
						win->SetTopmost(!win->topmost);
						return 0;
					}
					else if (cmd == ID_TOOLS_OPTIONS)
					{
						ConfigDialog(hWnd);
						return 0;
					}
					else if (cmd == ID_TOOLS_MANAGECONTEXTS)
					{
						Contexts.Configure(hWnd);
						return 0;
					}
					else if (cmd == ID_FILE_GLOBALHOTKEYS)
					{
						config.global_hotkeys = !config.global_hotkeys;
						if (config.global_hotkeys)
							RegisterHotKeys(hWndSuperMaster);
						else
							UnregisterHotKeys(hWndSuperMaster);
						MasterWindow::SetChecksAllWindows();
						SaveConfig();
						return 0;
					}
					else if (cmd == ID_TOOLS_AUTOCONVERT)
					{
						history.ClearTranslations();
						config.autoHalfWidthToFull = !config.autoHalfWidthToFull;
						MasterWindow::SetChecksAllWindows();
						SaveConfig();
						return 0;
					}
					else if (cmd == ID_TOOLS_ENABLESUBSTITUTIONS)
					{
						history.ClearTranslations();
						config.flags ^= CONFIG_ENABLE_SUBSTITUTIONS;
						MasterWindow::SetChecksAllWindows();
						SaveConfig();
						return 0;
					}
					else if (cmd == ID_TOOLS_AUTOCONVERT_HIRAGANA)
					{
						history.ClearTranslations();
						config.flags ^= CONFIG_ENABLE_AUTO_HIRAGANA;
						MasterWindow::SetChecksAllWindows();
						SaveConfig();
						return 0;
					}
					else if (cmd == ID_VIEW_LESSSOLID || cmd == ID_VIEW_MORESOLID)
					{
						int i = ((int)win->alpha) + 16*(2*(cmd - ID_VIEW_LESSSOLID)-1);
						win->SetAlpha(i);
						return 0;
					}
					else if (cmd == ID_VIEW_HIDEWINDOWFRAME)
					{
						win->SetWindowFrame(!win->showWindowFrame);
						// Need to set config.windowOrder to match.  If do it in ShowWindowFrame(),
						// bad things may happen on init.  Really, really nasty to do it like this,
						// but it works.
						return 0;
					}
#ifdef SETSUMI_CHANGES
					else if (cmd == ID_BORDERLESS_WINDOW) { //hack - borderless window menu click
						win->SetWindowFrame(win->showWindowFrame, !win->borderlessWindow);
						return 0;
					}
					else if (cmd == ID_VIEW_RESETPLACEMENT) { //hack - reset TA window position
						SetWindowPos(hWnd, HWND_TOPMOST, 100, 100, 0, 0, SWP_NOSIZE);
						return 0;
					}
#endif
					else if (cmd == ID_VIEW_TWOCOLUMN)
					{
						win->SetNumCols(win->numCols%2 + 1);
						return 0;
					}
					else if (cmd == ID_VIEW_HIDETOOLBAR)
					{
						win->SetToolbars(!win->showToolbars);
						return 0;
					}
					else if (cmd == ID_VIEW_LOCKORDER)
					{
						win->lockWindows = !win->lockWindows;
						win->SetChecks();
						return 0;
					}
					else if (cmd == ID_VIEW_SHOWALL)
					{
						for (int i=0; i<numWindows; i++)
						{
							if (!windows[i]->hWnd)
								win->AddChild(windows[i]);
						}
						return 0;
					}
					else if (ID_BIND_LAYOUT_0 <= cmd && cmd <= ID_BIND_LAYOUT_9)
					{
						MasterWindow::SaveLayout(cmd - ID_BIND_LAYOUT_0 + 1);
						return 0;
					}
					else if (ID_LAYOUT_0 <= cmd && cmd <= ID_LAYOUT_9)
					{
						MasterWindow::LoadLayout(cmd - ID_LAYOUT_0 + 1);
						return 0;
					}
					else if (ID_VIEWS_OFFSET <= cmd && cmd <= ID_VIEWS_OFFSET+numWindows)
					{
						for (int i=0; i<numWindows; i++)
						{
							if (windows[i]->menuIndex == cmd)
							{
								if (windows[i]->hWndParent == hWnd)
									win->RemoveChild(windows[i], 1);
								else
									win->AddChild(windows[i]);
								break;
							}
						}
						return 0;
					}
					else if (ID_WEBSITES_OFFSET <= cmd && cmd <= ID_WEBSITES_OFFSET+numWindows)
					{
						int id = cmd - ID_WEBSITES_OFFSET;
						for (int i=0; i<numWindows; i++)
						{
							if (windows[i]->id == id)
							{
								wchar_t temp[1000];
								wsprintf(temp, L"url.dll,FileProtocolHandler %s", windows[i]->srcUrl);
								ShellExecute(hWnd, L"open", L"rundll32.exe", temp, 0, SW_SHOWNORMAL);
								break;
							}
						}
						return 0;
					}
					else if (ID_SRC_LANGUAGE_OFFSET <= cmd && cmd < ID_SRC_LANGUAGE_OFFSET + LANGUAGE_NONE)
					{
						Language src = (Language) (cmd - ID_SRC_LANGUAGE_OFFSET);
						if (src == config.langDst) config.langDst = config.langSrc;
						config.langSrc = src;
						MasterWindow::SetChecksAllWindows();
						UpdateEnabledTranslationWindows();
						history.ClearTranslations();
					}
					else if (ID_DST_LANGUAGE_OFFSET <= cmd && cmd < ID_DST_LANGUAGE_OFFSET + LANGUAGE_NONE)
					{
						Language dst = (Language) (cmd - ID_DST_LANGUAGE_OFFSET);
						if (dst == config.langSrc) config.langSrc = config.langDst;
						config.langDst = dst;
						MasterWindow::SetChecksAllWindows();
						UpdateEnabledTranslationWindows();
						history.ClearTranslations();
					}
					else if (cmd == ID_FILE_INJECTINTOPROCESS)
					{
						InjectionDialog(hWnd);
						return 0;
					}
					else if (cmd == ID_VIEW_FONT)
					{
						if (MyChooseFont(hWnd, &config.font))
						{
							SaveConfig();
							for (int i=0; i<numWindows; i++)
								windows[i]->SetFont();
						}
						return 0;
					}
					else if (cmd == ID_VIEW_TOOLTIP_FONT)
					{
						if (MyChooseFont(hWnd, &config.toolTipFont))
							SaveConfig();
						return 0;
					}
				}
				break;
			case WM_DROPFILES:
				TryDragDrop(hWnd, (HDROP) wParam);
				return 0;
			case WM_LBUTTONDOWN:
				SetCapture(hWnd);
				dragging = 1;
				SetCursor();
				return 0;
			case WM_MOUSEMOVE:
			{
				int x = (short int)(lParam);
				int y = (short int)(lParam>>16);
				if (dragging)
				{
					if (!(MK_LBUTTON&wParam))
					{
						ReleaseCapture();
						dragging = 0;
					}
					else
					{
						RECT r;
						GetClientRect(hWnd, &r);
						if (!r.right || !r.bottom) return 0;
						if (DragCol)
						{
							win->colPlacement = (int)((x+1) * (__int64)RANGE_MAX / r.right);
							if (win->colPlacement<0) win->colPlacement = 0;
							else if (win->colPlacement > RANGE_MAX) win->colPlacement = RANGE_MAX;
						}
						if (DragRow)
						{
							int v = (int)((y+1) * (__int64)RANGE_MAX / r.bottom);
							if (v < 0)
								v = 0;
							if (v > RANGE_MAX)
								v = RANGE_MAX;
							if (DragRow>1 && win->rowPlacements[DragRow-2] > v) v = win->rowPlacements[DragRow-2];
							if (DragRow<win->numRows && win->rowPlacements[DragRow] < v) v = win->rowPlacements[DragRow];
							win->rowPlacements[DragRow-1] = v;
						}
						win->LayoutWindows();
					}
				}
				else
				{
					DragCol = DragRow = 0;
					if (win->numCols == 2 && win->numChildren > 1 && (x <= win->windowPlacements[win->numChildren-1].left+22 && x >=  win->windowPlacements[win->numChildren-1].left-3-22))
						DragCol = 1;
					int row = 0;
					int dist = 23;
					for (int i=0; i<win->numChildren - win->numCols; i+= win->numCols)
					{
						if (y > win->windowPlacements[i].bottom-dist && y <= win->windowPlacements[i].bottom+3+dist)
						{
							dist = abs(y - win->windowPlacements[i].bottom-2);
							DragRow = 1+row;
						}
						row++;
					}
				}
				SetCursor();
				return 0;
			}
			case WM_LBUTTONDBLCLK:
				if (DragCol)
					win->colPlacement = RANGE_MAX/2;
				if (DragRow)
					win->numRows = -1;
				win->LayoutWindows();
				SetCursor();
				return 0;
			case WM_LBUTTONUP:
				ReleaseCapture();
			case WM_ACTIVATE:
				dragging = 0;
				break;
			default:
				break;
		}
	}
	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

int Register()
{
	LoadGeneralConfig();
	// Hey...That's their name, not mine.  I just changed
	// the capitalization...
	INITCOMMONCONTROLSEX controlSex;
	controlSex.dwSize = sizeof(controlSex);
	// I really don't care.
	controlSex.dwICC = ICC_STANDARD_CLASSES | ICC_WIN95_CLASSES;
	InitCommonControlsEx(&controlSex);

	// Needed for the rich edit to work.
	CoInitializeEx(0, COINIT_APARTMENTTHREADED);
	if (!(LoadLibraryA("Msftedit.dll"))) return 0;
	//controls.dwSize = sizeof(controls);
	//controls.dwICC = ICC_LISTVIEW_CLASSES |
	WNDCLASS TwigMainWindow =
	{
		CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS,
		TwigMainWindowProc,
		0, 0,
		ghInst,
		LoadIcon(ghInst, MAKEINTRESOURCE(IDI_ICON1)),
		LoadCursor(NULL, IDC_ARROW),
		(HBRUSH)(COLOR_3DFACE+1),
		MAKEINTRESOURCE(IDR_MAIN_MENU),
		MAIN_WINDOW_CLASS
	};
	WNDCLASS TranslationWindowClass =
	{
		CS_HREDRAW | CS_VREDRAW,
		TranslationWindowProc,
		0, 0,
		ghInst,
		0,
		LoadCursor(NULL, IDC_ARROW),
		(HBRUSH)(COLOR_3DFACE+1),
		0,
		TRANSLATION_WINDOW_CLASS
	};
	WNDCLASS TwigDragWindowClass =
	{
		CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS,
		TwigDragWindowProc,
		0, 0,
		ghInst,
		0,
		0,
		CreateSolidBrush(RGB(160,170,235)),
		0,
		DRAG_WINDOW_CLASS
	};
	if (!RegisterClass(&TwigDragWindowClass) || !RegisterClass(&TwigMainWindow) || !RegisterClass(&TranslationWindowClass)) return 0;
	return 1;
}

void Unregister()
{
	int i;
	CleanupConfig();
	for (i=0; i<numWindows; i++)
		delete windows[i];
	free(windows);

	UnregisterClass(MAIN_WINDOW_CLASS, ghInst);
	UnregisterClass(TRANSLATION_WINDOW_CLASS, ghInst);
	UnregisterClass(DRAG_WINDOW_CLASS, ghInst);
}

void SaveConfig()
{
	SaveGeneralConfig();
	for (int i=0; i<numWindows; i++)
		windows[i]->SaveWindowTypeConfig();
}

#ifdef UNICODE
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow)
#else
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
#endif
{
#ifdef SETSUMI_CHANGES
	SetErrorMode(SetErrorMode(0) | SEM_NOGPFAULTERRORBOX);
#endif
	WSADATA wsaData;
	int winsockHappy = !WSAStartup(0x202, &wsaData);
	ghInst = hInstance;
	// Could do better, but good enough, for now.
	HANDLE hMapping = CreateFileMapping(INVALID_HANDLE_VALUE, 0, PAGE_READWRITE, 0, 2, MAIN_WINDOW_TITLE);
	int alreadyRunning = 0;
	if (hMapping)
	{
		CloseHandle(hMapping);
		if (GetLastError() == ERROR_ALREADY_EXISTS)
			alreadyRunning = 1;
	}
	{
		wchar_t * command = GetCommandLineW();
		int argc;
		wchar_t **argv = CommandLineToArgvW(command, &argc);
		if (argv)
		{
			int quit = 0;
			// Was thinking of adding a couple other ATLAS options, but decided
			// not to.
			if (argc >= 2 && !wcscmp(argv[1], L"/ATLAS:DICTSEARCH"))
			{
				quit = 1;
				LaunchAtlasDictSearch();
				UninitAtlas();
			}
			LocalFree(argv);
			if (!quit && argc >= 2)
			{
				if (alreadyRunning)
					quit += CommandLineInject();
			}
			if (quit)
				return 0;
		}
	}

	if (!Register())
	{
		Unregister();
		return 1;
	}

	//int styleEx = WS_EX_COMPOSITED;
	//if (config.topmost) styleEx |= WS_EX_TOPMOST;

	if (!MasterWindow::LoadAndCreateWindows() ||
		!CreateWindowEx(WS_EX_TOOLWINDOW|WS_EX_LAYERED|WS_EX_COMPOSITED, DRAG_WINDOW_CLASS, L"", WS_POPUP, 0, 0, 10, 10, 0, 0, ghInst, 0))
		return 1;
	SetLayeredWindowAttributes(hWndSuperMaster, 0, 0x80, LWA_ALPHA);

	MSG msg;

	HACCEL hAccel = LoadAccelerators(ghInst, MAKEINTRESOURCE(IDR_ACCELERATORS));

#ifdef GOAT
	wchar_t *text;
	int size;
	LoadFile(L"dictionaries\\cjdict.txt", &text, &size);
	int counts[2][1000] = {{0}};
	wchar_t *line = wcstok(text, L"\r\n");
	int ct = 0;
	while (line)
	{
		if (ct % 5000 == 0)
			printf("%8i\n", ct);
		ct ++;
		if (line[0] != '\t')
		{
			wchar_t *s = wcschr(line, '\t');
			if (s)
			{
				*s = 0;
				int num = _wtoi(s+1);
				if (num > 999) num = 999;
				Match *matches;
				int numMatches;
				FindExactMatches(line, wcslen(line), matches, numMatches);
				if (!numMatches)
					matches = matches;
				else
				{
					int p = -1;
					for (int i=0; i<numMatches; i++)
					{
						if (/*matches[i].conj[0].verbType ||*/ matches[i].inexactMatch) continue;
						if (matches[i].japFlags & JAP_WORD_COMMON)
						{
							p = 1;
							break;
						}
						else
							p = 0;
					}
					free(matches);
					if (p >= 0)
						counts[p][num]++;
				}
			}
		}
		line = wcstok(0, L"\r\n");
	}
	FILE *out = fopen("_temp.txt", "wb");
	for (int i=0; i<1000; i++)
	{
		if (counts[0][i] || counts[1][i])
			fprintf(out, "%4i\t%4i\t%4i\n", i, counts[0][i], counts[1][i]);
	}
	fclose(out);
	exit(0);
#endif
	if (config.global_hotkeys)
		RegisterHotKeys(hWndSuperMaster);

	while (GetMessage(&msg, 0, 0, 0) > 0)
	{
		// This just lets me use a single accelerator table for MainWindows and
		// the TranslationWindows.
		int munch = 0;
		int w = 0;
		for (w = numWindows-1; w>=0; w--)
		{
			if (windows[w]->hWnd == msg.hwnd || windows[w]->hWndEdit == msg.hwnd)
			{
				munch = TranslateAccelerator(windows[w]->hWnd, hAccel, &msg);
				break;
			}
		}
		if (munch)
			continue;
		if (w < 0 && TranslateAccelerator(GetAncestor(msg.hwnd, GA_ROOTOWNER), hAccel, &msg))
			continue;
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	// Cleanup old stuff from ini.
	// DeleteFileW(config.ini);
	// Also saves config.
	Unregister();

	CleanupDicts();

	if (winsockHappy)
		WSACleanup();
	return 0;
}
