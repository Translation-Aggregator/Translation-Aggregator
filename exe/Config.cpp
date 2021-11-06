#include <Shared/Shrink.h>
#include "Config.h"

// Needed for debug privileges.
#include <Shared/DllInjection.h>

#include "resource.h"

#include "exe/History/History.h"

Config config;

// #define INI APP_NAME L".ini"
#define INI L"TranslationAggregator.ini"

#define RANGE_MAX 100000

wchar_t *GetLanguageString(Language lang)
{
	switch (lang)
	{
		case LANGUAGE_AUTO:
			return L"Auto Detect";
		case LANGUAGE_English:
			return L"English";
		case LANGUAGE_Japanese:
			return L"Japanese";
		case LANGUAGE_Afrikaans:
			return L"Afrikaans";
		case LANGUAGE_Albanian:
			return L"Albanian";
		case LANGUAGE_Arabic:
			return L"Arabic";
		case LANGUAGE_Belarusian:
			return L"Belarusian";
		case LANGUAGE_Bengali:
			return L"Bengali";
		case LANGUAGE_Bosnian:
			return L"Bosnian";
		case LANGUAGE_Bulgarian:
			return L"Bulgarian";
		case LANGUAGE_Catalan:
			return L"Catalan";
		case LANGUAGE_Castilian:
			return L"Castilian";
		case LANGUAGE_Chinese_Simplified:
			return L"Chinese (Simplified)";
		case LANGUAGE_Chinese_Traditional:
			return L"Chinese (Traditional)";
		case LANGUAGE_Croatian:
			return L"Croatian";
		case LANGUAGE_Czech:
			return L"Czech";
		case LANGUAGE_Danish:
			return L"Danish";
		case LANGUAGE_Dari:
			return L"Dari";
		case LANGUAGE_Dutch:
			return L"Dutch";
		case LANGUAGE_Esperanto:
			return L"Esperanto";
		case LANGUAGE_Estonian:
			return L"Estonian";
		case LANGUAGE_Filipino:
			return L"Filipino";
		case LANGUAGE_Finnish:
			return L"Finnish";
		case LANGUAGE_French:
			return L"French";
		case LANGUAGE_Galician:
			return L"Galician";
		case LANGUAGE_German:
			return L"German";
		case LANGUAGE_Greek:
			return L"Greek";
		case LANGUAGE_Haitian_Creole:
			return L"Haitian Creole";
		case LANGUAGE_Hausa:
			return L"Hausa";
		case LANGUAGE_Hebrew:
			return L"Hebrew";
		case LANGUAGE_Hindi:
			return L"Hindi";
		case LANGUAGE_Hmong_Daw:
			return L"Hmong Daw";
		case LANGUAGE_Hungarian:
			return L"Hungarian";
		case LANGUAGE_Icelandic:
			return L"Icelandic";
		case LANGUAGE_Indonesian:
			return L"Indonesian";
		case LANGUAGE_Irish:
			return L"Irish";
		case LANGUAGE_Klingon:
			return L"Klingon";
		case LANGUAGE_Italian:
			return L"Italian";
		case LANGUAGE_Korean:
			return L"Korean";
		case LANGUAGE_Latin:
			return L"Latin";
		case LANGUAGE_Latvian:
			return L"Latvian";
		case LANGUAGE_Lithuanian:
			return L"Lithuanian";
		case LANGUAGE_Macedonian:
			return L"Macedonian";
		case LANGUAGE_Malay:
			return L"Malay";
		case LANGUAGE_Maltese:
			return L"Maltese";
		case LANGUAGE_Norwegian:
			return L"Norwegian";
		case LANGUAGE_Pashto:
			return L"Pashto";
		case LANGUAGE_Persian:
			return L"Persian";
		case LANGUAGE_Polish:
			return L"Polish";
		case LANGUAGE_Queretaro_Otomi:
			return L"Queretaro Otomi";
		case LANGUAGE_Portuguese:
			return L"Portuguese";
		case LANGUAGE_Romanian:
			return L"Romanian";
		case LANGUAGE_Russian:
			return L"Russian";
		case LANGUAGE_Serbian:
			return L"Serbian";
		case LANGUAGE_Somali:
			return L"Somali";
		case LANGUAGE_Slovak:
			return L"Slovak";
		case LANGUAGE_Slovenian:
			return L"Slovenian";
		case LANGUAGE_Spanish:
			return L"Spanish";
		case LANGUAGE_Swahili:
			return L"Swahili";
		case LANGUAGE_Swedish:
			return L"Swedish";
		case LANGUAGE_Thai:
			return L"Thai";
		case LANGUAGE_Turkish:
			return L"Turkish";
		case LANGUAGE_Ukrainian:
			return L"Ukrainian";
		case LANGUAGE_Urdu:
			return L"Urdu";
		case LANGUAGE_Vietnamese:
			return L"Vietnamese";
		case LANGUAGE_Welsh:
			return L"Welsh";
		case LANGUAGE_Yiddish:
			return L"Yiddish";
		case LANGUAGE_Yucatec_Maya:
			return L"Yucatec Maya";
		case LANGUAGE_Zulu:
			return L"Zulu";
		default:
			return 0;
	}
};

void SetDlgButton(HWND hWndDlg, unsigned int id, int pressed)
{
	if (pressed)
		CheckDlgButton(hWndDlg, id, BST_CHECKED);
	else
		CheckDlgButton(hWndDlg, id, BST_UNCHECKED);
}

int GetExeNames(wchar_t **exeNamePlusFolder, wchar_t **exeName, wchar_t *exePath)
{
	exeNamePlusFolder[0] = exeName[0] = wcsrchr(exePath, '\\');
	if (!exeName[0]) return 0;

	while (exeNamePlusFolder[0] > exePath && exeNamePlusFolder[0][-1] !='\\' && exeNamePlusFolder[0][-1] !=':')
		exeNamePlusFolder[0]--;
	exeName[0]++;
	return 1;
}

wchar_t *unescape(wchar_t *in)
{
	wchar_t *out = wcsdup(in);
	int i, j=0;
	for (i=0; in[i]; i++)
	{
		wchar_t c = in[i];
		if (c == '\\')
		{
			c = in[++i];
			if (!c)
				break;
			else if (c == 'n')
				c = '\n';
			else if (c == 't')
				c = '\t';
		}
		out[j++] = c;
	}
	out[j] = 0;
	return out;
}

wchar_t *escape(wchar_t *in)
{
	wchar_t *out = (wchar_t *) malloc(sizeof(wchar_t)*(1+2*wcslen(in)));
	int j=0;
	for (int i=0; in[i]; i++)
		if (in[i] == '\n')
		{
			out[j++] = '\\';
			out[j++] = 'n';
		}
		else if (in[i] == '\t')
		{
			out[j++] = '\\';
			out[j++] = 't';
		}
		else if (in[i] == '\\' /*|| in[i] == '(' || in[i] == ')' || in[i] == '[' || in[i] == ']' || in[i] == '.'//*/)
		{
			out[j++] = '\\';
			out[j++] = in[i];
		}
		else
			out[j++] = in[i];
	out[j] = 0;
	return out;
}

int ReplaceList::AddProfile(wchar_t *profile)
{
	int i = FindProfile(profile);
	if (i >= 0) return i;

	if (numLists % 16 == 0)
		lists = (ReplaceSubList*) realloc(lists, sizeof(ReplaceSubList) * (numLists+16));
	i = numLists ++;
	memset(lists+i, 0, sizeof(lists[i]));
	wcscpy(lists[i].profile, profile);
	return i;
}

int ReplaceList::FindProfile(wchar_t *profile)
{
	for (int i=0; i<numLists; i++)
		if (!wcsicmp(profile, lists[i].profile))
			return i;
	return -1;
}

int ReplaceSubList::DeleteString(wchar_t *old)
{
	int i = FindString(old);
	if (i < 0) return 0;
	free(strings[i].old);
	for (int j=i+1; j<numStrings; j++)
		strings[j-1] = strings[j];
	numStrings--;
	return 1;
}

void ReplaceSubList::Clear()
{
	for (int i=0; i<numStrings; i++)
		free(strings[i].old);
	free(strings);
	strings = 0;
	numStrings = 0;
}

ReplaceString *ReplaceSubList::AddString(wchar_t *old, wchar_t *rep)
{
	DeleteString(old);
	if (numStrings % 128 == 0)
		strings = (ReplaceString*) realloc(strings, sizeof(ReplaceString) * (numStrings+128));
	int i;
	for (i=numStrings; i> 0; i--)
	{
		if (wcsicmp(old, strings[i-1].old) >= 0) break;
		strings[i] = strings[i-1];
	}
	int L1 = wcslen(old);
	int L2 = wcslen(rep);
	strings[i].old = (wchar_t *) malloc(sizeof(wchar_t) * (L1+L2+2));
	strings[i].rep = strings[i].old + L1+1;
	strings[i].len = L1;
	wcscpy(strings[i].old, old);
	wcscpy(strings[i].rep, rep);
	numStrings++;
	return strings+i;
}

int __cdecl compareLists(const void *l1, const void *l2)
{
	ReplaceSubList *list1 = (ReplaceSubList *)l1;
	ReplaceSubList *list2 = (ReplaceSubList *)l2;

	if (list1->active != list2->active)
		return list2->active - list1->active;
	return wcsicmp(list1->profile, list2->profile);
}

void FormatProfileName(ReplaceSubList *list, wchar_t *out)
{
	if (!list->numStrings)
		wcscpy(out, list->profile);
	else if (list->numStrings == 1)
		wsprintf(out, L"%s: 1 line", list->profile);
	else
		wsprintf(out, L"%s: %i lines", list->profile, list->numStrings);
}

int ReplaceList::FindActiveAndPopulateCB(HWND hWndCombo)
{
	if (hWndCombo)
	{
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
				// Creates a corresponding profile, if one doesn't exist.
				config.replace.AddProfile(data.cFileName);
			}
			while (FindNextFile(hFind, &data));
			FindClose(hFind);
		}
	}

	for (int i=0; i<numLists; i++)
	{
		lists[i].active = 0;
		if (!wcscmp(lists[i].profile, L"*"))
			lists[i].active = 2;
	}

	DWORD pids[10000];
	DWORD ret;
	getDebugPriv();
	if (EnumProcesses(pids, sizeof(pids), &ret) && (ret/=4))
	{
		wchar_t name[MAX_PATH*2];
		for (DWORD i=0; i<ret; i++)
		{
			HANDLE hProcess;
			if (hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, 0, pids[i]))
			{
				if (GetModuleFileNameEx(hProcess, 0, name, sizeof(name)/sizeof(name[0])))
				{
					wchar_t *exeNamePlusFolder, *exeName;
					int active = 0;
					if (GetExeNames(&exeNamePlusFolder, &exeName, name))
					{
						for (int i=0; i<numLists; i++)
						{
							if (!wcsicmp(exeNamePlusFolder, lists[i].profile))
							{
								lists[i].active = 1;
								active = 1;
								break;
							}
						}
					}
				}
				CloseHandle(hProcess);
			}
		}
	}

	qsort(lists, numLists, sizeof(lists[0]), compareLists);
	int index = 0;
	if (hWndCombo)
	{
		for (int i=0; i<numLists; i++)
		{
			wchar_t temp[2 * MAX_PATH];
			FormatProfileName(&lists[i], temp);
			SendMessage(hWndCombo, CB_ADDSTRING, 0, (LPARAM)temp);
			if (lists[i].active == 1)
				if (!index)
					index = i;
				else
					if (lists[i].numStrings > lists[index].numStrings)
						index = i;
		}
	}
	return index;
}

ReplaceString *ReplaceList::AddString(wchar_t *old, wchar_t *rep, wchar_t *profile)
{
	int i = AddProfile(profile);
	return lists[i].AddString(old, rep);
}

void ReplaceList::DeleteString(wchar_t *old, wchar_t *profile)
{
	int i = FindProfile(profile);
	if (i >= 0)
		lists[i].DeleteString(old);
}


int ReplaceSubList::FindString(wchar_t *old)
{
#if 1
	int min = 0;
	int max = numStrings-1;
	while (min <= max)
	{
		int mid = (min+max)/2u;
		int cmp = wcsicmp(old, strings[mid].old);
		if (cmp > 0) min = mid+1;
		else if (cmp < 0) max = mid-1;
		else return mid;
	}
#else
	for (int i=0; i<numStrings; i++)
		if (!wcsicmp(old, strings[i].old))
			return i;
#endif
	return -1;
}

void ReplaceList::Save()
{
	HANDLE hFile = CreateFileW(path, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
	if (hFile != INVALID_HANDLE_VALUE)
	{
		wchar_t sig = 0xFEFF;
		wchar_t LF[] = L"\n\n";
		DWORD junk;
		WriteFile(hFile, &sig, 2, &junk, 0);
		for (int list=0; list<numLists; list++)
		{
			if (!lists[list].numStrings) continue;
			WriteFile(hFile, L"Profile:", sizeof(wchar_t)*8, &junk, 0);
			WriteFile(hFile, lists[list].profile, sizeof(wchar_t) * wcslen(lists[list].profile), &junk, 0);
			WriteFile(hFile, LF, sizeof(wchar_t), &junk, 0);
			for (int i=0; i<lists[list].numStrings; i++)
			{
				wchar_t *temp = escape(lists[list].strings[i].old);
				WriteFile(hFile, temp, sizeof(wchar_t)*wcslen(temp), &junk, 0);
				WriteFile(hFile, LF, sizeof(wchar_t), &junk, 0);
				free(temp);

				temp = escape(lists[list].strings[i].rep);
				WriteFile(hFile, temp, sizeof(wchar_t)*wcslen(temp), &junk, 0);
				WriteFile(hFile, LF, 2*sizeof(wchar_t), &junk, 0);
				free(temp);
			}
		}
		CloseHandle(hFile);
	}
}

void ReplaceList::Load()
{
	Clear();
	wchar_t *data;
	int size;
	LoadFile(path, &data, &size);
	wchar_t profile[MAX_PATH] = L"*";
	config.replace.AddProfile(profile);
	if (data)
	{
		wchar_t *pos = data;
		wchar_t *last = 0;
		int final = 0;
		while (pos)
		{
			wchar_t *end = wcschr(pos, '\n');
			if (!end)
			{
				end = wcschr(pos, 0);
				final = 1;
			}
			*end = 0;
			if (!last)
			{
				if (!wcsnicmp(pos, L"Profile:", 8))
				{
					pos += 8;
					while (pos[0] == ' ') pos++;
					if (end-pos < MAX_PATH)
						wcscpy(profile, pos);
				}
				else
					if (pos[0])
						last = pos;
			}
			else
			{
				if (pos-last-1 > 1000) last[1000] = 0;
				if (end-pos > 1000) pos[1000] = 0;
				wchar_t *old = unescape(last);
				wchar_t *replace = unescape(pos);
				ReplaceList::AddString(old, replace, profile);
				free(replace);
				free(old);
				last = 0;
			}

			if (final) break;
			pos = end+1;
		}
		free(data);
	}
}

ReplaceString *ReplaceList::FindReplacement(wchar_t *string)
{
	// Note that active lists are brought to the top.
	ReplaceString *best = 0;
	int bestLen = 0;
	for (int i=0; i<numLists && lists[i].active; i++)
	{
		ReplaceString *res = lists[i].FindReplacement(string);
		// Keep last hit, as generic '*' list is kept up top.
		if (res && res->len >= bestLen)
		{
			best = res;
			bestLen = res->len;
		}
	}
	return best;
}

ReplaceString *ReplaceSubList::FindReplacement(wchar_t *string)
{
	if (!numStrings) return 0;
	ReplaceString *best = 0;
	int min = 0;
	int max = numStrings-1;
	wchar_t *jap;
	while (min < max)
	{
		int mid = (min+max+1)/2;
		jap = strings[mid].old;
		int cmp = wcsicmp(string, jap);
		if (cmp >= 0) min = mid;
		else if (cmp < 0) max = mid-1;
	}
	int last = max;
	int len = 1;

	while (1)
	{
		max = last;
		if (len)
		{
			while (min < max)
			{
				int mid = (min+max)/2;
				jap = strings[mid].old;
				// All magically long enough for this and first len-1 characters all match.
				int cmp = wcsnicmp(string+len-1, jap+len-1, 1);
				if (cmp > 0) min = mid+1;
				else if (cmp <= 0) max = mid;
			}
		}
		max = last;

		if (max <= min)
		{
			jap = strings[min].old;
			if (max < min || wcsnicmp(string+len-1, jap+len-1, 1))
				break;
		}

		if (strings[min].len == len)
		{
			best = &strings[min];
			min++;
			if (max < min) break;
		}
		len++;
	}
	return best;
}

void AddString(HWND hWndList, ReplaceString *s)
{
	LVITEMW item;
	item.mask = LVIF_TEXT;
	item.iItem = 0;

	LV_FINDINFOW find;
	memset(&find, 0, sizeof(find));
	find.flags = LVFI_STRING;
	find.psz = item.pszText = escape(s->old);
	int index = ListView_FindItem(hWndList, -1, &find);

	if (index < 0)
	{
		item.iSubItem = 0;
		index = ListView_InsertItem(hWndList, &item);
	}
	free(item.pszText);

	item.mask = LVIF_TEXT;
	item.iItem = index;
	item.pszText = escape(s->rep);
	item.iSubItem = 1;
	ListView_SetItem(hWndList, &item);
	free(item.pszText);
}

wchar_t *GetSelectedProfileName(HWND hWndCombo, wchar_t * out)
{
	int allocated = 0;
	if (!out)
	{
		out = (wchar_t*) malloc(sizeof(wchar_t) * 2 * MAX_PATH);
		allocated = 1;
	}
	int cbIndex = SendMessage(hWndCombo, CB_GETCURSEL, 0, 0);
	if (cbIndex >= 0 && SendMessage(hWndCombo, CB_GETLBTEXT, cbIndex, (LPARAM)out))
	{
		wchar_t *c = wcsrchr(out, ':');
		if (c) *c = 0;
		return out;
	}
	if (allocated) free(out);
	return 0;
}

int GetSelectedProfileId(HWND hWndCombo)
{
	wchar_t profile[MAX_PATH*2];
	if (GetSelectedProfileName(hWndCombo, profile))
		return config.replace.FindProfile(profile);
	return -1;
}

void ClearProfile(HWND hWndList, HWND hWndCombo)
{
	int i = GetSelectedProfileId(hWndCombo);
	if (i >= 0)
	{
		ListView_DeleteAllItems(hWndList);
		config.replace.lists[i].Clear();
	}
}

void UpdateList(HWND hWndList, HWND hWndCombo)
{
	ListView_DeleteAllItems(hWndList);
	int i = GetSelectedProfileId(hWndCombo);
	if (i >= 0)
		for (int j=0; j<config.replace.lists[i].numStrings; j++)
			AddString(hWndList, &config.replace.lists[i].strings[j]);
}

void DeleteSelected(HWND hWnd, HWND hWndList, HWND hWndCombo)
{
	int index = -1;
	int changed = 0;

	wchar_t profile[MAX_PATH*2];

	if (!GetSelectedProfileName(hWndCombo, profile))
		// ???
		return;

	while (1)
	{
		index = SendMessage(hWndList, LVM_GETNEXTITEM, index, LVNI_SELECTED);
		if (index < 0) break;
		wchar_t text[10000];
		LVITEM item;
		item.iItem = index;
		item.cchTextMax = sizeof(text)/sizeof(wchar_t);
		item.pszText = text;
		item.iSubItem = 0;
		int len = SendMessage(hWndList, LVM_GETITEMTEXT, index, (LPARAM) &item);
		if (len > 0)
		{
			wchar_t *old = unescape(text);
			config.replace.DeleteString(old, profile);
			free(old);

			SendMessage(hWndList, LVM_DELETEITEM, index, 0);
			index--;
			changed = 1;
		}
	}
	if (changed)
		EnableWindow(GetDlgItem(hWnd, IDAPPLY), 1);
}

INT_PTR CALLBACK ConfigDialogProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	HWND hWndList = GetDlgItem(hWnd, IDC_SUBSTITUTION_LIST);
	HWND hWndCombo = GetDlgItem(hWnd, IDC_PROFILE_NAME);
	switch (uMsg)
	{
		case WM_INITDIALOG:
		{
			SendMessage(hWndList, LVM_SETEXTENDEDLISTVIEWSTYLE, 0, LVS_EX_FULLROWSELECT);

			LVCOLUMN c;
			c.mask = LVCF_TEXT | LVCF_WIDTH;
			c.cx = 171;
			c.pszText = L"Original Text";
			ListView_InsertColumn(hWndList, 0, &c);
			c.cx = 171;
			c.pszText = L"New Text";
			ListView_InsertColumn(hWndList, 1, &c);

			int index = config.replace.FindActiveAndPopulateCB(hWndCombo);

			SendMessage(hWndCombo, CB_SETCURSEL, index, 0);
			UpdateList(hWndList, hWndCombo);
			return 1;
		}
		case WM_NOTIFY:
		{
			NMHDR *n = (NMHDR *)lParam;
			if (n->idFrom == IDC_SUBSTITUTION_LIST)
			{
				if (n->code == LVN_KEYDOWN)
				{
					NMLVKEYDOWN *n = (NMLVKEYDOWN*)lParam;
					if (n->wVKey == VK_DELETE || n->wVKey == VK_BACK)
						DeleteSelected(hWnd, hWndList, hWndCombo);
					return 0;
				}
				else if (n->code == LVN_ITEMCHANGED)
				{
					NMITEMACTIVATE* n = (NMITEMACTIVATE*)lParam;
					int index = n->iItem;
					if (index >= 0 && (n->uNewState & LVIS_SELECTED) && !(n->uOldState & LVIS_SELECTED))
					{
						wchar_t old[1001];
						wchar_t replace[1001];
						ListView_GetItemText(hWndList, index, 0, old, sizeof(old)/sizeof(wchar_t));
						ListView_GetItemText(hWndList, index, 1, replace, sizeof(replace)/sizeof(wchar_t));
						SetDlgItemText(hWnd, IDC_OLD, old);
						SetDlgItemText(hWnd, IDC_REPLACE, replace);
					}

					return 0;
				}
			}
			return 1;
		}
		case WM_COMMAND:
		{

			int ctrl = LOWORD(wParam);
			int action = HIWORD(wParam);
			if (action == CBN_SELCHANGE)
				UpdateList(hWndList, hWndCombo);
			else if (action == BN_CLICKED)
			{
				if (ctrl == IDCANCEL)
				{
					config.replace.Load();
					EndDialog(hWnd, 1);
					return 1;
				}
				else if (ctrl == IDOK || ctrl == IDAPPLY)
				{
					/*
					config.replace.Clear();
					int count = ListView_GetItemCount(hWndList);
					for (int i=0; i<count; i++)
					{
						wchar_t old[1001];
						wchar_t replace[1001];
						ListView_GetItemText(hWndList, i, 0, old, sizeof(old)/sizeof(wchar_t));
						ListView_GetItemText(hWndList, i, 1, replace, sizeof(replace)/sizeof(wchar_t));
						wchar_t *old2 = unescape(old);
						wchar_t *replace2 = unescape(replace);
						config.replace.AddString(old2, replace2, profile);
						free(old2);
						free(replace2);
					}
					//*/
					CreateDirectoryW(L"Dictionaries", 0);
					config.replace.Save();
					if (ctrl == IDOK) EndDialog(hWnd, 1);
					else EnableWindow(GetDlgItem(hWnd, IDAPPLY), 0);
					history.ClearTranslations();
					return 1;
				}
				else if (ctrl == ID_DELETE)
					DeleteSelected(hWnd, hWndList, hWndCombo);
				else if (ctrl == ID_CLEAR)
					ClearProfile(hWndList, hWndCombo);
				else if (ctrl == ID_ADD)
				{
					HWND hOld = GetDlgItem(hWnd, IDC_OLD);
					HWND hReplace = GetDlgItem(hWnd, IDC_REPLACE);
					int len = GetWindowTextLength(hOld);
					if (!len) return 1;
					wchar_t *old = (wchar_t*)malloc((len+1)*sizeof(wchar_t));
					len = GetWindowText(hOld, old, len+1);
					if (!len)
					{
						free(old);
						return 1;
					}
					if (len > 1000) old[1000] = 0;

					int len2 = GetWindowTextLength(hReplace);
					wchar_t *rep = (wchar_t*)malloc((len2+1)*sizeof(wchar_t));
					len2 = GetWindowText(hReplace, rep, len2+1);
					rep[len2] = 0;
					if (len2 > 1000) rep[1000] = 0;


					wchar_t profile[MAX_PATH*2];
					if (GetSelectedProfileName(hWndCombo, profile))
					{
						wchar_t *old2 = unescape(old);
						wchar_t *rep2 = unescape(rep);
						ReplaceString *string = config.replace.AddString(old2, rep2, profile);
						free(old2);
						free(rep2);
						if (string) AddString(hWndList, string);
						EnableWindow(GetDlgItem(hWnd, IDAPPLY), 1);
					}
					free(old);
					free(rep);
				}
			}
		}
		default:
			break;
	}
	return 0;
}

void ConfigDialog(HWND hWnd)
{
	DialogBox(ghInst, MAKEINTRESOURCE(IDD_OPTIONS_DIALOG), hWnd, ConfigDialogProc);
}

void GetPaths()
{
	int len = GetCurrentDirectoryW(sizeof(config.myPath)/2, config.myPath);
	if (len + wcslen(INI) + 2 >= sizeof(config.ini)/2)
	{
		wcscpy(config.ini, INI);
		return;
	}
	wchar_t ini2[MAX_PATH*2];
	wcscpy(config.ini, config.myPath);
	wcscpy(ini2, config.myPath);
	wcscat(ini2, L"\\Translation Aggretator.ini");
	wcscat(config.ini, L"\\" INI);
	MoveFileW(ini2, config.ini);
}

void UnloadPlugin(int i)
{
	if (i >= 0 && i < config.numPlugins)
	{
		FreeLibrary(config.plugins[i].hDll);
		while (i+1 < config.numPlugins)
		{
			config.plugins[i] = config.plugins[i+1];
			i++;
		}
		config.numPlugins--;
	}
	if (!config.numPlugins)
	{
		free(config.plugins);
		config.plugins = 0;
	}
}

void UnloadPlugins()
{
	for (int i=config.numPlugins-1; i>=0; i--)
		UnloadPlugin(i);
}

void LoadPlugins()
{
	WIN32_FIND_DATAW data;
	HANDLE hFind = FindFirstFileW(L"plugins\\*.dll", &data);
	static TAInfo taInfo;
	taInfo.size = sizeof(taInfo);
	int version[3];
	swscanf(APP_VERSION, L"%i.%i.%i", version, version+1, version+2);
	taInfo.version = (version[0]<<16) + (version[1]<<8) + version[2];
	if (hFind !=INVALID_HANDLE_VALUE)
	{
		do
		{
			if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
			wchar_t temp[2*MAX_PATH];
			wsprintf(temp, L"plugins\\%s", data.cFileName);
			HMODULE hMod = LoadLibraryEx(temp, 0, LOAD_WITH_ALTERED_SEARCH_PATH);
			if (!hMod) continue;
			TAPluginGetVersionType *PluginGetVersion = (TAPluginGetVersionType*) GetProcAddress(hMod, "TAPluginGetVersion");
			if (!PluginGetVersion)
			{
				FreeLibrary(hMod);
				continue;
			}
			unsigned int version = PluginGetVersion(0);
			if (!version || version > TA_PLUGIN_VERSION)
			{
				FreeLibrary(hMod);
				continue;
			}
			config.plugins = (PluginInfo*)realloc(config.plugins, sizeof(PluginInfo) * (config.numPlugins+1));
			PluginInfo *plugin = (PluginInfo*)&config.plugins[config.numPlugins++];
			plugin->hDll = hMod;
			plugin->Initialize = (TAPluginInitializeType*) GetProcAddress(hMod, "TAPluginInitialize");
			plugin->ModifyStringPostSubstitution = (TAPluginModifyStringPostSubstitutionType*) GetProcAddress(hMod, "TAPluginModifyStringPostSubstitution");
			plugin->ModifyStringPreSubstitution = (TAPluginModifyStringPreSubstitutionType*) GetProcAddress(hMod, "TAPluginModifyStringPreSubstitution");
			plugin->ActiveProfileList = (TAPluginActiveProfileListType*) GetProcAddress(hMod, "TAPluginActiveProfileList");
			plugin->Free = (TAPluginFreeType*) GetProcAddress(hMod, "TAPluginFree");
			void *junk;
			if (plugin->Initialize && !plugin->Initialize(&taInfo, &junk))
				UnloadPlugin(config.numPlugins-1);
		}
		while (FindNextFile(hFind, &data));
		FindClose(hFind);
	}
}

void WritePrivateProfileInt(wchar_t *v1, wchar_t *v2, int i, wchar_t *v3)
{
	wchar_t val[15];
	_itow(i, val, 10);
	WritePrivateProfileString(v1, v2, val, v3);
}

void SaveGeneralConfig()
{
	WritePrivateProfileInt(L"General", L"Port", config.port, config.ini);

	WritePrivateProfileInt(L"General", L"Flags", config.flags, config.ini);

	WritePrivateProfileString(L"General", L"Font Face", config.font.face, config.ini);
	WritePrivateProfileInt(L"General", L"Font Height", config.font.height, config.ini);
	WritePrivateProfileInt(L"General", L"Font Color", config.font.color, config.ini);
	WritePrivateProfileInt(L"General", L"Bold", config.font.bold, config.ini);
	WritePrivateProfileInt(L"General", L"Italic", config.font.italic, config.ini);
	WritePrivateProfileInt(L"General", L"Underline", config.font.underline, config.ini);
	WritePrivateProfileInt(L"General", L"StrikeOut", config.font.strikeout, config.ini);

	WritePrivateProfileString(L"General", L"Tooltip Font Face", config.toolTipFont.face, config.ini);
	WritePrivateProfileInt(L"General", L"Tooltip Font Height", config.toolTipFont.height, config.ini);
	WritePrivateProfileInt(L"General", L"Tooltip Font Color", config.toolTipFont.color, config.ini);
	WritePrivateProfileInt(L"General", L"Tooltip Bold", config.toolTipFont.bold, config.ini);
	WritePrivateProfileInt(L"General", L"Tooltip Italic", config.toolTipFont.italic, config.ini);
	WritePrivateProfileInt(L"General", L"Tooltip Underline", config.toolTipFont.underline, config.ini);
	WritePrivateProfileInt(L"General", L"Tooltip StrikeOut", config.toolTipFont.strikeout, config.ini);

	WritePrivateProfileInt(L"General", L"AutoHalfWidthToFull", config.autoHalfWidthToFull, config.ini);

	WritePrivateProfileInt(L"General", L"toolTipKanji", config.toolTipKanji, config.ini);
	WritePrivateProfileInt(L"General", L"toolTipHiragana", config.toolTipHiragana, config.ini);
	WritePrivateProfileInt(L"General", L"toolTipParen", config.toolTipParen, config.ini);
	WritePrivateProfileInt(L"General", L"toolTipConj", config.toolTipConj, config.ini);

	WritePrivateProfileInt(L"General", L"GlobalHotkeys", config.global_hotkeys, config.ini);;

	WritePrivateProfileInt(L"JParser", L"HideCrossRefs", config.jParserHideCrossRefs, config.ini);
	WritePrivateProfileInt(L"JParser", L"HideUsage", config.jParserHideUsage, config.ini);
	WritePrivateProfileInt(L"JParser", L"HidePos", config.jParserHidePos, config.ini);

	WritePrivateProfileInt(L"JParser", L"DefinitionLines", config.jParserDefinitionLines, config.ini);
	WritePrivateProfileInt(L"JParser", L"ReformatNumbers", config.jParserReformatNumbers, config.ini);
	WritePrivateProfileInt(L"JParser", L"NoKanaBrackets", config.jParserNoKanaBrackets, config.ini);

	WritePrivateProfileInt(L"JParser", L"JParserFlags", config.jParserFlags, config.ini);

	WritePrivateProfileString(L"General", L"langSrc", GetLanguageString(config.langSrc), config.ini);
	WritePrivateProfileString(L"General", L"langDst", GetLanguageString(config.langDst), config.ini);

}

void LoadGeneralConfig()
{
	GetPaths();

	CreateDirectoryW(L"Dictionaries", 0);
	wcscpy(config.replace.path, L"Dictionaries\\UserDict.txt");
	config.replace.Load();

	config.flags = GetPrivateProfileInt(L"General", L"Flags", CONFIG_ENABLE_SUBSTITUTIONS, config.ini);

	config.port = GetPrivateProfileInt(L"General", L"Port", 56393, config.ini);

	GetPrivateProfileString(L"General", L"Font Face", L"Arial", config.font.face, 32, config.ini);
	config.font.height = GetPrivateProfileInt(L"General", L"Font Height", 195, config.ini);
	config.font.color = GetPrivateProfileInt(L"General", L"Font Color", RGB(0,0,0), config.ini);
	config.font.bold = GetPrivateProfileInt(L"General", L"Bold", 0, config.ini);
	config.font.italic = GetPrivateProfileInt(L"General", L"Italic", 0, config.ini);
	config.font.underline = GetPrivateProfileInt(L"General", L"Underline", 0, config.ini);
	config.font.strikeout = GetPrivateProfileInt(L"General", L"StrikeOut", 0, config.ini);

	NONCLIENTMETRICS ncm;
	memset(&ncm, 0, sizeof(ncm));
	ncm.cbSize = sizeof(NONCLIENTMETRICS);
	if (!SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(NONCLIENTMETRICS), &ncm, 0))
	{
		memset(&ncm, 0, sizeof(ncm));
		wcscpy(ncm.lfStatusFont.lfFaceName, L"Arial");
		ncm.lfStatusFont.lfHeight = -11;
		ncm.lfStatusFont.lfWeight = 400;
	}

	GetPrivateProfileString(L"General", L"Tooltip Font Face", ncm.lfStatusFont.lfFaceName, config.toolTipFont.face, 32, config.ini);
	config.toolTipFont.height = GetPrivateProfileInt(L"General", L"Tooltip Font Height", -15 * ncm.lfStatusFont.lfHeight, config.ini);
	config.toolTipFont.color = GetPrivateProfileInt(L"General", L"Tooltip Font Color", GetSysColor(COLOR_INFOTEXT), config.ini);
	config.toolTipFont.bold = GetPrivateProfileInt(L"General", L"Tooltip Bold", ncm.lfStatusFont.lfWeight > 500, config.ini);
	config.toolTipFont.italic = GetPrivateProfileInt(L"General", L"Tooltip Italic", ncm.lfStatusFont.lfItalic, config.ini);
	config.toolTipFont.underline = GetPrivateProfileInt(L"General", L"Tooltip Underline", ncm.lfStatusFont.lfUnderline, config.ini);
	config.toolTipFont.strikeout = GetPrivateProfileInt(L"General", L"Tooltip StrikeOut", ncm.lfStatusFont.lfStrikeOut, config.ini);

	config.autoHalfWidthToFull = GetPrivateProfileInt(L"General", L"AutoHalfWidthToFull", 1, config.ini);

	config.toolTipKanji = GetPrivateProfileInt(L"General", L"toolTipKanji", RGB(160,0,0), config.ini);
	config.toolTipHiragana = GetPrivateProfileInt(L"General", L"toolTipHiragana", RGB(30,160,30), config.ini);
	config.toolTipParen = GetPrivateProfileInt(L"General", L"toolTipParen", RGB(100,170,230), config.ini);
	config.toolTipConj = GetPrivateProfileInt(L"General", L"toolTipConj", RGB(150,150,150), config.ini);

	config.global_hotkeys = (GetPrivateProfileInt(L"General", L"GlobalHotkeys", 0, config.ini) > 0);

	config.jParserHideCrossRefs = GetPrivateProfileInt(L"JParser", L"HideCrossRefs", 1, config.ini);
	config.jParserHideUsage = GetPrivateProfileInt(L"JParser", L"HideUsage", 0, config.ini);
	config.jParserHidePos = GetPrivateProfileInt(L"JParser", L"HidePos", 0, config.ini);

	config.jParserDefinitionLines = GetPrivateProfileInt(L"JParser", L"DefinitionLines", 0, config.ini);
	config.jParserReformatNumbers = GetPrivateProfileInt(L"JParser", L"ReformatNumbers", 0, config.ini);
	config.jParserNoKanaBrackets = GetPrivateProfileInt(L"JParser", L"NoKanaBrackets", 0, config.ini);

	config.jParserFlags = GetPrivateProfileIntW(L"JParser", L"JParserFlags", JPARSER_USE_MECAB | JPARSER_DISPLAY_VERB_CONJUGATIONS, config.ini);

	config.langSrc = LANGUAGE_Japanese;
	config.langDst = LANGUAGE_English;

	wchar_t src[MAX_PATH];
	wchar_t dst[MAX_PATH];

	GetPrivateProfileString(L"General", L"langSrc", L"Japanese", src, MAX_PATH, config.ini);
	GetPrivateProfileString(L"General", L"langDst", L"English", dst, MAX_PATH, config.ini);
	for (int i=0; i<LANGUAGE_NONE; i++)
	{
		wchar_t *lang = GetLanguageString((Language)i);
		if (!wcsicmp(lang, src))
			config.langSrc = (Language)i;
		if (!wcsicmp(lang, dst) && i != LANGUAGE_AUTO)
			config.langDst = (Language)i;
	}

	//GetPrivateProfileString(L"General", L"Font Face", L"Serif", config.font, 32, config.ini);
	//config.fontHeight = GetPrivateProfileInt(L"General", L"Font Height", 200, config.ini);
	if (!config.hIdFont)
		config.hIdFont = CreateFont(19, 0, 0, 0, 500, 0, 0, 0, 0, 0, 0, 0, 0, L"Arial");
	LoadPlugins();

}

int GetConfigPath(wchar_t *path, wchar_t *name)
{
	wcscpy(path, config.ini);
	if (!wcsicmp(name, L"::System"))
		return 1;
	wchar_t *r = wcsrchr(path, '\\');
	if (!r) return 0;
	r++;
	wsprintfW(r, L"Game Configs\\%s", name);
	r = wcschr(r, '\\')+1;
	while (r = wcschr(r, '\\'))
		*r = '+';
	wcscat(path, L".ini");

	HANDLE hFile = CreateFile(path, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
	if (hFile == INVALID_HANDLE_VALUE)
		return 0;
	else CloseHandle(hFile);
	return 1;
}

int LoadInjectionSettings(InjectionSettings &cfg, wchar_t *name, wchar_t *newPath, std::wstring* internalHooks)
{
	// Can be called without calling load general configs first.
	GetPaths();
	memset(&cfg, 0, sizeof(cfg));
	wchar_t path[2*MAX_PATH];
	wcscpy(path, config.ini);
	int res = 2;
	int clipboard = 1;

	if (name)
	{
		wchar_t *exeNamePlusFolder = wcsrchr(name,'\\');
		if (!exeNamePlusFolder)
			exeNamePlusFolder = name;
		while (exeNamePlusFolder > name && exeNamePlusFolder[-1] !='\\' && exeNamePlusFolder[-1] !=':')
			exeNamePlusFolder--;
		name = exeNamePlusFolder;
	}

	if (name && wcsicmp(name, L"Default") && wcsicmp(name, L"::System"))
	{
		clipboard = 0;
		if (!GetConfigPath(path, name))
		{
			res = 1;
			wcscpy(path, config.ini);
		}
	}

	cfg.forceLocale = GetPrivateProfileInt(L"Launch", L"Language", 0x0411, path);

	cfg.atlasConfig.flags = GetPrivateProfileInt(L"Atlas", L"Break On", ~BREAK_ON_SINGLE_LINE_BREAKS, path);
	GetPrivateProfileString(L"Atlas", L"Environment", L"General", cfg.atlasConfig.environment, sizeof(cfg.atlasConfig.environment)/sizeof(wchar_t), path);
	GetPrivateProfileString(L"Atlas", L"Rule Set", L"", cfg.atlasConfig.trsPath, sizeof(cfg.atlasConfig.trsPath)/sizeof(wchar_t), path);
	if (!GetPrivateProfileStruct(L"Injection", L"Exe Path Struct", cfg.exePath, sizeof(cfg.exePath), path))
		GetPrivateProfileString(L"Injection", L"Exe Path", L"", cfg.exePath, sizeof(cfg.exePath)/sizeof(wchar_t), path);

	GetPrivateProfileString(L"Injection", L"AGTH params", L"", cfg.agthParams, sizeof(cfg.agthParams)/sizeof(wchar_t), path);

	cfg.injectionFlags = GetPrivateProfileInt(L"Injection", L"Mode", TRANSLATE_MENUS, path);
	cfg.agthFlags = GetPrivateProfileInt(L"Injection", L"AGTH Flags", FLAG_AGTH_ADD_PARAMS|FLAG_AGTH_COPYDATA, path);

	cfg.agthSymbolRepeatCount = GetPrivateProfileInt(L"Injection", L"AGTH Repeat", 1, path);
	cfg.agthPhraseCount1 = GetPrivateProfileInt(L"Injection", L"AGTH KF 1", 32, path);
	cfg.agthPhraseCount2 = GetPrivateProfileInt(L"Injection", L"AGTH KF 2", 16, path);

	cfg.internalHookDelay = GetPrivateProfileInt(L"Injection", L"Hook Delay", 3, path);

	if (clipboard)
		cfg.internalHookTime = GetPrivateProfileInt(L"Injection", L"Hook Time", 0, path);
	else
		cfg.internalHookTime = GetPrivateProfileInt(L"Injection", L"Hook Time", 250, path);

	cfg.maxLogLen = GetPrivateProfileInt(L"Injection", L"Log Len", 1024, path);
	if (internalHooks)
	{
		wchar_t temp[10000];
		GetPrivateProfileString(L"Injection", L"Internal Hooks", L"", temp, sizeof(temp)/sizeof(wchar_t), path);
		*internalHooks = wcsdup(temp);
	}

	if (newPath)
		wcscpy(cfg.exePath, newPath);
	GetExeNames(&cfg.exeNamePlusFolder, &cfg.exeName, cfg.exePath);

	cfg.logPath[0] = 0;
	if (cfg.injectionFlags & LOG_TRANSLATIONS)
	{
		wchar_t logPath[2*MAX_PATH];
		wcscpy(logPath, config.ini);
		wchar_t *s = wcsrchr(logPath, '\\');
		if (s)
		{
			s++;
			wcscpy(s, L"log");
			CreateDirectory(logPath, 0);
			s[3] = '\\';
			s += 4;
			wcscpy(s, cfg.exeNamePlusFolder);
			wcscat(s, L".log");
			s = wcsrchr(s, '\\');
			if (s) *s = '_';
			if (wcslen(logPath) < MAX_PATH)
				wcscpy(cfg.logPath, logPath);
		}
	}

	return res;
}

int LoadInjectionContextSettings(wchar_t *name, wchar_t *contextName, ContextSettings *settings)
{
	wchar_t path[2*MAX_PATH];
	wcscpy(path, config.ini);
	if (!GetConfigPath(path, name) || !GetPrivateProfileStruct(L"Contexts", contextName, settings, sizeof(*settings), path) || settings->version != CONTEXT_SETTINGS_VERSION)
	{
		memset(settings, 0, sizeof(settings));
		if (!wcsicmp(contextName, MERGED_CONTEXT))
			settings->autoTrans = 1;
		if (!wcsicmp(contextName, L"Clipboard"))
		{
			settings->lineBreakMode = LINE_BREAK_KEEP;
			settings->autoTrans = 1;
			settings->charRepeatMode = CHAR_REPEAT_NONE;
			settings->phaseRepeatMode = PHRASE_REPEAT_NONE;
			settings->japOnly = 1;
		}
		return 0;
	}
	if (!wcsicmp(contextName, L"Clipboard"))
		settings->autoClipboard = 0;
	return 1;
}

int SaveInjectionContextSettings(wchar_t *name, wchar_t *contextName, ContextSettings *settings)
{
	wchar_t path[2*MAX_PATH];
	wcscpy(path, config.ini);
	settings->version = CONTEXT_SETTINGS_VERSION;
	if (!GetConfigPath(path, name) || !WritePrivateProfileStruct(L"Contexts", contextName, settings, sizeof(*settings), path))
		return 0;
	return 1;
}

int SaveInjectionSettings(InjectionSettings &cfg, wchar_t *name, const wchar_t* internalHooks)
{
	int i;
	wchar_t path[2*MAX_PATH];
	wcscpy(path, config.ini);
	if (name && wcsicmp(name, L"Default") && wcsicmp(name, L"::System"))
	{
		CreateDirectory(L"Game Configs", 0);
		GetConfigPath(path, name);
	}
	WritePrivateProfileInt(L"Launch", L"Language", cfg.forceLocale, path);

	WritePrivateProfileInt(L"Atlas", L"Break On", cfg.atlasConfig.flags, path);
	WritePrivateProfileString(L"Atlas", L"Environment", cfg.atlasConfig.environment, path);
	WritePrivateProfileString(L"Atlas", L"Rule Set", cfg.atlasConfig.trsPath, path);

	for (i=0; cfg.exePath[i]; i++)
		if (cfg.exePath[i] > 0x7F) break;
	WritePrivateProfileString(L"Injection", L"Exe Path", cfg.exePath, path);
	if (cfg.exePath[i])
		WritePrivateProfileStruct(L"Injection", L"Exe Path Struct", cfg.exePath, sizeof(cfg.exePath), path);
	else
		WritePrivateProfileStruct(L"Injection", L"Exe Path Struct", cfg.exePath, 0, path);

	WritePrivateProfileString(L"Injection", L"AGTH params", cfg.agthParams, path);
	WritePrivateProfileInt(L"Injection", L"Mode", cfg.injectionFlags, path);
	WritePrivateProfileInt(L"Injection", L"AGTH Flags", cfg.agthFlags, path);

	WritePrivateProfileInt(L"Injection", L"AGTH Repeat", cfg.agthSymbolRepeatCount, path);
	WritePrivateProfileInt(L"Injection", L"AGTH KF 1", cfg.agthPhraseCount1, path);
	WritePrivateProfileInt(L"Injection", L"AGTH KF 2", cfg.agthPhraseCount2, path);

	WritePrivateProfileInt(L"Injection", L"Hook Delay", cfg.internalHookDelay, path);

	WritePrivateProfileInt(L"Injection", L"Hook Time", cfg.internalHookTime, path);
	WritePrivateProfileInt(L"Injection", L"Log Len", cfg.maxLogLen, path);

	WritePrivateProfileString(L"Injection", L"Internal Hooks", internalHooks, path);

	return 1;
}

void CleanupConfig()
{
	SaveConfig();
	if (config.hIdFont) DeleteObject(config.hIdFont);
	config.replace.Clear();
	config.hIdFont = 0;
	UnloadPlugins();
}

void InitLogFont(LOGFONT *lf, MyFontInfo *font)
{
	memset(lf, 0, sizeof(LOGFONT));
	wcscpy(lf->lfFaceName, font->face);
	lf->lfHeight = -font->height/15;
	lf->lfWeight = 400 + 300 * (font->bold != 0);
	lf->lfItalic = font->italic;
	lf->lfUnderline = font->underline;
	lf->lfStrikeOut = font->strikeout;
}

int MyChooseFont(HWND hWnd, MyFontInfo *font)
{
	CHOOSEFONT cf;
	LOGFONT lf;
	memset(&cf, 0, sizeof(cf));
	cf.lStructSize = sizeof(cf);
	InitLogFont(&lf, font);
	cf.rgbColors = config.font.color;
	cf.hwndOwner = hWnd;
	cf.lpLogFont = &lf;
	cf.Flags = CF_NOSCRIPTSEL | CF_SCREENFONTS | CF_EFFECTS | CF_INITTOLOGFONTSTRUCT;
	if (ChooseFont(&cf))
	{
		wcscpy(font->face, lf.lfFaceName);
		font->height = -15*lf.lfHeight;
		font->bold = (lf.lfWeight>=600);
		font->italic = lf.lfItalic;
		font->underline = lf.lfUnderline;
		font->strikeout = lf.lfStrikeOut;
		font->color = cf.rgbColors;
		return 1;
	}
	return 0;
}
