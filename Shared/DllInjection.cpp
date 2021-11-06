// Not currently used.
#include <Shared/Shrink.h>

#include "DllInjection.h"
#include <tlhelp32.h>

BOOL CALLBACK FindProcessWindow(HWND hWnd, LPARAM lParam)
{
	DWORD *pid = (DWORD*)lParam;
	DWORD windowPid = 0;
	DWORD junk = GetWindowThreadProcessId(hWnd, &windowPid);
	if (windowPid == *pid)
	{
		*pid = 0;
		return 0;
	}
	return 1;
}

/*
// Not used, but may be handy in the future.
unsigned long GetTargetThreadIdFromProcname(char *procName)
{
	PROCESSENTRY32 pe;
	HANDLE thSnapshot, hProcess;
	BOOL retval, ProcFound = false;
	unsigned long pTID, threadID;

	thSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

	if(thSnapshot == INVALID_HANDLE_VALUE)
	{
		MessageBoxA(NULL, "Error: unable to create toolhelp snapshot", "Loader", NULL);
		return false;
	}

	pe.dwSize = sizeof(PROCESSENTRY32);

	retval = Process32First(thSnapshot, &pe);

	while(retval)
	{
		if(!stricmp(pe.szExeFile, procName) )
		{
			ProcFound = true;
			break;
		}

		retval    = Process32Next(thSnapshot,&pe);
		pe.dwSize = sizeof(PROCESSENTRY32);
	}

	CloseHandle(thSnapshot);

	_asm
	{
		mov eax, fs:[0x18]
		add eax, 36
		mov [pTID], eax
	}

	hProcess = OpenProcess(PROCESS_VM_READ, false, pe.th32ProcessID);
	ReadProcessMemory(hProcess, (const void *)pTID, &threadID, 4, NULL);
	CloseHandle(hProcess);

	return threadID;
}

// Not used, but may be handy in the future.
unsigned long GetTargetProcessIdFromProcname(char *procName)
{
	PROCESSENTRY32 pe;
	HANDLE thSnapshot;
	BOOL retval, ProcFound = false;
	thSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if(thSnapshot == INVALID_HANDLE_VALUE)
	{
		MessageBox(NULL, "Error: unable to create toolhelp snapshot", "Loader", NULL);
		return false;
	}
	pe.dwSize = sizeof(PROCESSENTRY32);
	retval = Process32First(thSnapshot, &pe);
	while(retval)
	{
		if(!stricmp(pe.szExeFile, procName) )
		{
			ProcFound = true;
			break;
		}
		retval    = Process32Next(thSnapshot,&pe);
		pe.dwSize = sizeof(PROCESSENTRY32);
	}
	return pe.th32ProcessID;
}
//*/

int InjectDll(HANDLE hProcess, HINSTANCE hInst)
{
	HANDLE hThread;

	wchar_t dllFile[2*MAX_PATH];
	if (GetModuleFileNameW(hInst, dllFile, sizeof(dllFile)/2) > sizeof(dllFile)/2) return 0;

	FARPROC loadLibraryW = GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "LoadLibraryW");
	if (!loadLibraryW) return 0;

	wchar_t *name;
	if (!(name = wcsrchr(dllFile, '\\'))) return 0;
	name ++;
	wcscpy(name, DLL_NAME);
	if (GetFileAttributes(dllFile) == INVALID_FILE_ATTRIBUTES) return 0;

	size_t len = sizeof(wchar_t)*(1+wcslen(dllFile));
	void *remoteString = (LPVOID)VirtualAllocEx(hProcess, NULL, len, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
	if (remoteString)
	{
		if (WriteProcessMemory(hProcess, (LPVOID)remoteString, dllFile, len, NULL))
		{
			if (hThread = CreateRemoteThread(hProcess, 0, 0, (LPTHREAD_START_ROUTINE)loadLibraryW, (LPVOID)remoteString, 0,0))
			{
				WaitForSingleObject(hThread, 3000);
				CloseHandle(hThread);
				VirtualFreeEx(hProcess, remoteString, len, MEM_FREE);
				// Make sure it's injected before resuming.
				return 1;
			}
		}
		VirtualFreeEx(hProcess, remoteString, len, MEM_FREE);
	}
	return 0;
}

HANDLE Run(const wchar_t *exe, DWORD *processId, LCID lcid)
{
	if (!exe) return 0;
	STARTUPINFOW startInfo;
	PROCESS_INFORMATION procInfo;
	memset(&startInfo, 0, sizeof(startInfo));
	startInfo.cb = sizeof(startInfo);
	wchar_t *temp = wcsdup(exe);
	wchar_t *q = wcsrchr(temp, '\\');
	if (q) q[1] = 0;
	if (lcid)
	{
		SetEnvironmentVariableA("__COMPAT_LAYER", "#ApplicationLocale");
		char temp[20];
		sprintf(temp, "%04X", lcid);
		SetEnvironmentVariableA("AppLocaleID", temp);
	}
	else
	{
		SetEnvironmentVariableA("__COMPAT_LAYER", 0);
		SetEnvironmentVariableA("AppLocaleID", 0);
	}
	if (CreateProcessW(exe, 0, 0, 0, 0, 0, 0, temp, &startInfo, &procInfo))
	{
		*processId = procInfo.dwProcessId;
		CloseHandle(procInfo.hThread);
		free(temp);
		return procInfo.hProcess;
		//ResumeThread(procInfo.hThread);
	}
	return 0;
}

int getPriv(char * name)
{
	HANDLE hToken;
	LUID seValue;
	TOKEN_PRIVILEGES tkp;

	if (!LookupPrivilegeValueA(NULL, name, &seValue) ||
		!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken))
			return 0;

	tkp.PrivilegeCount = 1;
	tkp.Privileges[0].Luid = seValue;
	tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

	int res = AdjustTokenPrivileges(hToken, 0, &tkp, sizeof(tkp), NULL, NULL);

	CloseHandle(hToken);
	return res;
}

int getDebugPriv()
{
	return getPriv("SeDebugPrivilege");
}


int InjectIntoProcess(int pid)
{
	HANDLE hProcess = OpenProcess(CREATE_THREAD_ACCESS, 0, pid);
	if (hProcess == 0)
	{
		getDebugPriv();
		hProcess = OpenProcess(CREATE_THREAD_ACCESS, 0, pid);
		if (!hProcess) return 0;
	}

	int out = InjectDll(hProcess);

	CloseHandle(hProcess);
	return out;
}
