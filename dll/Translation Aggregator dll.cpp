// "Test" in Shift-JIS
// "ƒeƒXƒg"


// Contains code used to figure out how to get ATLAS dll working.
// Also contains an early version of AGTH-ish code.  Need a communication
// mechanism, and to add more functions, and some sort of duplicate character/string
// filter, and then it's basically done, with a lot of caveats (No special code
/// support, currently only monitors exe (Easy to change), won't work for multi-exe
// games (Also an easy fix - just monitor the exe launching functions and inject when they're
// used)).

#include <Shared/Shrink.h>
#include <tlhelp32.h>
#include <Shared/StringUtil.h>
#include <Shared/Atlas.h>
#include <psapi.h>
#include "WindowTranslator.h"
#include "GetText.h"

#include <Shared/DllInjection.h>

#include "Hooks/HookManager.h"
#include "Hooks/TextHooks.h"
#include <Shared/InjectionSettings.h>

InjectionSettings settings;

HINSTANCE ghInst;

int quitting = 0;
HANDLE hThread = 0;

BOOL WINAPI PostCreateProcessW(
  BOOL res,
  __in_opt     wchar_t* lpApplicationName,
  __inout_opt  wchar_t* lpCommandLine,
  __in_opt     LPSECURITY_ATTRIBUTES lpProcessAttributes,
  __in_opt     LPSECURITY_ATTRIBUTES lpThreadAttributes,
  __in         BOOL bInheritHandles,
  __in         DWORD dwCreationFlags,
  __in_opt     LPVOID lpEnvironment,
  __in_opt     wchar_t* lpCurrentDirectory,
  __in         LPSTARTUPINFOW lpStartupInfo,
  __out        LPPROCESS_INFORMATION lpProcessInformation
  ) {
  // Note: res should be 0 when lpProcessInformation is 0, anyways.  It's a required parameter.
    if (res && lpProcessInformation) {
    InjectDll(lpProcessInformation->hProcess, ghInst);
  }
  return res;
}

BOOL WINAPI PostCreateProcessA(
  BOOL res,
  __in_opt     char* lpApplicationName,
  __inout_opt  char* lpCommandLine,
  __in_opt     LPSECURITY_ATTRIBUTES lpProcessAttributes,
  __in_opt     LPSECURITY_ATTRIBUTES lpThreadAttributes,
  __in         BOOL bInheritHandles,
  __in         DWORD dwCreationFlags,
  __in_opt     LPVOID lpEnvironment,
  __in_opt     char* lpCurrentDirectory,
  __in         LPSTARTUPINFOA lpStartupInfo,
  __out        LPPROCESS_INFORMATION lpProcessInformation
  ) {
  // Note: res should be 0 when lpProcessInformation is 0, anyways.  It's a required parameter.
    if (res && lpProcessInformation) {
    InjectDll(lpProcessInformation->hProcess, ghInst);
  }
  return res;
}

// No longer needed.
/*
BOOL WINAPI MyCreateProcessA(
  __in_opt     char* lpApplicationName,
  __inout_opt  char* lpCommandLine,
  __in_opt     LPSECURITY_ATTRIBUTES lpProcessAttributes,
  __in_opt     LPSECURITY_ATTRIBUTES lpThreadAttributes,
  __in         BOOL bInheritHandles,
  __in         DWORD dwCreationFlags,
  __in_opt     LPVOID lpEnvironment,
  __in_opt     char* lpCurrentDirectory,
  __in         LPSTARTUPINFOA lpStartupInfo,
  __out        LPPROCESS_INFORMATION lpProcessInformation
  ) {
  BOOL res;
  if (res = CreateProcessA(lpApplicationName, lpCommandLine, lpProcessAttributes, lpThreadAttributes, bInheritHandles, dwCreationFlags, lpEnvironment, lpCurrentDirectory, lpStartupInfo, lpProcessInformation)) {
    InjectDll(lpProcessInformation->hProcess, ghInst);
  }
  return res;
}

BOOL WINAPI MyCreateProcessW(
  __in_opt     wchar_t* lpApplicationName,
  __inout_opt  wchar_t* lpCommandLine,
  __in_opt     LPSECURITY_ATTRIBUTES lpProcessAttributes,
  __in_opt     LPSECURITY_ATTRIBUTES lpThreadAttributes,
  __in         BOOL bInheritHandles,
  __in         DWORD dwCreationFlags,
  __in_opt     LPVOID lpEnvironment,
  __in_opt     wchar_t* lpCurrentDirectory,
  __in         LPSTARTUPINFOW lpStartupInfo,
  __out        LPPROCESS_INFORMATION lpProcessInformation
  ) {
  BOOL res;
  if (res = CreateProcessW(lpApplicationName, lpCommandLine, lpProcessAttributes, lpThreadAttributes, bInheritHandles, dwCreationFlags, lpEnvironment, lpCurrentDirectory, lpStartupInfo, lpProcessInformation)) {
    InjectDll(lpProcessInformation->hProcess, ghInst);
  }
  return res;
}
//*/

// Credit for this function goes to Nasser R. Rowhani.
// http://www.codeproject.com/KB/DLL/DLL_Injection_tutorial.aspx
void *OverrideFunction(HMODULE stealFrom, char *oldFunctionModule, char *functionName, void *newFunction) {

  HMODULE oldModule = GetModuleHandleA(oldFunctionModule);
  if (!stealFrom || !oldModule) return 0;
  void *originalAddress = GetProcAddress(oldModule, functionName);
  if (!originalAddress) return 0;
  IMAGE_DOS_HEADER *dosHeader = (IMAGE_DOS_HEADER*) stealFrom;
  char *base = (char*)stealFrom;
  if (IsBadReadPtr(dosHeader, sizeof(IMAGE_DOS_HEADER)) || dosHeader->e_magic != IMAGE_DOS_SIGNATURE)
    return 0;
  IMAGE_NT_HEADERS *ntHeader = (IMAGE_NT_HEADERS*) (base + dosHeader->e_lfanew);
  if (IsBadReadPtr(ntHeader, sizeof(IMAGE_NT_HEADERS)) || ntHeader->Signature != IMAGE_NT_SIGNATURE)
    return 0;
  if (!ntHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress) return 0;
  IMAGE_IMPORT_DESCRIPTOR *import = (IMAGE_IMPORT_DESCRIPTOR*) (base + ntHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);
  while (import->Name) {
    char *name = base + import->Name;
    if (!stricmp(name, oldFunctionModule)) break;
    import++;
  }
  if (!import->Name) return 0;
  IMAGE_THUNK_DATA *thunk = (IMAGE_THUNK_DATA*) (base + import->FirstThunk);
  while (thunk->u1.Function) {
    if ((ULONG_PTR)thunk->u1.Function == (ULONG_PTR) originalAddress) {
      ULONG_PTR* addr = (ULONG_PTR*)&thunk->u1.Function;

      MEMORY_BASIC_INFORMATION mbi;

      if(VirtualQuery((LPVOID)(addr), &mbi, sizeof(mbi)) == sizeof(mbi)) {
        DWORD dwOldProtect;
        if (VirtualProtect(mbi.BaseAddress, ((ULONG_PTR)addr + 8)-(ULONG_PTR)mbi.BaseAddress, PAGE_EXECUTE_READWRITE, &dwOldProtect)) {
          *addr = (ULONG_PTR) newFunction;
          VirtualProtect(mbi.BaseAddress, ((ULONG_PTR)addr + 8)-(ULONG_PTR)mbi.BaseAddress, dwOldProtect, &dwOldProtect);
          return originalAddress;
        }
      }

    }
    thunk++;
  }

  return 0;
}

struct FunctionReplaceInfo {
  char *name;
  char *dll;
  void *myFxn;
  void *oldFxn;
};
void * WINAPI MyGetProcAddress(HMODULE h, char* lpProcName);
HMODULE WINAPI MyLoadLibrary(__in  LPCTSTR lpFileName);
HMODULE WINAPI MyLoadLibraryEx(__in LPCTSTR lpFileName, __reserved HANDLE hFile, __in DWORD dwFlags);

UINT WINAPI MyGetOEMCP(void) {
  return GetACP();
}
/*
UINT WINAPI MyGetACP(void) {
  return 932;
}
//*/

BOOL WINAPI MyIsDBCSLeadByte(
  __in  BYTE TestChar
  ) {
    return IsDBCSLeadByteEx(GetACP(), TestChar);
}

BOOL WINAPI MyIsDBCSLeadByteEx(
  UINT CodePage,
  __in  BYTE TestChar
  ) {
    return IsDBCSLeadByteEx(GetACP(), TestChar);
}

// Mostly for finding new and interesting functions to hook
/*BOOL WINAPI MySetWindowTextA(HWND hWnd, char *lpString2) {
  return SetWindowTextA(hWnd, lpString2);
}//*/

int GetFunctions(const FunctionReplaceInfo **fxns) {
  static const FunctionReplaceInfo functs[] = {
    //{"SetWindowTextA", "Kernel32.dll", MySetWindowTextA, SetWindowTextA},

    {"GetProcAddress", "Kernel32.dll", MyGetProcAddress, GetProcAddress},
    {"LoadLibrary", "Kernel32.dll", MyLoadLibrary, LoadLibrary},
    {"LoadLibraryEx", "Kernel32.dll", MyLoadLibraryEx, LoadLibraryEx},

    {"IsDBCSLeadByte", "Kernel32.dll", MyIsDBCSLeadByte, IsDBCSLeadByte},
    {"IsDBCSLeadByteEx", "Kernel32.dll", MyIsDBCSLeadByteEx, IsDBCSLeadByteEx},
    {"GetOEMCP", "Kernel32.dll", MyGetOEMCP, GetOEMCP},
    //{"GetACP", "Kernel32.dll", MyGetACP, GetACP},

    {"MessageBoxA", "User32.dll", MyMessageBoxA, MessageBoxA},
    {"MessageBoxW", "User32.dll", MyMessageBoxW, MessageBoxW},

    {"TrackPopupMenu", "User32.dll", MyTrackPopupMenu, TrackPopupMenu},
    // Flag function:  Last one I always take control of.
    {"TrackPopupMenuEx", "User32.dll", MyTrackPopupMenuEx, TrackPopupMenuEx},
  };
  *fxns = functs;

  return sizeof(functs)/sizeof(functs[0]);
}

void OverrideFunctions(HMODULE hModule) {
  const FunctionReplaceInfo *fxns;
  int numFxns = GetFunctions(&fxns);
  /*
  if (!(settings.injectionFlags & NO_INJECT_CHILDREN)) {
    const static FunctionReplaceInfo launchFxns[] = {
      {"CreateProcessA", "Kernel32.dll", MyCreateProcessA, CreateProcessA},
      {"CreateProcessW", "Kernel32.dll", MyCreateProcessW, CreateProcessW},
    };
    for (int i=0; i<2; i++) {
      void *t = OverrideFunction(hModule, launchFxns[i].dll, launchFxns[i].name, launchFxns[i].myFxn);
    }
  }
  //*/
  for (int i=0; i<numFxns; i++) {
    if (!(settings.injectionFlags & TRANSLATE_MENUS) &&
      (fxns[i].oldFxn == MessageBoxA ||
       fxns[i].oldFxn == MessageBoxW ||
       fxns[i].oldFxn == TrackPopupMenu ||
       fxns[i].oldFxn == TrackPopupMenuEx)) continue;
    if (!(settings.injectionFlags & DCBS_OVERRIDE) &&
      (fxns[i].oldFxn == GetOEMCP ||
       fxns[i].oldFxn == MyIsDBCSLeadByte ||
       fxns[i].oldFxn == MyIsDBCSLeadByteEx)) continue;
    void *t = OverrideFunction(hModule, fxns[i].dll, fxns[i].name, fxns[i].myFxn);
  }
}

HMODULE WINAPI MyLoadLibrary(__in LPCTSTR lpFileName) {
  HMODULE test = GetModuleHandle(lpFileName);
  HMODULE out = LoadLibrary(lpFileName);
  if (!test) {
    OverrideFunctions(out);
  }
  return out;
}

HMODULE WINAPI MyLoadLibraryEx(__in LPCTSTR lpFileName, __reserved HANDLE hFile, __in DWORD dwFlags) {
  HMODULE test = GetModuleHandle(lpFileName);
  HMODULE out = LoadLibraryEx(lpFileName, hFile, dwFlags);
  if (!test) {
    OverrideFunctions(out);
  }
  return out;
}

void * WINAPI MyGetProcAddress(HMODULE h, char* lpProcName) {
  char dllFile[2*MAX_PATH];
  char *name;
  if (GetModuleFileNameA(h, dllFile, sizeof(dllFile))) {
    const FunctionReplaceInfo *fxns;
    int numFxns = GetFunctions(&fxns);
    if (name = strrchr(dllFile, '\\')) name ++;
    else name = dllFile;
    for (int i=0; i<numFxns; i++) {
      if (!stricmp(name, fxns[i].dll) && !stricmp(lpProcName, fxns[i].name))
        return fxns[i].myFxn;
    }
  }

  return GetProcAddress(h, lpProcName);
}

wchar_t exePath[MAX_PATH*2];

int Init() {
  int i = 0;
  wchar_t path[MAX_PATH*2];
  wchar_t *exeName = wcsrchr(exePath, '\\');
  if (!exeName) return 0;
  exeName++;

  HMODULE modules[2000];
  DWORD size;
  if (exeName) {
    if (EnumProcessModules(GetCurrentProcess(), modules, sizeof(modules), &size) && (size/=4)) {
      for (i=0; i<(int)size; i++) {
        if (GetModuleFileNameW(modules[i], path, sizeof(path)/2)) {
          if (!wcsnicmp(path, exePath, exeName-exePath)) {
            OverrideFunctions(modules[i]);
          }
        }
      }
    }
  }
  return 1;
}


void Uninit() {
}


DWORD WINAPI ThreadProc(void *junk) {
  int pid = GetCurrentProcessId();
  int tid = GetCurrentThreadId();
  Buffer buffer = {0,0};
  buffer.size = 3000;
  buffer.text = (wchar_t*) malloc(sizeof(wchar_t) * 3000);
  int idle = 0;
  int sleepTime = 500;
  int checkWindowInterval = 500;
  int checkWindowTime = 0;
  int rehookInterval = 10000;
  int rehookTime = 5000;
  int t = GetTickCount();
  while(1) {
    if (!AtlasIsLoaded()) {
      InitAtlas(settings.atlasConfig, ATLAS_JAP_TO_ENG);
    }
    int oldt = t;
    t = GetTickCount();
    int dt = t - oldt;
    if (quitting) break;
    //CheckDrawing(t);
    rehookTime -= dt;
    if (rehookTime <= 0) {
      Init();
      rehookTime = rehookInterval;
    }
    checkWindowTime -= dt;
    if (checkWindowTime <= 0 && (settings.injectionFlags & TRANSLATE_MENUS) && AtlasIsLoaded()) {
      checkWindowTime = checkWindowInterval;
      HANDLE h = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
      if (h == INVALID_HANDLE_VALUE) {
        break;
      }
      THREADENTRY32 thread;
      thread.dwSize = sizeof(thread);
      int count = 0;
      int windows = 0;
      if (Thread32First(h, &thread)) {
        do {
          if (thread.th32OwnerProcessID != pid || thread.th32ThreadID == tid) continue;
          windows |= TranslateThreadWindows(thread.th32ThreadID, buffer);
          count++;
        }
        while(Thread32Next(h, &thread));
      }
      CloseHandle(h);
      if (!count) break;
      if (!windows) {
        idle++;
        if (idle >= 40)
          break;
      }
    }
    Sleep(sleepTime);
  }
  // Never seem to end up here, for some reason.
  free(buffer.text);
  UninitAtlas();
  return 0;
}

static time_t lastTry;
int TryReconnect();

int SendAll(SOCKET &sock, char *data, int len, int tryConnectOnFail) {
  if (sock == INVALID_SOCKET) {
    if (!tryConnectOnFail || time(0) - lastTry < 1 || !TryReconnect()) {
      return 0;
    }
  }
  int sent = 0;
  while (sent < 4) {
    int s = send(sock, (char*)&len, 4-sent, 0);
    if (!s || s == SOCKET_ERROR) {
      closesocket(sock);
      sock = INVALID_SOCKET;
      return 0;
    }
    sent += s;
  }
  sent = 0;
  while (sent < len) {
    int s = send(sock, data+sent, len-sent, 0);
    if (!s || s == SOCKET_ERROR) {
      closesocket(sock);
      sock = INVALID_SOCKET;
      return 0;
    }
    sent += s;
  }
  return 1;
}

void *GetAllData(SOCKET &sock, int *len) {
  if (sock == INVALID_SOCKET) return 0;
  int rec = 0;
  *len = 0;
  while (rec < 4) {
    int r = recv(sock, ((char*)len)+rec, 4-rec, 0);
    if (!r ||r == SOCKET_ERROR) {
      closesocket(sock);
      sock = INVALID_SOCKET;
      return 0;
    }
    rec += r;
  }
  rec = 0;
  char *data = (char*) malloc(*len);
  while (rec < *len) {
    int r = recv(sock, data+rec, *len-rec, 0);
    if (!r ||r == SOCKET_ERROR) {
      closesocket(sock);
      sock = INVALID_SOCKET;
      free(data);
      return 0;
    }
    rec += r;
  }
  return data;
}

SOCKET sock = INVALID_SOCKET;
wchar_t LauncherName[MAX_PATH*2];
int GetSettings(InjectionSettings &settings, wchar_t **internalHooks) {
  lastTry = time(0);
  HANDLE hMapping = CreateFileMapping(INVALID_HANDLE_VALUE, 0, PAGE_READONLY, 0, 2, MAIN_WINDOW_TITLE);
  if (!hMapping) return 0;
  if (GetLastError() != ERROR_ALREADY_EXISTS) {
    CloseHandle(hMapping);
    return 0;
  }
  void *v = MapViewOfFile(hMapping, FILE_MAP_READ, 0, 0, 2);
  if (!v) {
    CloseHandle(hMapping);
    return 0;
  }
  unsigned short port = *(unsigned short*)v;
  UnmapViewOfFile(v);
  CloseHandle(hMapping);

  sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (sock == INVALID_SOCKET) {
    return 0;
  }
  sockaddr_in addr;
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");

  int headerLen = sizeof(wchar_t) * (1 + wcslen(MAIN_WINDOW_TITLE));
  wchar_t temp[1024 + MAX_PATH*2];
  int len;
  if (connect(sock, (sockaddr*)&addr, sizeof(addr)) || !SendAll(sock, (char*)MAIN_WINDOW_TITLE, headerLen, 0) ||
    // Should all arrive together, since it's so short.  May make more robust later.
    4 != recv(sock, (char*)&len, 4, 0) || len != headerLen ||
    headerLen != recv(sock, (char*)temp, headerLen, 0) || wcscmp(temp, MAIN_WINDOW_TITLE)) {
      closesocket(sock);
      sock = INVALID_SOCKET;
      return 0;
  }
  InjectionSettings *settings2 = 0;
  DWORD junk = 0;
  wcscpy(temp, LauncherName);
  wchar_t *end = wcsrchr(temp, 0);
  wsprintf(end, L"*%i", GetCurrentProcessId());
  if (!SendAll(sock, (char*)temp, sizeof(wchar_t) * (wcslen(temp)+1), 0) ||
    !(settings2 = (InjectionSettings *)GetAllData(sock, &len)) || len != sizeof(settings) ||
    !(*internalHooks = (wchar_t*)GetAllData(sock, &len)) || !len || (len&1) || internalHooks[0][len/2-1]) {
      closesocket(sock);
      sock = INVALID_SOCKET;
      return 0;
  }
  settings = *settings2;
  free(settings2);
  return 1;
}

int TryReconnect() {
  // Ignore new settongs on reconnect.
  InjectionSettings junkSettings;
  wchar_t *internalHooks = 0;
  int res = GetSettings(junkSettings, &internalHooks);
  free(internalHooks);
  return res;
}

#define TA_LAUNCH_NAME L"__TA_LAUNCHER_NAME"

BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD fdwReason, void* lpvReserved) {
  static int winsockHappy = 0;
  if (fdwReason == DLL_PROCESS_ATTACH) {
    SuspendedThreads *threads = SuspendThreads();
    DisableThreadLibraryCalls(hInstance);
    GetModuleFileNameW(0, exePath, sizeof(exePath)/sizeof(wchar_t));
    WSADATA wsaData;
    winsockHappy = !WSAStartup(0x202, &wsaData);
    ghInst = hInstance;
    wchar_t *internalHooks = 0;
    int len = GetEnvironmentVariable(TA_LAUNCH_NAME, LauncherName, sizeof(LauncherName)/sizeof(wchar_t));
    if (len <= 0 || len >= sizeof(LauncherName)/sizeof(wchar_t) || !LauncherName[0]) {
      if (!GetModuleFileName(0, LauncherName, sizeof(LauncherName)/sizeof(wchar_t))) {
        ResumeThreads(threads);
        MessageBox(0, L"Failed to get process name", L"Error", MB_OK);
        return 0;
      }
    }
    if (!winsockHappy || !GetSettings(settings, &internalHooks)) {
      free(internalHooks);
      ResumeThreads(threads);
      MessageBox(0, L"Failed to get settings", L"Error", MB_OK);
      return 0;
    }
    if ((settings.injectionFlags & (NO_INJECT_CHILDREN | NO_INJECT_CHILDREN_SAME_SETTINGS))) {
      SetEnvironmentVariableW(TA_LAUNCH_NAME, 0);
    }
    else {
      SetEnvironmentVariableW(TA_LAUNCH_NAME, LauncherName);
    }
    //while(1);
    SetLogFile(settings.logPath);
    SetCacheSize(1000);
    Init();

    if (settings.injectionFlags & TRANSLATE_MENUS) {
      if (!(hThread = CreateThread(0, 0, ThreadProc, 0, 0, 0))) {
        //Uninit();
        ResumeThreads(threads);
        free(internalHooks);
        MessageBox(0, L"Failed launch translation thread", L"Error", MB_OK);
        return 0;
      }
    }
    if (!(settings.injectionFlags & NO_INJECT_CHILDREN)) {
      HookAfterFunction(CreateProcessW, L"", 0, PostCreateProcessW);
      HookAfterFunction(CreateProcessA, L"", 0, PostCreateProcessA);
    }
    if (settings.injectionFlags & INTERNAL_HOOK) {
      HookTextFunctions(internalHooks, !(settings.injectionFlags & INTERNAL_NO_DEFAULT_HOOKS), !(settings.injectionFlags & INTERNAL_NO_DEFAULT_FILTERS), settings.internalHookDelay);
    }
    else {
      closesocket(sock);
      sock = INVALID_SOCKET;
      WSACleanup();
      winsockHappy = 0;
    }
    free(internalHooks);
    ResumeThreads(threads);
  }
  else if (fdwReason == DLL_PROCESS_DETACH) {
    quitting = 1;

    WaitForSingleObject(hThread, 5000);
    CloseHandle(hThread);
    if (winsockHappy) {
      WSACleanup();
    }
  }
  return 1;
}

