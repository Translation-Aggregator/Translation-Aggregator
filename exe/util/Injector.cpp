#include <Shared/Shrink.h>

#include <sstream>

#include "Injector.h"
#include "../Config.h"
#include <Shared/DllInjection.h>

std::wstring GetAGTHParams(const InjectionSettings *cfg)
{
	std::wstringstream params;
	if (cfg->agthFlags & FLAG_AGTH_COPYDATA)
		params << L" /C";
	if (cfg->agthFlags & FLAG_AGTH_SUPPRESS_SYMBOLS)
		params << L" /KS" << cfg->agthSymbolRepeatCount;
	if (cfg->agthFlags & FLAG_AGTH_SUPPRESS_PHRASES)
		params << L" /KF" << cfg->agthPhraseCount1 << L":" << cfg->agthPhraseCount2;
	if (cfg->agthFlags & FLAG_AGTH_ADD_PARAMS)
		params << L" " << cfg->agthParams;
	return params.str();
}

int FindProcess(wchar_t *partialPath)
{
	DWORD pids[10000];
	int pathLen = wcslen(partialPath);
	DWORD ret;
	getDebugPriv();
	if (EnumProcesses(pids, sizeof(pids), &ret) && (ret/=4))
	{
		LVITEM item;
		wchar_t name[MAX_PATH*2];
		item.pszText = name;
		for (DWORD i=0; i<ret; i++)
		{
			HANDLE hProcess;
			if (hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, 0, pids[i]))
			{
				if (GetModuleFileNameEx(hProcess, 0, name, sizeof(name)/sizeof(name[0])))
				{
					int len = wcslen(name);
					if (len >= pathLen && !wcsicmp(name+len-pathLen, partialPath) && (len == pathLen || name[len-pathLen-1] == '\\'))
					{
						DWORD id = GetProcessId(hProcess);
						if (id)
						{
							CloseHandle(hProcess);
							return id;
						}
					}
				}
				CloseHandle(hProcess);
			}
		}
	}
	return 0;
}

int Inject(const InjectionSettings *cfg, DWORD pid, HWND hWnd)
{
	WritePrivateProfileString(L"Injection", L"Last Launched", cfg->exeNamePlusFolder, config.ini);
	wchar_t temp[MAX_PATH] = {0};
	wcscpy(temp, cfg->exeNamePlusFolder);
	int someErrors = 0;
	int i;
	for (i=0; temp[i]; i++)
		if (temp[i] > 0x7F) break;
	if (temp[i])
		WritePrivateProfileStruct(L"Injection", L"Last Launched Struct", temp, sizeof(temp), config.ini);
	else
		WritePrivateProfileStruct(L"Injection", L"Last Launched Struct", temp, 0, config.ini);

	HANDLE hProcess = 0;
	if (cfg->injectionFlags & NEW_PROCESS)
		pid = 0;
	else
	{
		if (!pid)
			pid = FindProcess(cfg->exeNamePlusFolder);
		if (!pid && (cfg->injectionFlags & INJECT_PROCESS))
		{
			MessageBoxA(hWnd, "Unable to find process.", "Error", MB_OK | MB_ICONERROR);
			return 0;
		}
	}
	if (!pid)
	{
		hProcess = Run(cfg->exePath, &pid, cfg->forceLocale);
		if (!hProcess)
		{
			MessageBoxW(hWnd, L"Unable to create process.", L"Error", MB_OK | MB_ICONERROR);
			return 0;
		}
	}
	else
	{
		hProcess = OpenProcess(CREATE_THREAD_ACCESS, 0, pid);
		if (!hProcess)
		{
			MessageBoxA(hWnd, "Unable to open process.", "Error", MB_OK | MB_ICONERROR);
			return 0;
		}
	}
	if (hProcess)
	{
		int res = 1;
		if (cfg->injectionFlags & (TRANSLATE_MENUS | DCBS_OVERRIDE | INTERNAL_HOOK))
			res = InjectDll(hProcess);
		if (cfg->injectionFlags & AGTH_HOOK)
		{
			wchar_t temp2[MAX_PATH*3];
			wsprintfW(temp2, L"AGTH.exe /P%i %s", pid, GetAGTHParams(cfg).c_str());
			STARTUPINFO si;
			PROCESS_INFORMATION pi;
			memset(&si, 0, sizeof(si));
			si.cb = sizeof(si);

			if (CreateProcess(L"AGTH.exe", temp2, 0, 0, 0, /*CREATE_SUSPENDED*/0, 0, 0, &si, &pi))
			{
				CloseHandle(pi.hProcess);
				CloseHandle(pi.hThread);
			}
			else
				MessageBoxW(hWnd, L"Unable to launch AGTH.", L"Error", MB_OK | MB_ICONERROR);
		}
		if (!res)
		{
			MessageBoxW(hWnd, L"Unable to inject " DLL_NAME L" into process.", L"Error", MB_OK | MB_ICONERROR);
			someErrors = 1;
		}
		CloseHandle(hProcess);
	}
	if (someErrors)
		return -1;
	return 1;
}

int Inject(wchar_t *path)
{
	InjectionSettings cfg;
	if (LoadInjectionSettings(cfg, path, path, 0))
		return Inject(&cfg, 0, 0);
	return 0;
}
