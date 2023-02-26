#include <Shared/Shrink.h>
#include "Context.h"
#include "resource.h"
#include "Filter.h"

// For changing port
#include "BufferedSocket.h"

ContextList Contexts;


static HWND hWndContextDlg = 0;
static ContextProcessInfo *selectedProcess = 0;
static Context *selectedContext = 0;

void UpdateDialogProcesses();
void UpdateDialogProcess(ContextProcessInfo *process, HWND hWndProcesses=0);
void UpdateDialogContext(Context *ctxt);
void UpdateDialogContexts();
void DeleteDialogProcess(ContextProcessInfo *process);
void DialogSelectProcess(ContextProcessInfo *process);

void RefreshDialogText();

void ContextRecord::Cleanup()
{
	string->Release();
}

void ContextProcessInfo::Cleanup()
{
	free(name);
	free(this);
}

void ContextProcessInfo::AddSocket()
{
	numSockets++;
	UpdateDialogProcess(this);
}

void ContextProcessInfo::AddContext()
{
	numContexts++;
}

void ContextProcessInfo::ReleaseSocket()
{
	numSockets--;
	if (!numSockets && !numContexts)
		Contexts.RemoveProcess(this);
	else
		UpdateDialogProcess(this);
}

void ContextProcessInfo::ReleaseContext()
{
	numContexts--;
	if (!numSockets && !numContexts)
		Contexts.RemoveProcess(this);
}

void Context::Cleanup()
{
	Clear(1);

	process->ReleaseContext();
	free(name);
	free(this);
}

void Context::Clear(int noRedraw)
{
	for (int i=0; i<numRecords; i++)
		records[i].Cleanup();
	free(loop);
	loop = 0;
	loopLen = 0;
	nextLoopTime = 0;
	lastCheckLength = 0;

	numRecords = 0;
	free(records);
	records = 0;
	free(current);
	current = 0;
	currentLen = 0;
	characters = 0;
	filteredCharacters = 0;

	if (!noRedraw)
	{
		UpdateDialogContext(this);
		if (this == selectedContext)
			Contexts.needTextRefresh = 1;
	}
}

void ContextList::ClearProcess(ContextProcessInfo *process)
{
	int out = 0;
	for (int i=0; i<numContexts; i++)
		if (contexts[i]->process == process)
			contexts[i]->Cleanup();
		else
			contexts[out++] = contexts[i];
	numContexts = out;
	selectedContext = 0;
	UpdateDialogContexts();
}

ContextList::ContextList()
{
	memset(this, 0, sizeof(*this));
}

ContextList::~ContextList()
{
	for (int i=0; i<numContexts; i++)
		contexts[i]->Cleanup();
	free(contexts);
	for (int i=0; i<numProcesses; i++)
		processes[i]->Cleanup();
	free(processes);
}

void ContextList::RemoveProcess(ContextProcessInfo *process)
{
	for (int i=0; i<numProcesses; i++)
		if (processes[i] == process)
		{
			for (int j=i+1; j<numProcesses; j++)
				processes[j-1] = processes[j];
			numProcesses--;
			break;
		}
	DeleteDialogProcess(process);
	process->Cleanup();
}

ContextProcessInfo *ContextList::FindProcess(wchar_t *name, unsigned int pid, InjectionSettings *settings)
{
	ContextProcessInfo *process = 0;
	int needUpdate = 0;
	for (int i=0; i<numProcesses; i++)
	{
		if (processes[i]->pid == pid && !wcsicmp(name, processes[i]->name))
		{
			process =  processes[i];
			break;
		}
	}
	if (!process)
	{
		if (!settings)
			return 0;
		process = (ContextProcessInfo*) calloc(1, sizeof(ContextProcessInfo));
		process->name = wcsdup(name);
		process->pid = pid;
		process->connectTime = time(0);
		processes = (ContextProcessInfo**) realloc(processes, sizeof(ContextProcessInfo*) * (numProcesses+1));
		int i = numProcesses;
		while (i)
		{
			int cmp = wcsicmp(processes[i-1]->name, process->name);
			if (cmp < 0 || (!cmp && processes[i-1]->connectTime > process->connectTime))
				break;
			processes[i] = processes[i-1];
			i--;
		}
		processes[i] = process;
		numProcesses++;
		needUpdate = 1;
	}

	if (settings)
	{
		process->internalHookTime = settings->internalHookTime;
		process->maxLogLen = settings->maxLogLen;
	}

	if (needUpdate)
	{
		UpdateDialogProcess(process);
		DialogSelectProcess(process);
	}
	return process;
}

void ContextList::AddText(wchar_t *exeNamePlusFolder, int pid, wchar_t *contextName, wchar_t *string)
{
	ContextProcessInfo *proc = FindProcess(exeNamePlusFolder, pid, 0);
	if (!proc)
	{
		InjectionSettings settings;
		if (!LoadInjectionSettings(settings, exeNamePlusFolder, 0, 0))
		{
			// ????
			return;
		}
		proc = FindProcess(exeNamePlusFolder, pid, &settings);
		if (!proc)
			return;
	}
	AddText(proc, contextName, string);
}

int __cdecl CompareContexts(const void *a, const void *b)
{
	Context *c1 = *(Context**)a;
	Context *c2 = *(Context**)b;
	if (c1->process != c2->process)
	{
		int strDiff = wcscmp(c1->process->name, c2->process->name);
		if (strDiff) return strDiff;
		return (int)c1->process->pid - (int)c2->process->pid;
	}
	return wcscmp(c1->name, c2->name);
}

void ContextList::AddText(ContextProcessInfo *process, wchar_t *contextName, wchar_t *string)
{
	Context temp;
	memset(&temp, 0, sizeof(temp));
	temp.process = process;
	temp.name = contextName;
	Context *temp2 = &temp;
	Context **context2 = (Context**) bsearch(&temp2, contexts, numContexts, sizeof(temp2), CompareContexts);
	Context *context = 0;
	if (context2)
		context = *context2;
	else
	{
		if (numContexts % 10 == 0)
			contexts = (Context**) realloc(contexts, (numContexts+10) * sizeof(Context*));
		int i = numContexts;
		while (i > 0)
		{
			if (CompareContexts(&contexts[i-1], &temp2) <= 0)
				break;
			contexts[i] = contexts[i-1];
			i--;
		}
		context = contexts[i] = (Context*) malloc(sizeof(Context));
		*context = temp;
		context->name = wcsdup(contextName);

		LoadInjectionContextSettings(process->name, contextName, &context->settings);
		if (!wcsicmp(contextName, MERGED_CONTEXT))
			context->settings.flushMerge = context->settings.instaMerge = 0;
		if (!context->settings.phraseMin || context->settings.phraseMax < context->settings.phraseMin)
		{
			context->settings.phraseMin = DEFAULT_PHRASE_MIN;
			context->settings.phraseMax = DEFAULT_PHRASE_MAX;
		}
		if (context->settings.charRepeatMode >= CHAR_REPEAT_CUSTOM)
			context->settings.charRepeatMode = CHAR_REPEAT_AUTO_CONSTANT;
		if (context->settings.charRepeatMode >= PHRASE_REPEAT_CUSTOM)
			context->settings.charRepeatMode = PHRASE_REPEAT_AUTO;
		numContexts++;
		process->AddContext();
		// Make sure disabled contexts get added.
		UpdateDialogContext(context);
	}
	if (context->settings.disable)
		return;

	if (context->settings.instaMerge)
		AddText(context->process, MERGED_CONTEXT, string);
	else
	{
		unsigned int time = timeGetTime();
		if (context->currentLen)
			CheckContext(context, time);
		if (context->settings.textLoops)
		{
			if (context->loop && context->lastTime + process->internalHookTime < time)
			{
				free(context->loop);
				context->loop = 0;
			}
			if (!context->nextLoopTime)
			{
				context->nextLoopTime = time + process->internalHookTime;
				context->lastCheckLength = context->currentLen;
			}
		}

		int len = wcslen(string);
		context->current = (wchar_t*) realloc(context->current, sizeof(wchar_t) * (len + 1 + context->currentLen));
		wcscpy(context->current + context->currentLen, string);
		context->currentLen += len;
		context->lastTime = time;
		context->characters += len;

		if (context->process->internalHookTime <= 0)
			CheckContext(context, time);

		UpdateDialogContext(context);
		if (context == selectedContext && !context->loop)
			Contexts.needTextRefresh = 1;
	}
}

int ContextList::CheckDone()
{
	unsigned int t = timeGetTime();

	int done = 1;
	for (int i=0; i<numContexts; i++)
		done &= CheckContext(contexts[i], t);
	if (needTextRefresh)
		RefreshDialogText();
	return done;
}

void Context::FlushCurrent()
{
	nextLoopTime = 0;

	currentLen = AutoFilter(current, currentLen, &settings);

	filteredCharacters += currentLen;

	if (settings.flushMerge)
		Contexts.AddText(process, L"::Merged", current);
	else
	{
		SharedString *string = CreateSharedString(current, currentLen);
		if (loopLen)
		{
			//ResizeSharedString(string, currentLen + 2 + loopLen);
			//wsprintf(string->string+currentLen, L"::%s", loop);
		}
		if (numRecords % 10 == 0)
			records = (ContextRecord*) realloc(records, sizeof(ContextRecord) * (numRecords+10));
		records[numRecords++].string = string;
		logLen += string->length;
		if (settings.autoTrans)
		{
			if (!settings.japOnly || HasJap(string->string))
			{
				string->AddRef();
				SendMessage(hWndSuperMaster, WMA_AUTO_TRANSLATE_CONTEXT, (WPARAM)string, 0);
			}
		}

		if (settings.autoClipboard)
		{
			if (OpenClipboard(0))
			{
				EmptyClipboard();
				HGLOBAL hGlobal = GlobalAlloc(GMEM_MOVEABLE, sizeof(wchar_t) * (string->length+2));
				if (hGlobal)
				{
					wchar_t *clipboard = (wchar_t *)GlobalLock(hGlobal);
					if (clipboard)
					{
						wcscpy(clipboard, string->string);
						GlobalUnlock(hGlobal);
						SetClipboardData(CF_UNICODETEXT, hGlobal);
					}
					else
						GlobalFree(hGlobal);
				}
				CloseClipboard();
			}
		}

		if (logLen / 1024 > process->maxLogLen)
		{
			int next = 0;
			while (logLen / 1024 > process->maxLogLen/2)
			{
				logLen -= records[next].string->length;
				records[next].Cleanup();
				next++;
			}
			numRecords -= next;
			memmove(records, records+next, sizeof(ContextRecord) * numRecords);
		}
	}
	free(current);
	current = 0;
	currentLen = 0;
	UpdateDialogContext(this);
	if (this == selectedContext)
		Contexts.needTextRefresh = 1;
}

int ContextList::CheckContext(Context *context, unsigned int time)
{
	if (!context->currentLen)
		return 1;
	if (context->settings.textLoops)
	{
		if (context->loop)
		{
			int clearLoop = 0;
			if (context->loopLen <= context->currentLen)
			{
				int loopCount = 0;
				while (!wcsncmp(context->loop, context->current + loopCount, context->loopLen))
					loopCount += context->loopLen;
				if (loopCount)
				{
					wcscpy(context->current, context->current+loopCount);
					context->currentLen -= loopCount;
					// reset loop timer.
					context->nextLoopTime = 0;
					return 0;
				}
				clearLoop = 1;
			}
			else if (wcsncmp(context->loop, context->current, context->currentLen))
				clearLoop = 1;
			if (clearLoop)
			{
				free(context->loop);
				context->loop = 0;
				context->loopLen = 0;
			}
		}
	}
	if (time - context->lastTime < context->process->internalHookTime)
	{
		if (context->settings.textLoops && context->nextLoopTime && time >= context->nextLoopTime)
		{
			// Will set again on next AddText.
			context->nextLoopTime = 0;
			//if (context->currentLen > 100) {
				int repLen = 0;
				int checkStart = context->lastCheckLength;
				int i = 1;
				for (; i <= (context->currentLen-checkStart)/2; i++)
				{
					if (!wcsncmp(context->current + checkStart, context->current+checkStart + i, context->currentLen - checkStart-i))
					{
						repLen = i;
						break;
					}
				}
				if (!repLen)
				{
					while (i <= context->currentLen-checkStart && i < context->currentLen*2)
					{
						if (!wcsncmp(context->current + context->currentLen-2*i, context->current + context->currentLen-i, i))
						{
							repLen = i;
							break;
						}
						i++;
					}
				}
				if (repLen)
				{
					int start = checkStart;
					while (start > 0 && context->current[start-1] == context->current[start-1+repLen]) start--;
					//if (context->currentLen - start >= 100) {
						if (context->loop)
						{
							free(context->loop);
							context->loop = 0;
						}
						wchar_t *temp = wcsdup(context->current);
						int bestLen = wcslen(context->current);
						int bestEnd = 0;
						for (int end = 0; end<repLen; end++)
						{
							wcscpy(temp, context->current);
							int len = context->currentLen - end;
							temp[len] = 0;
							len = AutoConstantRepeatFilter(temp, len);
							len = PhraseRepeatFilter(temp, len, 3, 150);
							if (len < bestLen)
							{
								bestLen = len;
								bestEnd = end;
							}
						}
						free(temp);
						int len = context->currentLen - bestEnd;
						len = len - ((len-start)/repLen-1) * repLen;
						int extraChars = bestEnd;
						context->currentLen = len;
						context->current[context->currentLen] = 0;
						context->loop = wcsdup(context->current + context->currentLen - repLen);
						context->loopLen = repLen;

						context->FlushCurrent();
						if (extraChars)
						{
							context->currentLen = extraChars;
							context->current = (wchar_t*) malloc(sizeof(wchar_t)*(extraChars+1));
							memcpy(context->current, context->loop, sizeof(wchar_t)*extraChars);
							context->current[extraChars] = 0;
						}
						UpdateDialogContext(context);
						if (context == selectedContext)
							RefreshDialogText();

						return 0;
					}
				//}
			//}
		}
		return 0;
	}

	context->FlushCurrent();
	return 1;
}

void UpdateDialogProcess(ContextProcessInfo *process, HWND hWndProcesses)
{
	if (!hWndContextDlg)
		return;
	if (!hWndProcesses)
		hWndProcesses = GetDlgItem(hWndContextDlg, IDC_PROCESS_LIST);
	LVFINDINFO info;
	info.flags = LVFI_STRING | LVFI_PARAM;
	info.psz = process->name;
	info.lParam = process->pid;
	int index = ListView_FindItem(hWndProcesses, -1, &info);
	if (index < 0)
	{
		LVITEM item;
		item.mask = LVIF_PARAM | LVIF_TEXT;
		item.pszText = process->name;
		item.lParam = process->pid;
		item.iItem = 0;
		item.iSubItem = 0;
		for (int i=0; i<Contexts.numProcesses; i++)
			if (Contexts.processes[i] == process)
				item.iItem = i;
		index = ListView_InsertItem(hWndProcesses, &item);
	}
	if (index >= 0)
	{
		LVITEM item;
		wchar_t temp[100];
		if (process->numSockets)
			wsprintf(temp, L"%i", process->pid);
		else
			temp[0] = 0;
		item.iItem = index;
		item.iSubItem = 1;
		item.pszText = temp;
		item.mask = LVIF_TEXT;
		ListView_SetItem(hWndProcesses, &item);
	}
}

void UpdateDialogProcesses()
{
	if (!hWndContextDlg)
		return;
	HWND hWndProcesses = GetDlgItem(hWndContextDlg, IDC_PROCESS_LIST);
	for (int i=0; i<Contexts.numProcesses; i++)
		UpdateDialogProcess(Contexts.processes[i], hWndProcesses);
}

int GetProcessIndex(ContextProcessInfo *process)
{
	if (!hWndContextDlg)
		return -1;
	HWND hWndProcesses = GetDlgItem(hWndContextDlg, IDC_PROCESS_LIST);
	LVFINDINFO info;
	info.flags = LVFI_STRING | LVFI_PARAM;
	info.psz = process->name;
	info.lParam = process->pid;
	return ListView_FindItem(hWndProcesses, -1, &info);
}

void DeleteDialogProcess(ContextProcessInfo *process)
{
	int index = GetProcessIndex(process);

	// Will be updated later anyways, possibly before event deleting
	// the object, but best to be safe.
	if (process == selectedProcess)
		selectedProcess = 0;

	if (index >= 0)
	{
		HWND hWndProcesses = GetDlgItem(hWndContextDlg, IDC_PROCESS_LIST);
		ListView_DeleteItem(hWndProcesses, index);
	}
}

void UpdateSelectedContext();

void UpdateSelectedProcess()
{
	if (!hWndContextDlg)
	{
		selectedProcess = 0;
		return;
	}
	HWND hWndProcesses = GetDlgItem(hWndContextDlg, IDC_PROCESS_LIST);
	HWND hWndIdle = GetDlgItem(hWndContextDlg, IDC_INTERNAL_IDLE_DELAY);
	HWND hWndMaxHistory = GetDlgItem(hWndContextDlg, IDC_MAX_HISTORY);

	int index = ListView_GetNextItem(hWndProcesses, -1, LVNI_SELECTED);
	if (index >= 0)
	{
		LV_ITEM item;
		wchar_t temp[2*MAX_PATH];
		item.iItem = index;
		item.iSubItem = 0;
		item.mask = LVIF_TEXT | LVIF_PARAM;
		item.pszText = temp;
		item.cchTextMax = sizeof(temp)/sizeof(temp[0]);

		if (ListView_GetItem(hWndProcesses, &item))
		{
			for (int i=0; i<Contexts.numProcesses; i++)
			{
				if (Contexts.processes[i]->pid == item.lParam &&
					!wcsicmp(Contexts.processes[i]->name, temp))
					{
						selectedProcess = Contexts.processes[i];
						EnableWindow(hWndIdle, 1);
						EnableWindow(hWndMaxHistory, 1);
						SetDlgItemInt(hWndContextDlg, IDC_INTERNAL_IDLE_DELAY, selectedProcess->internalHookTime, 0);
						SetDlgItemInt(hWndContextDlg, IDC_MAX_HISTORY, selectedProcess->maxLogLen, 0);
						UpdateDialogContexts();
						UpdateSelectedContext();
						return;
				}
			}
		}
	}
	selectedProcess = 0;
	SetDlgItemText(hWndContextDlg, IDC_INTERNAL_IDLE_DELAY, L"");
	SetDlgItemText(hWndContextDlg, IDC_MAX_HISTORY, L"");
	EnableWindow(hWndIdle, 0);
	EnableWindow(hWndMaxHistory, 0);
	UpdateSelectedContext();
}

static int GetContextFromListIndex(int index)
{
	if (!hWndContextDlg || !selectedProcess)
		return -1;
	if (index >= 0)
	{
		HWND hWndContexts = GetDlgItem(hWndContextDlg, IDC_CONTEXT_LIST);
		LV_ITEM item;
		wchar_t temp[2*MAX_PATH];
		item.iItem = index;
		item.iSubItem = 0;
		item.mask = LVIF_TEXT;
		item.pszText = temp;
		item.cchTextMax = sizeof(temp)/sizeof(temp[0]);

		if (ListView_GetItem(hWndContexts, &item))
			for (int i=0; i<Contexts.numContexts; i++)
				if (Contexts.contexts[i]->process == selectedProcess && !wcsicmp(Contexts.contexts[i]->name, temp))
					return i;
	}
	return -1;
}

static void UpdateSelectedContextInternal()
{
	if (!hWndContextDlg || !selectedProcess)
	{
		selectedContext = 0;
		return;
	}
	HWND hWndContexts = GetDlgItem(hWndContextDlg, IDC_CONTEXT_LIST);

	int index = ListView_GetNextItem(hWndContexts, -1, LVNI_SELECTED);
	index = GetContextFromListIndex(index);
	if (index < 0)
		selectedContext = 0;
	else
		selectedContext = Contexts.contexts[index];
}

void UpdateSelectedContext()
{
	UpdateSelectedContextInternal();
	if (!hWndContextDlg)
		return;
	HWND hWndContextAutoTrans = GetDlgItem(hWndContextDlg, IDC_CONTEXT_AUTO_TRANSLATE);
	HWND hWndContextDisable = GetDlgItem(hWndContextDlg, IDC_CONTEXT_DISABLE);
	EnableWindow(hWndContextAutoTrans, selectedContext != 0);
	EnableWindow(hWndContextDisable, selectedContext != 0);

	Context *tc = selectedContext;
	// Prevent saving junk when set values.
	selectedContext = 0;
	for (int i=IDC_CONTEXT_INSTA_MERGE; i<=IDC_PHRASE_REPEAT_CUSTOM_MAX; i++)
	{
		HWND hWnd = GetDlgItem(hWndContextDlg, i);
		if (!tc)
			EnableWindow(hWnd, 0);
		else if (i == IDC_CONTEXT_DISABLE)
			EnableWindow(hWnd, 1);
		else if (tc->settings.disable)
			EnableWindow(hWnd, 0);
		else if (i == IDC_CONTEXT_INSTA_MERGE || i == IDC_CONTEXT_FLUSH_MERGE)
		{
			if (!wcsicmp(tc->name, MERGED_CONTEXT))
				EnableWindow(hWnd, 0);
			else
				EnableWindow(hWnd, 1);
		}
		else if (tc->settings.instaMerge)
			EnableWindow(hWnd, 0);
		else if (tc->settings.flushMerge && i == IDC_CONTEXT_AUTO_TRANSLATE)
			EnableWindow(hWnd, 0);
		else if (!tc->settings.autoTrans && i == IDC_JAPANESE_ONLY)
			EnableWindow(hWnd, 0);
		else if (i == IDC_AUTO_CLIPBOARD && (!wcsicmp(tc->name, L"Clipboard") || i == IDC_CONTEXT_INSTA_MERGE))
			EnableWindow(hWnd, 0);
		else
			EnableWindow(hWnd, 1);
		if (!tc)
		{
			if (i < IDC_CHAR_REPEAT_CUSTOM_TEXT)
				CheckDlgButton(hWndContextDlg, i, BST_UNCHECKED);
			else
				SetWindowText(hWnd, L"");
		}
	}
	if (!tc)
	{
		selectedContext = tc;
		return;
	}

	for (int i=0; i<=LINE_BREAK_REMOVE_SOME; i++)
		SetDlgButton(hWndContextDlg, IDC_LINE_BREAK_REMOVE+i, tc->settings.lineBreakMode == i);
	for (int i=0; i<=CHAR_REPEAT_CUSTOM; i++)
		SetDlgButton(hWndContextDlg, IDC_CHAR_REPEAT_AUTO_CONSTANT+i, tc->settings.charRepeatMode == i);
	for (int i=0; i<=PHRASE_REPEAT_CUSTOM; i++)
		SetDlgButton(hWndContextDlg, IDC_PHRASE_REPEAT_AUTO+i, tc->settings.phaseRepeatMode == i);
	for (int i=0; i<=PHRASE_EXTENSION_AGGRESSIVE; i++)
		SetDlgButton(hWndContextDlg, IDC_PHRASE_EXTENSION_NONE+i, tc->settings.phaseExtensionMode == i);

	SetDlgButton(hWndContextDlg, IDC_CONTEXT_AUTO_TRANSLATE, tc->settings.autoTrans);
	SetDlgButton(hWndContextDlg, IDC_CONTEXT_DISABLE, tc->settings.disable);
	SetDlgButton(hWndContextDlg, IDC_CONTEXT_INSTA_MERGE, tc->settings.instaMerge);
	SetDlgButton(hWndContextDlg, IDC_CONTEXT_FLUSH_MERGE, tc->settings.flushMerge);
	SetDlgButton(hWndContextDlg, IDC_TEXT_LOOPS, tc->settings.textLoops);
	SetDlgButton(hWndContextDlg, IDC_JAPANESE_ONLY, tc->settings.japOnly);
	SetDlgButton(hWndContextDlg, IDC_AUTO_CLIPBOARD, tc->settings.autoClipboard);

	wchar_t temp[100];
	temp[0] = 0;
	for (int i=0; i<sizeof(tc->settings.customRepeats)/sizeof(tc->settings.customRepeats[0]); i++)
	{
		int c = tc->settings.customRepeats[i];
		if (c)
		{
			if (temp[0])
				wsprintf(wcschr(temp, 0), L", %i", c);
			else
				wsprintf(temp, L"%i", c);
		}
	}
	SetDlgItemText(hWndContextDlg, IDC_CHAR_REPEAT_CUSTOM_TEXT, temp);
	SetDlgItemInt(hWndContextDlg, IDC_PHRASE_REPEAT_CUSTOM_MIN, tc->settings.phraseMin, 0);
	SetDlgItemInt(hWndContextDlg, IDC_PHRASE_REPEAT_CUSTOM_MAX, tc->settings.phraseMax, 0);
	SetDlgItemInt(hWndContextDlg, IDC_LINE_BREAK_FIRST, tc->settings.lineBreaksFirst, 0);
	SetDlgItemInt(hWndContextDlg, IDC_LINE_BREAK_LAST, tc->settings.lineBreaksLast, 0);
	selectedContext = tc;
}

void RefreshDialogText()
{
	Contexts.needTextRefresh = 0;
	if (!hWndContextDlg)
		return;

	HWND hWndContextText = GetDlgItem(hWndContextDlg, IDC_CONTEXT_TEXT);
	if (!selectedContext)
		SetWindowText(hWndContextText, L"");
	else
	{
		int charCount = selectedContext->currentLen;
		int lineCount = 0;
		while (lineCount < selectedContext->numRecords && (lineCount < 5 || charCount < 2000 || (config.flags & CONFIG_CONTEXT_FULL_HISTORY)))
		{
			SharedString *string = selectedContext->records[selectedContext->numRecords - lineCount-1].string;
			charCount += string->length+4;
			for (int i=0; i<string->length; i++)
				charCount += (string->string[i] == '\n');
			lineCount++;
		}
		wchar_t *string = (wchar_t*) malloc(sizeof(wchar_t) * (charCount+1));
		wchar_t *pos = string;

		for (int i=selectedContext->numRecords-lineCount; i<selectedContext->numRecords; i++)
		{
			SharedString *string = selectedContext->records[i].string;
			for (int i=0; i<string->length; i++)
			{
				if (string->string[i] == '\n')
					pos++[0] = '\r';
				pos++[0] = string->string[i];
			}

			wcscpy(pos, L"\r\n\r\n");
			pos += 4;
		}
		if (selectedContext->currentLen)
		{
			wcscpy(pos, selectedContext->current);
			pos += selectedContext->currentLen;
		}
		else if (lineCount)
		{
			pos[-4] = 0;
			pos -= 4;
		}
		else
			string[0] = 0;

		SetWindowText(hWndContextText, string);
		SendMessage(hWndContextText, EM_SETSEL, pos-string, pos-string);
		SendMessage(hWndContextText, EM_SCROLLCARET, 0, 0);
		free(string);
	}
}


void UpdateItem(HWND hWndContexts, int index, Context *context)
{
	if (index >= 0)
	{
		LVITEM item;
		item.mask = LVIF_TEXT;
		item.iSubItem;
		wchar_t temp[100];

		wchar_t *text = L"";
		if (context->settings.disable)
			text = L"Disabled";
		else if (context->settings.flushMerge || context->settings.instaMerge)
			text = L"Merge";
		else if (context->settings.autoTrans)
			text = L"Auto";

		item.iItem = index;
		item.iSubItem = 1;
		item.pszText = text;
		item.mask = LVIF_TEXT;
		ListView_SetItem(hWndContexts, &item);

		item.pszText = temp;
		wsprintf(temp, L"%i", context->numRecords);
		item.iSubItem = 2;
		ListView_SetItem(hWndContexts, &item);

		wsprintf(temp, L"%i", context->characters);
		item.iSubItem = 3;
		ListView_SetItem(hWndContexts, &item);

		wsprintf(temp, L"%i", context->filteredCharacters);
		item.iSubItem = 4;
		ListView_SetItem(hWndContexts, &item);
	}
}

void UpdateDialogContexts()
{
	if (!hWndContextDlg)
		return;
	HWND hWndContexts = GetDlgItem(hWndContextDlg, IDC_CONTEXT_LIST);
	ListView_DeleteAllItems(hWndContexts);
	if (!selectedProcess)
		return;

	for (int i=0; i<Contexts.numContexts; i++)
	{
		Context *context = Contexts.contexts[i];
		if (context->process != selectedProcess) continue;
		LVFINDINFO info;
		info.flags = LVFI_STRING;
		info.psz = context->name;
		int index = ListView_FindItem(hWndContexts, -1, &info);
		if (index < 0)
		{
			LVITEM item;
			item.mask = LVIF_TEXT;
			item.pszText = context->name;
			item.iItem = 0;
			item.iSubItem = 0;
			index = ListView_InsertItem(hWndContexts, &item);
			UpdateItem(hWndContexts, index, context);
		}
	}
}

void UpdateDialogContext(Context *ctxt)
{
	if (!selectedProcess)
		return;
	if (ctxt->process == selectedProcess)
	{
		HWND hWndContexts = GetDlgItem(hWndContextDlg, IDC_CONTEXT_LIST);
		LVFINDINFO info;
		info.flags = LVFI_STRING;
		info.psz = ctxt->name;
		int index = ListView_FindItem(hWndContexts, -1, &info);
		if (index >= 0)
			UpdateItem(hWndContexts, index, ctxt);
		else
			UpdateDialogContexts();
	}
}

void ClearSelectedProcess()
{
	if (selectedProcess)
		Contexts.ClearProcess(selectedProcess);
}

void ClearSelectedContext()
{
	if (selectedContext)
	{
		selectedContext->Clear();
		UpdateDialogContext(selectedContext);
		RefreshDialogText();
	}
}

void DialogSelectProcess(ContextProcessInfo *process)
{
	if (process && hWndContextDlg)
	{
		int index = GetProcessIndex(process);
		if (index >= 0)
		{
			HWND hWndProcesses = GetDlgItem(hWndContextDlg, IDC_PROCESS_LIST);
			ListView_SetItemState(hWndProcesses, index, LVIS_SELECTED, LVIS_SELECTED);
		}
	}
	UpdateSelectedProcess();
}

void UpdatePort()
{
	SetDlgItemInt(hWndContextDlg, IDC_HOOK_PORT, config.port, 0);
}

INT_PTR CALLBACK ContextDialogProc(HWND hWndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	HWND hWndProcesses = GetDlgItem(hWndDlg, IDC_PROCESS_LIST);
	HWND hWndContexts = GetDlgItem(hWndDlg, IDC_CONTEXT_LIST);
	HWND hWndContextText = GetDlgItem(hWndDlg, IDC_CONTEXT_TEXT);
	switch (uMsg)
	{
		case WM_INITDIALOG:
			{
				SendMessage(hWndProcesses, LVM_SETEXTENDEDLISTVIEWSTYLE, LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT);
				SendMessage(hWndContexts, LVM_SETEXTENDEDLISTVIEWSTYLE, LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT);
				if (config.flags & CONFIG_CONTEXT_FULL_HISTORY)
					CheckDlgButton(hWndDlg, IDC_CONTEXT_FULL_HISTORY, BST_CHECKED);

				hWndContextDlg = hWndDlg;
				LVCOLUMN c;
				c.mask = LVCF_TEXT | LVCF_WIDTH;
				c.cx = 290;
				c.pszText = L"Process";
				ListView_InsertColumn(hWndProcesses, 0, &c);
				c.cx =  60;
				c.pszText = L"Pid";
				ListView_InsertColumn(hWndProcesses, 1, &c);


				c.cx = 155;
				c.pszText = L"Name";
				ListView_InsertColumn(hWndContexts, 0, &c);

				c.cx = 55;
				c.pszText = L"Mode";
				ListView_InsertColumn(hWndContexts, 1, &c);

				c.cx = 40;
				c.pszText = L"Lines";
				ListView_InsertColumn(hWndContexts, 2, &c);

				c.cx = 55;
				c.pszText = L"Chars";
				ListView_InsertColumn(hWndContexts, 3, &c);

				c.cx = 45;
				c.pszText = L"Filter";
				ListView_InsertColumn(hWndContexts, 4, &c);

				UpdateDialogProcesses();
				ContextProcessInfo *select = 0;
				if (Contexts.numProcesses) select = Contexts.processes[0];
				for (int i=0; i<Contexts.numProcesses; i++)
				{
					if (!Contexts.processes[i]->numSockets)
						continue;
					if (Contexts.processes[i]->numSockets && select->numSockets &&
						Contexts.processes[i]->connectTime > select->connectTime)
							continue;
					select = Contexts.processes[i];
					continue;
				}
				DialogSelectProcess(select);
				UpdatePort();
			}
			break;
		case WM_NOTIFY:
			{
				NMHDR *n = (NMHDR *)lParam;
				if (n->idFrom == IDC_PROCESS_LIST)
				{
					if (n->code == LVN_KEYDOWN)
					{
						NMLVKEYDOWN *n = (NMLVKEYDOWN*)lParam;
						if (n->wVKey == VK_DELETE || n->wVKey == VK_BACK)
							ClearSelectedProcess();
						return 0;
					}
					else if (n->code == LVN_ITEMCHANGED)
					{
						NMITEMACTIVATE* n = (NMITEMACTIVATE*)lParam;
						int index = n->iItem;
						if (index >= 0 && ((n->uNewState^n->uOldState) & LVIS_SELECTED))
						{
							UpdateSelectedProcess();
							UpdateDialogContexts();
						}
						return 0;
					}
					else if (n->code == NM_RCLICK)
					{
						NMITEMACTIVATE* n = (NMITEMACTIVATE*)lParam;
						if (selectedProcess)
						{
							HMENU hMenu = LoadMenu(ghInst, MAKEINTRESOURCE(IDR_MENU_CONTEXT_MANAGER));
							HMENU hMenu2;
							if (hMenu && (hMenu2 = GetSubMenu(hMenu, 0)))
							{
								POINT pt;
								GetCursorPos(&pt);
								int res = TrackPopupMenu(hMenu2, TPM_RETURNCMD, pt.x, pt.y, 0, hWndDlg, 0);
								if (res == ID_PROCESS_CLEAR)
									ClearSelectedProcess();
							}
							DestroyMenu(hMenu);
						}
						return 0;
					}
				}
				else if (n->idFrom == IDC_CONTEXT_LIST)
				{
					if (n->code == LVN_KEYDOWN)
					{
						NMLVKEYDOWN *n = (NMLVKEYDOWN*)lParam;
						if (n->wVKey == VK_DELETE || n->wVKey == VK_BACK)
							ClearSelectedContext();
						return 0;
					}
					else if (n->code == LVN_ITEMCHANGED)
					{
						NMITEMACTIVATE* n = (NMITEMACTIVATE*)lParam;
						int index = n->iItem;
						if (index >= 0 && ((n->uNewState^n->uOldState) & LVIS_SELECTED))
						{
							UpdateSelectedContext();
							RefreshDialogText();
						}
						return 0;
					}
					else if (n->code == NM_RCLICK)
					{
						NMITEMACTIVATE* n = (NMITEMACTIVATE*)lParam;
						int index = GetContextFromListIndex(n->iItem);
						if (selectedContext)
						{
							HMENU hMenu = LoadMenu(ghInst, MAKEINTRESOURCE(IDR_MENU_CONTEXT_MANAGER));
							HMENU hMenu2;
							if (hMenu && (hMenu2 = GetSubMenu(hMenu, 1)))
							{
								POINT pt;
								GetCursorPos(&pt);
								int res = TrackPopupMenu(hMenu2, TPM_RETURNCMD, pt.x, pt.y, 0, hWndDlg, 0);
								if (res == ID_CONTEXT_CLEAR)
									ClearSelectedContext();
							}
							DestroyMenu(hMenu);
						}
						return 0;
					}
				}
			}
			return 1;
		case WM_COMMAND:
			if (HIWORD(wParam) == BN_CLICKED)
			{
				int i = LOWORD(wParam);
				if (i == IDOK || i == IDCANCEL)
				{
					hWndContextDlg = 0;
					selectedProcess = 0;
					selectedContext = 0;
					EndDialog(hWndDlg, 0);
					return 0;
				}
				else if (i == IDC_CONTEXT_FULL_HISTORY)
				{
					if (BST_CHECKED == IsDlgButtonChecked(hWndDlg, IDC_CONTEXT_FULL_HISTORY))
						config.flags |= CONFIG_CONTEXT_FULL_HISTORY;
					else
						config.flags &= ~CONFIG_CONTEXT_FULL_HISTORY;
					RefreshDialogText();
					SaveGeneralConfig();
				}
				else if (selectedContext)
				{
					unsigned char checked = (BST_CHECKED == IsDlgButtonChecked(hWndDlg, i));
					if (i == IDC_CONTEXT_AUTO_TRANSLATE)
						selectedContext->settings.autoTrans = checked;
					else if (i == IDC_AUTO_CLIPBOARD)
					{
						selectedContext->settings.autoClipboard = checked;
						UpdateSelectedContext();
					}
					else if (i == IDC_JAPANESE_ONLY)
					{
						selectedContext->settings.japOnly = checked;
						UpdateSelectedContext();
					}
					else if (i == IDC_CONTEXT_DISABLE)
					{
						selectedContext->settings.disable = checked;
						UpdateSelectedContext();
					}
					else if (i == IDC_CONTEXT_INSTA_MERGE)
					{
						selectedContext->settings.instaMerge = checked;
						selectedContext->settings.flushMerge &= !checked;
						UpdateSelectedContext();
					}
					else if (i == IDC_CONTEXT_FLUSH_MERGE)
					{
						selectedContext->settings.flushMerge = checked;
						selectedContext->settings.instaMerge &= !checked;
						UpdateSelectedContext();
					}
					else if (IDC_LINE_BREAK_REMOVE <= i && i <= IDC_LINE_BREAK_KEEP_SOME)
					{
						for (int j=0; j <= LINE_BREAK_REMOVE_SOME; j++)
							if (BST_CHECKED == IsDlgButtonChecked(hWndDlg, IDC_LINE_BREAK_REMOVE+j))
								selectedContext->settings.lineBreakMode = j;
					}
					else if (IDC_CHAR_REPEAT_AUTO_CONSTANT <= i && i <= IDC_CHAR_REPEAT_CUSTOM)
					{
						for (int j=0; j <= CHAR_REPEAT_CUSTOM; j++)
							if (BST_CHECKED == IsDlgButtonChecked(hWndDlg, IDC_CHAR_REPEAT_AUTO_CONSTANT+j))
								selectedContext->settings.charRepeatMode = j;
					}
					else if (IDC_PHRASE_REPEAT_AUTO <= i && i <= IDC_PHRASE_REPEAT_CUSTOM)
					{
						for (int j=0; j<= PHRASE_REPEAT_CUSTOM; j++)
							if (BST_CHECKED == IsDlgButtonChecked(hWndDlg, IDC_PHRASE_REPEAT_AUTO+j))
								selectedContext->settings.phaseRepeatMode = j;
					}
					else if (IDC_PHRASE_EXTENSION_NONE <= i && i <= IDC_PHRASE_EXTENSION_AGGRESSIVE)
					{
						for (int j=0; j <= PHRASE_EXTENSION_AGGRESSIVE; j++)
							if (BST_CHECKED == IsDlgButtonChecked(hWndDlg, IDC_PHRASE_EXTENSION_NONE+j))
								selectedContext->settings.phaseExtensionMode = j;
					}
					else if (i == IDC_TEXT_LOOPS)
						selectedContext->settings.textLoops = checked;
					else
						break;
					SaveInjectionContextSettings(selectedContext->process->name, selectedContext->name, &selectedContext->settings);
					UpdateDialogContext(selectedContext);
				}
			}
			else if (HIWORD(wParam) == EN_KILLFOCUS)
			{
				if (LOWORD(wParam) == IDC_HOOK_PORT)
				{
					unsigned short port = GetDlgItemInt(hWndContextDlg, IDC_HOOK_PORT, 0, 0);
					if (port)
					{
						config.port = port;
						TryStartListen(1);
					}
					else
						UpdatePort();
				}
				else if (LOWORD(wParam) == IDC_INTERNAL_IDLE_DELAY || LOWORD(wParam) == IDC_MAX_HISTORY)
				{
					if (selectedProcess)
					{
						unsigned int hookTime = GetDlgItemInt(hWndContextDlg, IDC_INTERNAL_IDLE_DELAY, 0, 0);
						unsigned int maxLog = GetDlgItemInt(hWndContextDlg, IDC_MAX_HISTORY, 0, 0);
						if (selectedProcess->internalHookTime != hookTime || maxLog != selectedProcess->maxLogLen)
						{
							selectedProcess->internalHookTime = hookTime;
							selectedProcess->maxLogLen = maxLog;
							InjectionSettings settings;
							if (LoadInjectionSettings(settings, selectedProcess->name, 0, 0))
							{
								settings.internalHookTime = selectedProcess->internalHookTime;
								settings.maxLogLen = selectedProcess->maxLogLen;
								SaveInjectionSettings(settings, selectedProcess->name, 0);
							}
						}
					}
				}
				else if (LOWORD(wParam) == IDC_PHRASE_REPEAT_CUSTOM_MIN || LOWORD(wParam) == IDC_PHRASE_REPEAT_CUSTOM_MAX)
				{
					if (selectedContext != 0)
					{
						unsigned int min = GetDlgItemInt(hWndContextDlg, IDC_PHRASE_REPEAT_CUSTOM_MIN, 0, 0);
						unsigned int max = GetDlgItemInt(hWndContextDlg, IDC_PHRASE_REPEAT_CUSTOM_MAX, 0, 0);
						if (min && max && min < max)
						{
							selectedContext->settings.phraseMin = min;
							selectedContext->settings.phraseMax = max;
							SaveInjectionContextSettings(selectedContext->process->name, selectedContext->name, &selectedContext->settings);
						}
						UpdateSelectedContext();
					}
				}
				else if (LOWORD(wParam) == IDC_LINE_BREAK_FIRST || LOWORD(wParam) == IDC_LINE_BREAK_LAST)
				{
					if (selectedContext != 0)
					{
						unsigned int first = GetDlgItemInt(hWndContextDlg, IDC_LINE_BREAK_FIRST, 0, 0);
						unsigned int last = GetDlgItemInt(hWndContextDlg, IDC_LINE_BREAK_LAST, 0, 0);
						selectedContext->settings.lineBreaksFirst = first;
						selectedContext->settings.lineBreaksLast = last;
						SaveInjectionContextSettings(selectedContext->process->name, selectedContext->name, &selectedContext->settings);
						UpdateSelectedContext();
					}
				}
				else if (LOWORD(wParam) == IDC_CHAR_REPEAT_CUSTOM_TEXT)
				{
					if (selectedContext)
					{
						wchar_t temp[200];
						int len = GetDlgItemText(hWndContextDlg, IDC_CHAR_REPEAT_CUSTOM_TEXT, temp, sizeof(temp)/sizeof(wchar_t));
						if (!len) temp[0] = 0;
						wchar_t *pos = temp;
						unsigned int index = 0;
						memset(selectedContext->settings.customRepeats, 0, sizeof(selectedContext->settings.customRepeats));
						while (*pos)
						{
							wchar_t *end;
							unsigned int c = wcstoul(pos, &end, 0);
							if (end != pos)
							{
								pos = end;
								if (c > 255) c = 255;
								if (c > 1)
								{
									unsigned int i;
									for (i=0; i<index; i++)
									{
										if (selectedContext->settings.customRepeats[i] == c)
											break;
									}
									if (i == index && i < sizeof(selectedContext->settings.customRepeats) / sizeof(selectedContext->settings.customRepeats[0]))
									{
										selectedContext->settings.customRepeats[index] = c;
										index ++;
									}
								}
							}
							else
								pos++;
						}
						SaveInjectionContextSettings(selectedContext->process->name, selectedContext->name, &selectedContext->settings);
					}
				}
			}
			break;
		default:
			break;
	}
	return 0;
}

void ContextList::Configure(HWND hWnd)
{
	if (!hWndContextDlg)
	{
		HWND hWndDlg = CreateDialog(ghInst, MAKEINTRESOURCE(IDD_CONTEXT_MANAGER), hWnd, ContextDialogProc);
		if (hWndDlg)
			ShowWindow(hWndDlg, 1);
	}
}
