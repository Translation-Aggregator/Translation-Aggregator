#define INITGUID
#include <Shared/Shrink.h>
#include "InjectionDialog.h"
#include "../Config.h"
#include <Shared/DllInjection.h>
#include "../TranslationWindows/LocalWindows/AtlasWindow.h"
#include "../util/Injector.h"
#include <shobjidl.h>
#include <shlguid.h>

#include "../resource.h"

#include <Shared/TextHookParser.h>
#include <Shared/HookEval.h>

static InjectionSettings *cfg;

// Used for locale callbacks.  Only valid for very short periods.  Not threadsafe.
static HWND gCBLocale;
static wchar_t *gSeekLocaleName;
static LCID gFoundLCID;

void GetConfigFromDialog(HWND hWnd, std::wstring* injectionHooks);

static void UpdateEnabledControls(HWND hWnd)
{
	int enableExePath = 1;
	int enableProcessList = 1;
	if (BST_CHECKED == IsDlgButtonChecked(hWnd, IDC_NEW_PROCESS))
		enableProcessList = 0;
	else if (BST_CHECKED == IsDlgButtonChecked(hWnd, IDC_INJECT_PROCESS))
		enableExePath = 0;
	EnableWindow(GetDlgItem(hWnd, IDC_PROCESS_LIST), enableProcessList);
	EnableWindow(GetDlgItem(hWnd, IDC_EXE_PATH), enableExePath);

	int enableAgth = BST_CHECKED == IsDlgButtonChecked(hWnd, IDC_AGTH);
	EnableWindow(GetDlgItem(hWnd, IDC_CLIPBOARD_COPY), enableAgth);

	EnableWindow(GetDlgItem(hWnd, IDC_AGTH_REMOVE_REPEATED_SYMBOLS), enableAgth);
	int enableSymbolCount = enableAgth && BST_CHECKED == IsDlgButtonChecked(hWnd, IDC_AGTH_REMOVE_REPEATED_SYMBOLS);
	EnableWindow(GetDlgItem(hWnd, IDC_AGTH_REMOVE_REPEATED_SYMBOL_COUNT), enableSymbolCount);

	EnableWindow(GetDlgItem(hWnd, IDC_AGTH_REMOVE_REPEATED_PHRASES), enableAgth);
	int enablePhraseCount = enableAgth && BST_CHECKED == IsDlgButtonChecked(hWnd, IDC_AGTH_REMOVE_REPEATED_PHRASES);
	EnableWindow(GetDlgItem(hWnd, IDC_AGTH_REMOVE_REPEATED_PHRASE_COUNT), enablePhraseCount);
	EnableWindow(GetDlgItem(hWnd, IDC_AGTH_REMOVE_REPEATED_PHRASE_COUNT2), enablePhraseCount);

	EnableWindow(GetDlgItem(hWnd, IDC_AGTH_ADD_PARAMS), enableAgth);
	int enableAddParams = enableAgth && BST_CHECKED == IsDlgButtonChecked(hWnd, IDC_AGTH_ADD_PARAMS);
	EnableWindow(GetDlgItem(hWnd, IDC_AGTH_PARAMS_ADDED), enableAddParams);

	int enableInternal = IsDlgButtonChecked(hWnd, IDC_INTERNAL_HOOK);
	EnableWindow(GetDlgItem(hWnd, IDC_INTERNAL_IDLE_DELAY), enableInternal);
	EnableWindow(GetDlgItem(hWnd, IDC_INTERNAL_HOOKS), enableInternal);
	EnableWindow(GetDlgItem(hWnd, IDC_INTERNAL_DEFAULT_HOOKS), enableInternal);
	EnableWindow(GetDlgItem(hWnd, IDC_INTERNAL_DEFAULT_FILTERS), enableInternal);


	EnableWindow(GetDlgItem(hWnd, IDC_AGTH_ACTUAL_PARAMS), enableAgth);

	HWND hWndText = GetDlgItem(hWnd, IDC_EXE_PATH);
	SendMessage(hWndText, EM_SETSEL, MAX_PATH, MAX_PATH);
}

void UpdateAGTHText(HWND hWnd)
{
	GetConfigFromDialog(hWnd, 0);
	std::wstring params = GetAGTHParams(cfg).c_str();
	if (!params.length())
		SetDlgItemText(hWnd, IDC_AGTH_ACTUAL_PARAMS, L"");
	else
		SetDlgItemText(hWnd, IDC_AGTH_ACTUAL_PARAMS, GetAGTHParams(cfg).c_str()+1);
}

void SetInternalHooks(HWND hWnd, const std::wstring& injectionHooks)
{
	wchar_t* out = (wchar_t*) malloc(sizeof(wchar_t) * (1+sizeof(wchar_t)*injectionHooks.length()));
	const wchar_t* inPos = injectionHooks.c_str();
	wchar_t* outPos = out;
	while (1)
	{
		outPos[0] = inPos++[0];
		if (!outPos[0]) break;
		if (wcschr(L" \t\r\n", outPos[0]))
		{
			outPos++[0] = '\r';
			outPos[0] = '\n';
		}
		outPos++;
	}
	SetDlgItemText(hWnd, IDC_INTERNAL_HOOKS, out);
	free(out);
}

int GetLocaleName(const LCID& lcid, wchar_t *name, int maxNameLen)
{
	if (lcid)
	{
#ifdef SETSUMI_CHANGES
		int len = GetLocaleInfoW(lcid, 0x00001001/*LOCALE_SENGLISHLANGUAGENAME*/, name, maxNameLen); //hack - replace LOCALE_SENGLISHLANGUAGENAME with value 
#else
		int len = GetLocaleInfoW(lcid, LOCALE_SLOCALIZEDDISPLAYNAME, name, maxNameLen);
#endif
		if (len > 0) return len;
		len = GetLocaleInfoW(lcid, LOCALE_SLANGUAGE, name, maxNameLen);
		if (len > 0) return len;
	}
	return 0;
}

static BOOL CALLBACK PopulateLocalesProc(wchar_t *lpLocaleString)
{
	// Seems to work.  This behavior is unspecified by MSDN.
	// "Locale string" sounds more like locale name than LCID to me.
	LCID lcid = wcstoul(lpLocaleString, 0, 16);
	wchar_t name[80];
	if (GetLocaleName(lcid, name, sizeof(name)/2))
	{
		int w = SendMessageW(gCBLocale, CB_FINDSTRINGEXACT, -1, (LPARAM)name);
		if (w < 0)
			SendMessageW(gCBLocale, CB_ADDSTRING, 0, (LPARAM)name);
	}
	return 1;
}

static BOOL CALLBACK FindLocaleProc(wchar_t *lpLocaleString)
{
	// Seems to work.  This behavior is unspecified by MSDN.
	// "Locale string" sounds more like locale name than LCID to me.
	LCID lcid = wcstoul(lpLocaleString, 0, 16);
	wchar_t name[80];
	if (lcid && GetLocaleName(lcid, name, sizeof(name)/2) && !wcsicmp(name, gSeekLocaleName))
	{
		gFoundLCID = lcid;
		return 0;
	}
	return 1;
}

BOOL CALLBACK ProcessesWithWindows(HWND hWnd, LPARAM lParam)
{
	DWORD *pids = (DWORD*)lParam;
	DWORD windowPid = 0;
	DWORD junk = GetWindowThreadProcessId(hWnd, &windowPid);
	while (*pids && *pids != windowPid)
		++pids;
	*pids = windowPid;
	return 1;
}

// Returns currently selected pid, if any.
static int UpdateProcessList(HWND hWndList)
{
	int last_selected_item = ListView_GetNextItem(hWndList, -1, LVNI_SELECTED);
	DWORD last_selected_pid = 0;
	if (last_selected_item >= 0)
	{
		LVITEM item;
		item.iItem = last_selected_item;
		item.iSubItem = 0;
		item.mask = LVIF_PARAM;
		ListView_GetItem(hWndList, &item);
		last_selected_pid = item.lParam;
	}

	int selected_pid = 0;
	// 1 = same name, 2 = same path+folder, 3 = same path, 4 = same as last_selected_pid.
	int selected_match_quality = 0;

	DWORD pids[10000];
	memset(pids, 0, sizeof(pids));
	getDebugPriv();
	EnumWindows(ProcessesWithWindows, (LPARAM)pids);
	LVITEM item;
	wchar_t name[MAX_PATH*2];
	item.pszText = name;
	LVFINDINFO find_info;
	find_info.flags = LVFI_PARAM;
	DWORD index = 0;
	item.iSubItem = 0;

	// Remove processes without a window.  Old processes that could once be opened
	// but no longer can be could theoretically be left in the list if they have
	// Windows, but don't think that really matters.  Will still catch them when
	// they close, anyways.
	while (true)
	{
		item.iItem = index;
		if (!ListView_GetItem(hWndList, &item))
			break;
		DWORD i;
		for (i = 0; pids[i]; ++i)
		{
			if (pids[i] == item.lParam)
				break;
		}
		if (pids[i])
		{
			++index;
			continue;
		}
		ListView_DeleteItem(hWndList, index);
	}

	for (DWORD i = 0; pids[i]; ++i)
	{
		find_info.lParam = pids[i];
		int index = ListView_FindItem(hWndList, -1, &find_info);
		HANDLE hProcess;
		if (hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, 0, pids[i]))
		{
			if (GetModuleBaseName(hProcess, 0, name, sizeof(name)/sizeof(name[0])))
			{
				int match_quality = 0;
				if (cfg->exeName && !wcsicmp(name, cfg->exeName))
				{
					match_quality = 1;
					wchar_t full_name[MAX_PATH*2];
					if (pids[i] == last_selected_pid)
						match_quality = 4;
					else if (GetModuleFileNameEx(hProcess, NULL, full_name, sizeof(full_name)/sizeof(full_name[0])))
						if (wcsicmp(full_name, cfg->exePath))
							match_quality = 3;
						else if (wcsstr(full_name, cfg->exeNamePlusFolder))
							match_quality = 2;
				}

				if (index < 0)
				{
					item.mask = LVIF_TEXT | LVIF_PARAM;
					item.lParam = pids[i];
					item.iSubItem = 0;
					item.iItem = 0;
					index = ListView_InsertItem(hWndList, &item);

					item.mask = LVIF_TEXT;
					item.iItem = index;
					item.iSubItem = 1;
					_itow(pids[i], name, 10);
					ListView_SetItem(hWndList, &item);
				}

				if (selected_match_quality < match_quality)
				{
					selected_match_quality = match_quality;
					selected_pid = pids[i];
					ListView_SetItemState(hWndList, index, LVIS_SELECTED|LVIS_FOCUSED,LVIS_SELECTED|LVIS_FOCUSED);
				}
			}
			CloseHandle(hProcess);
		}
	}
	return selected_pid;
}

static void UpdateDialog(HWND hWnd, const std::wstring& injectionHooks)
{
	if (cfg->injectionFlags & INJECT_PROCESS)
	{
		CheckDlgButton(hWnd, IDC_AUTODETECT_PROCESS, BST_UNCHECKED);
		CheckDlgButton(hWnd, IDC_INJECT_PROCESS, BST_CHECKED);
		CheckDlgButton(hWnd, IDC_NEW_PROCESS, BST_UNCHECKED);
	}
	else if (cfg->injectionFlags & NEW_PROCESS)
	{
		CheckDlgButton(hWnd, IDC_AUTODETECT_PROCESS, BST_UNCHECKED);
		CheckDlgButton(hWnd, IDC_INJECT_PROCESS, BST_UNCHECKED);
		CheckDlgButton(hWnd, IDC_NEW_PROCESS, BST_CHECKED);
	}
	else
	{
		CheckDlgButton(hWnd, IDC_AUTODETECT_PROCESS, BST_CHECKED);
		CheckDlgButton(hWnd, IDC_INJECT_PROCESS, BST_UNCHECKED);
		CheckDlgButton(hWnd, IDC_NEW_PROCESS, BST_UNCHECKED);
	}

	int localeStringIndex = 0;
	gCBLocale = GetDlgItem(hWnd, IDC_CB_LOCALE);
	wchar_t name[80];
	if (cfg->forceLocale && GetLocaleName(cfg->forceLocale, name, sizeof(name)/2))
	{
		localeStringIndex = SendMessageW(gCBLocale, CB_FINDSTRINGEXACT, -1, (LPARAM)name);
		if (localeStringIndex < 0) localeStringIndex = 0;
	}
	SendMessage(gCBLocale, CB_SETCURSEL, localeStringIndex, 0);

	SetDlgButton(hWnd, IDC_TRANSLATE_MENUS, cfg->injectionFlags & TRANSLATE_MENUS);

	SetDlgButton(hWnd, IDC_LOG_TRANSLATIONS, cfg->injectionFlags & LOG_TRANSLATIONS);

	SetDlgButton(hWnd, IDC_DCBS_OVERRIDE, cfg->injectionFlags & DCBS_OVERRIDE);

	SetDlgButton(hWnd, IDC_INJECT_CHILDREN, !(cfg->injectionFlags & NO_INJECT_CHILDREN));
	SetDlgButton(hWnd, IDC_INJECT_CHILDREN_SAME_SETTINGS, !(cfg->injectionFlags & NO_INJECT_CHILDREN_SAME_SETTINGS));

	int hooks[3] =
	{
		BST_UNCHECKED, BST_UNCHECKED, BST_UNCHECKED
	};
	if (cfg->injectionFlags & INTERNAL_HOOK)
		hooks[2] = BST_CHECKED;
	else if (cfg->injectionFlags & AGTH_HOOK)
		hooks[1] = BST_CHECKED;
	else
		hooks[0] = BST_CHECKED;
	for (int i=0; i<4; i++)
		CheckDlgButton(hWnd, IDC_NO_HOOK+i, hooks[i]);

	SetDlgItemText(hWnd, IDC_EXE_PATH, cfg->exePath);

	SetDlgButton(hWnd, IDC_CLIPBOARD_COPY, cfg->agthFlags & FLAG_AGTH_COPYDATA);

	SetDlgButton(hWnd, IDC_AGTH_REMOVE_REPEATED_SYMBOLS, cfg->agthFlags & FLAG_AGTH_SUPPRESS_SYMBOLS);
	SetDlgItemInt(hWnd, IDC_AGTH_REMOVE_REPEATED_SYMBOL_COUNT, cfg->agthSymbolRepeatCount, 1);

	SetDlgButton(hWnd, IDC_AGTH_REMOVE_REPEATED_PHRASES, cfg->agthFlags & FLAG_AGTH_SUPPRESS_PHRASES);
	SetDlgItemInt(hWnd, IDC_AGTH_REMOVE_REPEATED_PHRASE_COUNT, cfg->agthPhraseCount1, 1);
	SetDlgItemInt(hWnd, IDC_AGTH_REMOVE_REPEATED_PHRASE_COUNT2, cfg->agthPhraseCount2, 1);


	SetDlgButton(hWnd, IDC_AGTH_ADD_PARAMS, cfg->agthFlags & FLAG_AGTH_ADD_PARAMS);
	SetDlgItemText(hWnd, IDC_AGTH_PARAMS_ADDED, cfg->agthParams);

	SetDlgButton(hWnd, IDC_INTERNAL_DEFAULT_HOOKS, !(cfg->injectionFlags & INTERNAL_NO_DEFAULT_HOOKS));
	SetDlgButton(hWnd, IDC_INTERNAL_DEFAULT_FILTERS, !(cfg->injectionFlags & INTERNAL_NO_DEFAULT_FILTERS));

	SetDlgItemInt(hWnd, IDC_HOOK_DELAY, cfg->internalHookDelay, 0);
	SetDlgItemInt(hWnd, IDC_INTERNAL_IDLE_DELAY, cfg->internalHookTime, 0);

	SetInternalHooks(hWnd, injectionHooks.c_str());

	UpdateProcessList(GetDlgItem(hWnd, IDC_PROCESS_LIST));
	UpdateEnabledControls(hWnd);
	UpdateAGTHText(hWnd);
}

WNDPROC stolenComboWndProc = 0;

int GetInternalHooks(HWND hWnd, wchar_t *temp, int tempLen)
{
	int len = GetDlgItemText(hWnd, IDC_INTERNAL_HOOKS, temp, tempLen);
	if (len <= 0 || len >= tempLen)
	{
		temp[0] = 0;
		return 0;
	}
	wchar_t *inPos = temp;
	wchar_t *outPos = temp;
	while (1)
	{
		outPos[0] = inPos[0];
		if (!inPos[0])
			break;
		inPos++;

		// Ignore all whitespace, except linebreaks, which we replace with spaces.
		// Saved strings should be one line, I believe.
		if (wcschr(L"\r \t", outPos[0])) continue;
		if (outPos[0] == '\n')
			outPos[0] = ' ';
		outPos++;
	}
	return inPos - temp;
}

void GetConfigFromDialog(HWND hWnd, std::wstring* injectionHooks)
{
	GetDlgItemText(hWnd, IDC_EXE_PATH, cfg->exePath, sizeof(cfg->exePath)/sizeof(wchar_t));

	gCBLocale = GetDlgItem(hWnd, IDC_CB_LOCALE);
	cfg->forceLocale = 0;
	wchar_t name[80];
	int sel = SendMessageW(gCBLocale, CB_GETCURSEL, 0, 0);
	if (sel >= 0)
	{
		int l = SendMessageW(gCBLocale, CB_GETLBTEXTLEN, sel, 0);
		gFoundLCID = 0;
		if (l > 0 && l < sizeof(name)/sizeof(wchar_t) &&
			l == SendMessageW(gCBLocale, CB_GETLBTEXT, sel, (LPARAM)name))
			{
				gSeekLocaleName = name;
				EnumSystemLocalesW(FindLocaleProc, 0);
		}
	}
	cfg->forceLocale = gFoundLCID;

	cfg->injectionFlags = 0;
	if (BST_CHECKED == IsDlgButtonChecked(hWnd, IDC_INJECT_PROCESS))
		cfg->injectionFlags |= INJECT_PROCESS;
	else if (BST_CHECKED == IsDlgButtonChecked(hWnd, IDC_NEW_PROCESS))
		cfg->injectionFlags |= NEW_PROCESS;
	if (BST_CHECKED == IsDlgButtonChecked(hWnd, IDC_TRANSLATE_MENUS))
		cfg->injectionFlags |= TRANSLATE_MENUS;
	if (BST_CHECKED == IsDlgButtonChecked(hWnd, IDC_DCBS_OVERRIDE))
		cfg->injectionFlags |= DCBS_OVERRIDE;
	if (BST_CHECKED != IsDlgButtonChecked(hWnd, IDC_INJECT_CHILDREN))
		cfg->injectionFlags |= NO_INJECT_CHILDREN;
	if (BST_CHECKED != IsDlgButtonChecked(hWnd, IDC_INJECT_CHILDREN_SAME_SETTINGS))
		cfg->injectionFlags |= NO_INJECT_CHILDREN_SAME_SETTINGS;
	if (BST_CHECKED == IsDlgButtonChecked(hWnd, IDC_INTERNAL_HOOK))
		cfg->injectionFlags |= INTERNAL_HOOK;
	else if (BST_CHECKED == IsDlgButtonChecked(hWnd, IDC_AGTH))
		cfg->injectionFlags |= AGTH_HOOK;
	if (BST_CHECKED == IsDlgButtonChecked(hWnd, IDC_LOG_TRANSLATIONS))
		cfg->injectionFlags |= LOG_TRANSLATIONS;

	cfg->agthFlags = 0;
	if (BST_CHECKED == IsDlgButtonChecked(hWnd, IDC_CLIPBOARD_COPY))
		cfg->agthFlags |= FLAG_AGTH_COPYDATA;

	if (BST_CHECKED == IsDlgButtonChecked(hWnd, IDC_AGTH_REMOVE_REPEATED_SYMBOLS))
		cfg->agthFlags |= FLAG_AGTH_SUPPRESS_SYMBOLS;
	cfg->agthSymbolRepeatCount = GetDlgItemInt(hWnd, IDC_AGTH_REMOVE_REPEATED_SYMBOL_COUNT, 0, 1);

	if (BST_CHECKED == IsDlgButtonChecked(hWnd, IDC_AGTH_REMOVE_REPEATED_PHRASES))
		cfg->agthFlags |= FLAG_AGTH_SUPPRESS_PHRASES;
	cfg->agthPhraseCount1 = GetDlgItemInt(hWnd, IDC_AGTH_REMOVE_REPEATED_PHRASE_COUNT, 0, 1);
	cfg->agthPhraseCount2 = GetDlgItemInt(hWnd, IDC_AGTH_REMOVE_REPEATED_PHRASE_COUNT2, 0, 1);

	if (BST_CHECKED == IsDlgButtonChecked(hWnd, IDC_AGTH_ADD_PARAMS))
		cfg->agthFlags |= FLAG_AGTH_ADD_PARAMS;

	cfg->internalHookDelay = GetDlgItemInt(hWnd, IDC_HOOK_DELAY, 0, 0);
	cfg->internalHookTime = GetDlgItemInt(hWnd, IDC_INTERNAL_IDLE_DELAY, 0, 0);

	if (BST_CHECKED != IsDlgButtonChecked(hWnd, IDC_INTERNAL_DEFAULT_HOOKS))
		cfg->injectionFlags |= INTERNAL_NO_DEFAULT_HOOKS;
	if (BST_CHECKED != IsDlgButtonChecked(hWnd, IDC_INTERNAL_DEFAULT_FILTERS))
		cfg->injectionFlags |= INTERNAL_NO_DEFAULT_FILTERS;

	if (injectionHooks)
	{
		wchar_t temp[10000];
		int len = GetInternalHooks(hWnd, temp, sizeof(temp)/sizeof(temp[0]));
		injectionHooks->assign(temp);
	}

	GetDlgItemText(hWnd, IDC_AGTH_PARAMS_ADDED, cfg->agthParams, sizeof(cfg->agthParams)/sizeof(wchar_t));

	cfg->exeNamePlusFolder = cfg->exeName = wcsrchr(cfg->exePath, '\\');
	if (cfg->exeName)
	{
		while (cfg->exeNamePlusFolder > cfg->exePath && cfg->exeNamePlusFolder[-1] !='\\' && cfg->exeNamePlusFolder[-1] !=':')
			cfg->exeNamePlusFolder--;
		cfg->exeName++;
	}
}

LRESULT CALLBACK ComboProc(HWND hWndCombo, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if ((msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN) && wParam == VK_DELETE)
	{
		wchar_t temp[MAX_PATH];
		int index = SendMessage(hWndCombo, CB_GETCURSEL, 0, 0);
		if (index >= 0 && SendMessage(hWndCombo, CB_GETLBTEXT, index, (LPARAM)temp))
		{
			wchar_t path[MAX_PATH];
			if (GetConfigPath(path, temp))
				DeleteFileW(path);
			SendMessage(hWndCombo, CB_DELETESTRING, index, 0);
			int count = SendMessage(hWndCombo, CB_GETCOUNT, 0, 0);
			if (count <= index) index--;
			SendMessage(hWndCombo, CB_SETCURSEL, index, 0);
		}
		return 0;
	}
	return CallWindowProc(stolenComboWndProc, hWndCombo, msg, wParam, lParam);
}

void SetInjectPath(HWND hWnd, wchar_t *path)
{
	HWND hWndCombo = GetDlgItem(hWnd, IDC_PROFILE_NAME);
	SetDlgItemText(hWnd, IDC_EXE_PATH, path);
	wchar_t * w = wcsrchr(path, L'\\');
	if (w)
	{
		while (w > path && w[-1] != '\\') w--;
		if (w>path)
		{
			InjectionSettings cfg2 = *cfg;
			std::wstring injectionHooks;
			if (LoadInjectionSettings(cfg2, w, path, &injectionHooks) > 1)
			{
				*cfg = cfg2;
				UpdateDialog(hWnd, injectionHooks);
				int sel = SendMessage(hWndCombo, CB_FINDSTRINGEXACT, -1, (LPARAM)w);
				SendMessage(hWndCombo, CB_SETCURSEL, sel, 0);
				return;
			}
			*cfg = cfg2;
			UpdateDialog(hWnd, injectionHooks);
		}
	}
	if (BST_CHECKED == IsDlgButtonChecked(hWnd, IDC_INJECT_PROCESS))
	{
		CheckDlgButton(hWnd, IDC_AUTODETECT_PROCESS, BST_CHECKED);
		CheckDlgButton(hWnd, IDC_INJECT_PROCESS, BST_UNCHECKED);
	}
	SendMessage(hWndCombo, CB_SETCURSEL, -1, 0);
	UpdateEnabledControls(hWnd);
}

wchar_t *GetDragDropPath(HWND hWnd, HDROP hDrop)
{
	wchar_t name[MAX_PATH] = {0};
	int numFiles = DragQueryFileW(hDrop, -1, name, sizeof(name)/sizeof(name[0]));
	for (int i=0; i<numFiles; i++)
	{
		int oldStart = name[0];
		int len = DragQueryFileW(hDrop, i, name, sizeof(name)/sizeof(name[0]));
		if (len <= 4) continue;
		wchar_t *ext = wcsrchr(name, '.');
		if (!ext || (!wcsicmp(ext, L".exe") && numFiles > 1)) continue;
		if (oldStart)
		{
			MessageBoxW(hWnd, L"Only drag one exe at a time.", L"Multiple Exes", MB_OK);
			DragFinish(hDrop);
			return 0;
		}
	}
	if (!name[0])
		MessageBoxW(hWnd, L"Only drag one file at a time.", L"Multiple Exes", MB_OK);
	else
		return wcsdup(name);
	DragFinish(hDrop);
	return 0;
}

int CheckHookSyntax(HWND hWnd, wchar_t *temp, wchar_t *titleOnError)
{
	wchar_t errors[20000];
	errors[0] = 0;
	if (titleOnError)
		wcscpy(errors, L"Invalid hook codes:\r\n\r\n");
	int happy = 1;
	wchar_t *p = temp;
	while (*p)
	{
		int len = wcscspn(p, L" \r\n\t");
		if (!len)
		{
			p++;
			continue;
		}
		wchar_t c = p[len];
		p[len] = 0;

		TextHookInfo *info = ParseTextHookString(p, 1);
		if (!info)
		{
			wcscat(errors, p);
			wcscat(errors, L" (Bad syntax)\r\n");
			happy = 0;
		}
		else
			CleanupTextHookInfo(info);

		p[len] = c;
		p+=len;
	}
	if (!happy && titleOnError)
	{
		wcschr(errors, 0)[-2] = 0;
		MessageBoxW(hWnd, errors, titleOnError, MB_OK | MB_ICONERROR);
	}
	return happy;
}

int CheckHookSyntax(HWND hWnd, wchar_t *titleOnError)
{
	wchar_t temp[10000];
	GetInternalHooks(hWnd, temp, sizeof(temp)/sizeof(temp[0]));
	return CheckHookSyntax(hWnd, temp, titleOnError);
}

int GetAddr(wchar_t *&taPos, wchar_t *&pos)
{
	wchar_t base[50];
	wchar_t *end;
	if (pos[0] == '-')
	{
		pos++;
		unsigned long v = wcstoul(pos, &end, 16);
		wchar_t *reg;
		switch(v)
		{
			case 0x04:
				reg = L"EAX";
				break;
			case 0x08:
				reg = L"ECX";
				break;
			case 0x0C:
				reg = L"EDX";
				break;
			case 0x10:
				reg = L"EDX";
				break;
			case 0x14:
				reg = L"ESP";
				break;
			case 0x18:
				reg = L"EBP";
				break;
			case 0x1C:
				reg = L"ESI";
				break;
			case 0x20:
				reg = L"EDI";
				break;
			default:
				return 0;
		}
		wcscpy(base, reg);
	}
	else
	{
		unsigned long v = wcstoul(pos, &end, 16);
		if (pos == end) return 0;
		if (v)
			wsprintf(base, L"[ESP+%u]", v);
		// Probably not exactly common...
		else
			wsprintf(base, L"[ESP]", v);
	}
	pos = end;
	if (pos[0] != '*')
	{
		wcscpy(taPos, base);
		taPos = wcschr(taPos, 0);
		return 1;
	}

	pos++;
	int v = wcstoul(pos, &end, 16);
	if (pos == end) return 0;

	if (v)
		wsprintf(taPos, L"[%s+%u]", base, v);
	else
		wsprintf(taPos, L"[%s]", base, v);
	taPos = wcschr(taPos, 0);
	pos = end;
	return 1;
}

int ConvertCode(wchar_t *agth, wchar_t *ta)
{
	int forceLen = 0;
	int noSubContext = 0;
	wchar_t *pos = agth;
	if (pos[0] == '/') pos++;
	if (towupper(pos[0]) == 'H') pos++;
	// Currently just ignore /HX.
	if (towupper(pos[0]) == 'X') pos++;
	wchar_t *type = 0;
	switch (towupper(pos[0]))
	{
		case 'A':
			type = L"char";
			break;
		case 'B':
			type = L"charBE";
			break;
		case 'W':
			type = L"wchar";
			break;
		case 'S':
			type = L"char*";
			break;
		case 'Q':
			type = L"wchar*";
			break;
		case 'H':
			type = L"char";
			forceLen = 2;
			break;
		default:
			return 0;
	}
	wcscpy(ta, type);
	wcscat(ta, L":");
	wchar_t *taPos = wcschr(ta, 0);
	pos++;
	if (towupper(pos[0]) == 'N')
	{
		// Don't use the default subcontext.
		noSubContext = 1;
		pos++;
	}
	if (!GetAddr(taPos, pos))
		return 0;
	if (pos[0] == ':')
	{
		taPos++[0] = ':';
		pos++;
		if (!noSubContext)
		{
			wcscpy(taPos, L"[ESP];");
			taPos = wcschr(taPos, 0);
		}
		if (!GetAddr(taPos, pos))
			return 0;
	}
	else if (noSubContext)
	{
		taPos++[0] = ':';
		taPos++[0] = '0';
	}
	else if (forceLen)
		taPos++[0] = ':';
	if (forceLen)
	{
		wsprintf(taPos, L":%i", forceLen);
		taPos = wcschr(taPos, 0);
	}
	if (pos[0] != '@')
		return 0;
	wcscpy(taPos, pos);
	return 1;
}

INT_PTR CALLBACK ConvertDialog(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (msg == WM_COMMAND)
	{
		int ctrl = LOWORD(wParam);
		int cmd = HIWORD(wParam);

		wchar_t TACode[1000];
		if (cmd == EN_CHANGE && ctrl == IDC_AGTH_CODE)
		{
			wchar_t agthCode[500];
			int len = GetDlgItemText(hWnd, IDC_AGTH_CODE, agthCode, sizeof(agthCode)/sizeof(wchar_t));
			if (len < 0 || len >= sizeof(agthCode)/sizeof(wchar_t))
				TACode[0] = 0;
			else
			{
				SpiffyUp(agthCode);
				if (!ConvertCode(agthCode, TACode) || !CheckHookSyntax(hWnd, TACode, 0))
					TACode[0] = 0;
			}
			SetDlgItemText(hWnd, IDC_TA_CODE, TACode);
			EnableWindow(GetDlgItem(hWnd, IDC_ADD), TACode[0] != 0);
		}
		else if (cmd == BN_CLICKED)
		{
			if (ctrl == IDCANCEL || ctrl == IDOK)
				EndDialog(hWnd, 1);
			else if (ctrl == IDC_ADD)
			{
				if (GetDlgItemText(hWnd, IDC_TA_CODE, TACode, sizeof(TACode)/sizeof(wchar_t)))
				{
					wchar_t temp[10000];
					HWND hWndParent = GetParent(hWnd);
					if (hWndParent)
					{
						GetInternalHooks(hWndParent, temp, sizeof(temp)/sizeof(temp[0]));
						if (wcslen(temp) + wcslen(TACode) + 10 < sizeof(temp)/sizeof(temp[0]))
						{
							if (temp[0]) wcscat(temp, L"\n");
							wcscat(temp, TACode);
							SetInternalHooks(hWndParent, temp);
						}
					}
				}
			}
		}
	}
	return 0;
}

INT_PTR CALLBACK InjectDialog(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	HWND hWndList = GetDlgItem(hWnd, IDC_PROCESS_LIST);
	static int busy = 0;
	if (busy) return 0;
	busy = 1;
	switch (msg)
	{
		case WM_INITDIALOG:
		{
			SetForegroundWindow(hWnd);
			SetTimer(hWnd, 1, 300, NULL);
			DragAcceptFiles(hWnd, 1);
			HWND hWndCombo = GetDlgItem(hWnd, IDC_PROFILE_NAME);
			stolenComboWndProc = (WNDPROC)SetWindowLongPtr(hWndCombo, GWLP_WNDPROC, (LONG_PTR) ComboProc);

			WIN32_FIND_DATAW data;
			HANDLE hFind = FindFirstFileW(L"Game Configs\\*.ini", &data);
			if (hFind !=INVALID_HANDLE_VALUE)
			{
				do
				{
					if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
					wchar_t *s = wcsrchr(data.cFileName, '.');
					if (s) *s = 0;
					s = wcschr(data.cFileName, '+');
					if (s) *s = '\\';
					SendMessage(hWndCombo, CB_ADDSTRING, 0, (LPARAM)data.cFileName);
				}
				while (FindNextFile(hFind, &data));
				FindClose(hFind);
			}

			ListView_SetExtendedListViewStyleEx(hWndList, LVS_EX_DOUBLEBUFFER, LVS_EX_DOUBLEBUFFER);
			LVCOLUMN c;
			c.mask = LVCF_TEXT | LVCF_WIDTH;
			c.cx = 171;
			c.pszText = L"Process Name";
			ListView_InsertColumn(hWndList, 0, &c);
			c.cx = 50;
			c.pszText = L"PID";
			ListView_InsertColumn(hWndList, 1, &c);
			wchar_t temp[MAX_PATH];
			int res = GetPrivateProfileStruct(L"Injection", L"Last Launched Struct", temp, sizeof(temp), config.ini);
			if (!res)
				GetPrivateProfileStringW(L"Injection", L"Last Launched", L"Default", temp, MAX_PATH*2, config.ini);
			gCBLocale = GetDlgItem(hWnd, IDC_CB_LOCALE);
			SendMessage(gCBLocale, CB_ADDSTRING, 0, (LPARAM)L" System Default");
			EnumSystemLocalesW(PopulateLocalesProc, LCID_INSTALLED);

			std::wstring injectionHooks;
			LoadInjectionSettings(*cfg, temp, 0, &injectionHooks);

			int i = SendMessage(hWndCombo, CB_FINDSTRINGEXACT, -1, (LPARAM)cfg->exeNamePlusFolder);
			if (i >= 0)
				SendMessage(hWndCombo, CB_SETCURSEL, i, 0);

			UpdateDialog(hWnd, injectionHooks.c_str());
			if (lParam)
				SetInjectPath(hWnd, (wchar_t*) lParam);
			break;
		}
		case WM_TIMER:
		{
			std::wstring injectionHooks;
			GetConfigFromDialog(hWnd, &injectionHooks);
			UpdateProcessList(GetDlgItem(hWnd, IDC_PROCESS_LIST));
			break;
		}
		case WM_DROPFILES:
		{
			wchar_t *path = GetDragDropPath(hWnd, (HDROP) wParam);
			if (path)
			{
				SetInjectPath(hWnd, path);
				free(path);
			}
			break;
		}
		case WM_NOTIFY:
		{
			NMHDR *n = (NMHDR *)lParam;
			if (n->idFrom == IDC_PROCESS_LIST)
			{
				if (n->code == LVN_ITEMCHANGED)
				{
					wchar_t temp[MAX_PATH];
					NMLISTVIEW *nlv = (NMLISTVIEW*)n;
					if (!(nlv->uNewState & LVIS_SELECTED) || (nlv->uOldState & LVIS_SELECTED))
					{
						cfg->exeName = cfg->exeNamePlusFolder = cfg->exePath;
						cfg->exePath[0] = 0;
						UpdateDialog(hWnd, NULL);
						break;
					}
					HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, 0, nlv->lParam);
					int sel = -1;
					if (hProcess)
					{
						if (GetModuleFileNameEx(hProcess, 0, temp, sizeof(temp)/sizeof(temp[0])))
						{
							wchar_t * w = wcsrchr(temp, L'\\');
							SetDlgItemText(hWnd, IDC_EXE_PATH, temp);
							if (w)
							{
								while (w > temp && w[-1] != '\\') w--;
								if (w > temp)
								{
									InjectionSettings cfg2 = *cfg;
									std::wstring injectionHooks;
									if (LoadInjectionSettings(cfg2, w, temp, &injectionHooks) > 1)
									{
										*cfg = cfg2;
										UpdateDialog(hWnd, injectionHooks);
										sel = SendMessage(GetDlgItem(hWnd, IDC_PROFILE_NAME), CB_FINDSTRINGEXACT, -1, (LPARAM)w);
									}
								}
							}
							UpdateEnabledControls(hWnd);
						}
					}
					else
					{
						// Process has already been closed.
						UpdateDialog(hWnd, NULL);
					}
					SendMessage(GetDlgItem(hWnd, IDC_PROFILE_NAME), CB_SETCURSEL, sel, 0);
					CloseHandle(hProcess);
				}
			}
			break;
		}
		case WM_COMMAND:
		{
			if (HIWORD(wParam) == EN_CHANGE)
				UpdateAGTHText(hWnd);
			if (HIWORD(wParam) == CBN_SELCHANGE && LOWORD(wParam) == IDC_PROFILE_NAME)
			{
				std::wstring injectionHooks;
				HWND hWndCombo = GetDlgItem(hWnd, IDC_PROFILE_NAME);
				wchar_t temp[MAX_PATH*2];
				int index = SendMessage(hWndCombo, CB_GETCURSEL, 0, 0);
				InjectionSettings inj;
				if (index >= 0 && SendMessage(hWndCombo, CB_GETLBTEXT, index, (LPARAM)temp) && LoadInjectionSettings(inj, temp, 0, &injectionHooks))
				{
					*cfg = inj;
					UpdateDialog(hWnd, injectionHooks);
				}
			}
			else if (HIWORD(wParam) == BN_CLICKED)
			{
				int cmd = LOWORD(wParam);
				if (cmd == IDC_CONFIG_ATLAS)
					ConfigureAtlas(hWnd, cfg->atlasConfig);
				else if (cmd == IDC_CHECK_INTERNAL_CODES)
				{
					if (CheckHookSyntax(hWnd, L"Invalid hook codes"))
						MessageBoxW(hWnd, L"Hook codes valid", L"No errors found", MB_OK);
					break;
				}
				else if (cmd == IDC_AGTH_CODE_CONVERSION_TOOL)
				{
					DialogBoxW(ghInst, MAKEINTRESOURCE(IDD_AGTH_CONVERSION_TOOL), hWnd, ConvertDialog);
					break;
				}
				else if (cmd == IDC_CREATE_SHORTCUT)
				{
					std::wstring injectionHooks;
					GetConfigFromDialog(hWnd, &injectionHooks);
					wchar_t taFileName[MAX_PATH];
					if (!cfg->exeName || GetModuleFileNameW(0, taFileName, sizeof(taFileName)/sizeof(wchar_t)) <= 0)
						break;
					IShellLink* link;
					HRESULT result = CoCreateInstance(CLSID_ShellLink, 0, CLSCTX_INPROC_SERVER, IID_IShellLink, (LPVOID*)&link);
					if (SUCCEEDED(result))
					{
						link->SetPath(taFileName);
						link->SetIconLocation(cfg->exePath, 0);
						wchar_t* taExe = wcsrchr(taFileName, '\\');
						*taExe = 0;
						link->SetWorkingDirectory(taFileName);
						wchar_t params[MAX_PATH*10];
						wsprintf(params, L"\"/I:%s\"", cfg->exePath);
						link->SetArguments(params);
						IPersistFile* file;
						result = link->QueryInterface(IID_IPersistFile, (LPVOID*)&file);
						if (SUCCEEDED(result))
						{
							OPENFILENAMEW ofn;
							memset (&ofn, 0, sizeof(ofn));
							ofn.lStructSize = sizeof(ofn);
							ofn.hwndOwner = hWnd;
							ofn.lpstrFilter = L"Shortcuts\0*.lnk\0All Files\0*.*\0\0";
							wchar_t fileName[MAX_PATH*2];
							ofn.lpstrFile = fileName;
							wcscpy(ofn.lpstrFile, cfg->exePath);
							wchar_t* ext = wcschr(cfg->exeName, '.');
							if (ext)
								fileName[ext-cfg->exePath] = 0;
							ofn.nMaxFile = MAX_PATH*2;
							ofn.lpstrInitialDir = 0;
							ofn.Flags = OFN_LONGNAMES | OFN_NOCHANGEDIR;
							ofn.lpstrTitle = L"Create Shortcut";
							if (GetSaveFileNameW(&ofn))
							{
								wcscat(ofn.lpstrFile, L".lnk");
								result = file->Save(ofn.lpstrFile, TRUE);
							}
							file->Release();
						}
						link->Release();
					}
					break;
				}
				else if (cmd == IDOK)
				{
					if (!CheckHookSyntax(hWnd, L"Invalid hook codes"))
						break;
					std::wstring injectionHooks;
					GetConfigFromDialog(hWnd, &injectionHooks);
					// Shouldn't be needed, but just in case.
					UpdateAGTHText(hWnd);
					// Shouldn't happen, unless path is invalid.
					if (!cfg->exeName)
						break;

					SaveInjectionSettings(*cfg, cfg->exeNamePlusFolder, injectionHooks.c_str());
					int pid = 0;
					if (!(cfg->injectionFlags & NEW_PROCESS))
					{
						UpdateDialog(hWnd, injectionHooks);
						pid = UpdateProcessList(hWndList);
						if ((cfg->injectionFlags & NEW_PROCESS) && !pid)
						{
							MessageBoxA(hWnd, "No process selected.", "Error", MB_OK | MB_ICONERROR);
							break;
						}
					}
					if (Inject(cfg, pid, hWnd))
						PostMessage(hWnd, WM_CLOSE, 0, 0);
				}
				else if (cmd == IDCANCEL)
					PostMessage(hWnd, WM_CLOSE, 0, 0);
				else if (cmd == IDC_BROWSE)
				{
					OPENFILENAMEW ofn;
					memset (&ofn, 0, sizeof(ofn));
					ofn.lStructSize = sizeof(ofn);
					ofn.hwndOwner = hWnd;
					ofn.lpstrFilter = L"All Executables\0*.exe\0\0";
					wchar_t file[MAX_PATH*2];
					ofn.lpstrFile = file;
					ofn.nMaxFile = MAX_PATH*2;
					if (!GetDlgItemTextW(hWnd, IDC_EXE_PATH, file, sizeof(file)/sizeof(wchar_t)))
						file[0] = 0;
					ofn.lpstrInitialDir = 0;
					ofn.Flags = OFN_LONGNAMES | OFN_NOCHANGEDIR | OFN_FILEMUSTEXIST;
					ofn.lpstrTitle = L"Launch Program";
					if (GetOpenFileNameW(&ofn))
						SetInjectPath(hWnd, ofn.lpstrFile);
				}
				else
				{
					UpdateAGTHText(hWnd);
					UpdateEnabledControls(hWnd);
				}
			}
			break;
		}
		case WM_CLOSE:
			KillTimer(hWnd, 1);
			EndDialog(hWnd, 1);
			break;
#ifdef SETSUMI_CHANGES
		case WM_SHOWWINDOW: //hack - fix keyboard input on freshly shown dialogs
			if (wParam) {
				HWND hwndOk = GetDlgItem(hWnd, IDOK);
				SetActiveWindow(hWnd);
				SetFocus(hwndOk);
			}
			break;
#endif
		default:
			break;
	}
	busy = 0;
	return 0;
}

void InjectionDialog(HWND hWnd, wchar_t *exe)
{
	InjectionSettings settings;
	cfg = &settings;
	DialogBoxParamW(ghInst, MAKEINTRESOURCE(IDD_INJECT_DIALOG), hWnd, InjectDialog, (LPARAM)exe);
}

void TryDragDrop(HWND hWnd, HDROP hDrop)
{
	wchar_t *path = GetDragDropPath(hWnd, hDrop);
	if (!path) return;
	InjectionDialog(hWnd, path);
	free(path);
}
