#include <Shared/Shrink.h>
#include "HookManager.h"
#include <tlhelp32.h>
#include <Shared/TextHookParser.h>


static CRITICAL_SECTION cSection;
static int cSectionInitialized = 0;
static int resolveDelay = 0;

struct DllInfo {
  ULONG_PTR base;
  ULONG_PTR end;
  int systemDll;
#ifdef _DEBUG
  wchar_t *name;
#endif
};

DllInfo *dlls = 0;
int numDlls = 0;

extern wchar_t exePath[MAX_PATH*2];

void UpdateDlls() {
#ifdef _DEBUG
  for (int i=0; i<numDlls; i++) {
    free(dlls[i].name);
  }
#endif
  free(dlls);
  numDlls = 0;
  dlls = 0;

  int pathLen;
  wchar_t *pos = wcsrchr(exePath, '\\');
  if (pos)
    pathLen = pos-exePath;
  else
    pathLen = wcslen(exePath);

  /*
  HMODULE modules[1024];
  DWORD needed;
  HANDLE hProcess = GetCurrentProcess();
  if ((EnumProcessModules(hProcess, modules, sizeof(modules), &needed) || GetLastError() == ERROR_PARTIAL_COPY) && needed < sizeof(modules)) {
    // Don't use numDlls just in case...
    int numModules = needed / sizeof(modules[0]);
    dlls = (DllInfo*) malloc(sizeof(DllInfo) * numModules);
    for (int i = 0; i < numModules; i++) {
      wchar_t path2[MAX_PATH*2];
      MODULEINFO info;
      if (GetModuleFileName(modules[i], path2, sizeof(path2)/sizeof(wchar_t)) && GetModuleInformation(hProcess, modules[i], &info, sizeof(info))) {
        DllInfo *dll = dlls + numDlls;
        dll->base = (ULONG_PTR)info.lpBaseOfDll;
        dll->end = dll->base + info.SizeOfImage;
        if (!wcsnicmp(path2, path, pathLen)) {
          dll->systemDll = 0;
        }
        else {
          dll->systemDll = 1;
        }
        numDlls++;
      }
    }
  }
  //*/
  HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, 0);
  if (hSnap == INVALID_HANDLE_VALUE) return;

  MODULEENTRY32 entry;
  entry.dwSize = sizeof(entry);
  if (Module32First(hSnap, &entry) && entry.dwSize >= sizeof(entry)) {
    do {
      if (numDlls % 10 == 0) {
        dlls = (DllInfo*) realloc(dlls, (numDlls+10) * sizeof(DllInfo));
      }
      DllInfo *info = &dlls[numDlls];
      info->base = (ULONG_PTR) entry.modBaseAddr;
      info->end = info->base + entry.modBaseSize;
      wchar_t path[MAX_PATH*2];
      GetModuleFileName(entry.hModule, path, sizeof(path)/sizeof(wchar_t));
      if (!wcsnicmp(path, exePath, pathLen)) {
        info->systemDll = 0;
      }
      else {
        info->systemDll = 1;
      }
      #ifdef _DEBUG
      info->name = wcsdup(path);
      #endif
      numDlls++;
      entry.dwSize = sizeof(entry);
    }
    while (Module32Next(hSnap, &entry));
  }
  CloseHandle(hSnap);
  //*/
}

__forceinline int AddressIsSystemDll(ULONG_PTR addr) {
  static time_t StartTime = time(0);
  static time_t UpdateTime = 0;
  if (!numDlls) {
    UpdateTime = time(0);
    // Need to block initially for app to load.
    // Can only then call module info functions without hanging.
    if (UpdateTime - StartTime < resolveDelay)
      return 1;
    UpdateDlls();
  }
  for (int i=0; i<numDlls; i++) {
    if (dlls[i].base <= addr && addr < dlls[i].end) {
      UpdateTime = 0;
      return dlls[i].systemDll;
    }
  }
  if (time(0) != UpdateTime) {
    UpdateDlls();
    for (int i=0; i<numDlls; i++) {
      if (dlls[i].base <= addr && addr < dlls[i].end) {
        // If I find it, don't count it against refreshing list.
        // Really just want to prevent checking again and again
        // for funky code that I can't find in module list.
        UpdateTime = 0;
        return dlls[i].systemDll;
      }
    }
  }
  return 0;
}

extern SOCKET sock;

int SendAll(SOCKET &sock, char *data, int len, int tryConnectOnFail);

void TextHook(Registers *regs, void *param, wchar_t *id) {
  unsigned char *codePos = (unsigned char *)TextHook;
#ifdef _DEBUG
    int *jadder = (int*) (codePos+1);
    codePos = *jadder + (unsigned char*)(jadder+1);
#endif
  // Prevent recursion.
  if (*(ULONG_PTR*)regs->esp >= (ULONG_PTR) codePos && *(ULONG_PTR*)regs->esp <= (ULONG_PTR) codePos+0x1000)
    return;

  TextHookInfo *info = (TextHookInfo*) param;

  EnterCriticalSection(&cSection);
  if (info->defaultFilter) {
    // Check where I'm coming from.
    if (AddressIsSystemDll(*(ULONG*)regs->esp)) {
      LeaveCriticalSection(&cSection);
      return;
    }
  }

  int error = 0;
  ULONG value = info->value->Eval(regs, &error);
  int length = -1;
  if (info->length)
    length = info->length->Eval(regs, &error);
  int codePage = 0;
  if (info->codePage) {
    codePage = info->codePage->Eval(regs, &error);
    if (codePage == -1) codePage = 0;
  }
  if (error) {
    LeaveCriticalSection(&cSection);
    return;
  }

  wchar_t contextString[2024];
  wcscpy(contextString, info->alias);

  int pos = 0;
  CompiledExpression **context = info->contexts;
  while (context[0]) {
    pos += wcslen(contextString+pos);
    wchar_t *append = L"::";
    if (context != info->contexts)
      append = L";";
    wsprintf(contextString+pos, L"%s%X", append, context[0]->Eval(regs, &error));
    context++;
  }
  if (error) {
    LeaveCriticalSection(&cSection);
    return;
  }

  wchar_t *string;
  if (info->type & TEXT_PTR) {
    if (IsBadReadPtr((ULONG*)value, 1)) {
      LeaveCriticalSection(&cSection);
      return;
    }
  }
  if (info->type == TEXT_WCHAR_PTR || info->type == TEXT_WCHAR) {
    wchar_t *tempString = (wchar_t*)value;
    if (info->type == TEXT_WCHAR) {
      length = 1;
      tempString = (wchar_t*)&value;
    }
    else if (length < 0) {
      length = wcslen((wchar_t*)value);
    }
    else {
      for (int i=0; i<length; i++) {
        if (!((wchar_t*)value)[i]) {
          length = i;
          break;
        }
      }
    }
    if (length < 0 || length > 1024*1024) length = 0;
    string = (wchar_t *)malloc((length +1) * sizeof(wchar_t));
    string[length] = 0;
    if (!info->flip) {
      memcpy(string, tempString, length * sizeof(wchar_t));
    }
    else {
      for (int i=0; i<length; i++) {
        string[i] = (tempString[i]>>8) + (tempString[i]<<8);
      }
    }
  }
  else {
    char temp[4];
    char *tempString = (char*) value;
    if (info->type == TEXT_CHAR) {
      char *next;
      tempString = (char*) &value;
      if (!info->flip) {
        if (length < 0) {
          length = 4;
          for (int i=1; i<4; i++) {
            if (!tempString[i]) {
              length = i;
              break;
            }
          }
        }
        if (length > 4)
          length = 4;
        if (length == 1) {
          temp[0] = tempString[0];
        }
        else if (length == 2) {
          temp[0] = tempString[1];
          temp[1] = tempString[0];
        }
        else if (length == 3) {
          temp[0] = tempString[2];
          temp[1] = tempString[1];
          temp[2] = tempString[0];
        }
        else if (length == 4) {
          temp[0] = tempString[3];
          temp[1] = tempString[2];
          temp[2] = tempString[1];
          temp[3] = tempString[0];
        }
        tempString = temp;
      }
      else {
        tempString = (char*)&value;
        if (length == -1) {
          next = CharNextExA(codePage, tempString, 0);
          if (!next)
            length = 0;
          else
            length = next-tempString;
        }
        else if (length > 4) {
          length = 1;
        }
      }
    }
    else if (length < 0) {
      length = strlen(tempString);
    }
    else {
      for (int i=0; i<length; i++) {
        if (!tempString[i]) {
          length = i;
          break;
        }
      }
    }
    if (length < 0 || length > 1024*1024) length = 0;
    string = (wchar_t *)malloc((length +1) * sizeof(wchar_t));
    int length2 = MultiByteToWideChar(codePage, 0, tempString, length, string, length+1);
    if (length2 >= 0 && length2 <= length)
      string[length2] = 0;
    else
      string[0] = 0;
  }

  wchar_t *text = (wchar_t*)malloc(sizeof(wchar_t) * (wcslen(string) + wcslen(contextString) + 20));
  wsprintf(text, L"%s\n%s", contextString, string);
  int len = wcslen(text);
  if (len) {
    SendAll(sock, (char*)text, 2 * (len + 1), 1);
  }

  free(string);
  free(text);
  LeaveCriticalSection(&cSection);
}

int HookText(TextHookInfo *info) {
  if (!info)
    return 0;
  if (!HookRawAddress(info->address, info->hookText, info, TextHook)) {
    free(info);
    return 0;
  }
  return 1;
}

int HookText(wchar_t *id, void* address, unsigned int type, wchar_t *value, wchar_t *context, wchar_t *length, wchar_t *codePage, int defaultFilter) {
  TextHookInfo *info = CreateTextHookInfo(id, 0, address, type, value, context, length, codePage, defaultFilter);
  return HookText(info);
}

int TryUserHookText(wchar_t *hook) {
  TextHookInfo *info = ParseTextHookString(hook, 0);
  return HookText(info);
}

// dupString is mostly for debugging.
void HookText(wchar_t *string, int dupString = 0) {
  if (dupString)
    string = wcsdup(string);
  if (dupString)
    free(string);
}

int HookTextFunctions(wchar_t *hooks, unsigned int defaultHooks, unsigned int defaultFilter, int internalHookDelay) {
  resolveDelay = internalHookDelay;
  InitializeCriticalSection(&cSection);
  if (defaultHooks) {
    HookText(L"MultiByteToWideChar", MultiByteToWideChar, TEXT_CHAR_PTR,  L"_12", 0, L"_16", L"_4", defaultFilter);
    HookText(L"WideCharToMultiByte", WideCharToMultiByte, TEXT_WCHAR_PTR, L"_12", 0, L"_16", 0, defaultFilter);

    HookText(L"DrawTextA", DrawTextA, TEXT_CHAR_PTR,  L"_8", 0, L"_12", 0, defaultFilter);
    HookText(L"DrawTextW", DrawTextW, TEXT_WCHAR_PTR, L"_8", 0, L"_12", 0, defaultFilter);
    HookText(L"DrawTextExA", DrawTextExA, TEXT_CHAR_PTR,  L"_8", 0, L"_12", 0, defaultFilter);
    HookText(L"DrawTextExW", DrawTextExW, TEXT_WCHAR_PTR, L"_8", 0, L"_12", 0, defaultFilter);

    HookText(L"TextOutA", TextOutA, TEXT_CHAR_PTR,  L"_16", 0, L"_20", 0, defaultFilter);
    HookText(L"TextOutW", TextOutW, TEXT_WCHAR_PTR, L"_16", 0, L"_20", 0, defaultFilter);
    HookText(L"ExtTextOutA", ExtTextOutA, TEXT_CHAR_PTR,  L"_24", 0, L"_28", 0, defaultFilter);
    HookText(L"ExtTextOutW", ExtTextOutW, TEXT_WCHAR_PTR, L"_24", 0, L"_28", 0, defaultFilter);


    HookText(L"GetGlyphOutlineA", GetGlyphOutlineA, TEXT_CHAR,  L"_8", 0, L"-1", 0, defaultFilter);
    HookText(L"GetGlyphOutlineW", GetGlyphOutlineW, TEXT_WCHAR, L"_8", 0, 0, 0, defaultFilter);

    HookText(L"GetTextExtentPoint32A", GetTextExtentPoint32A, TEXT_CHAR_PTR,  L"_8", 0, L"_12", 0, defaultFilter);
    HookText(L"GetTextExtentPoint32W", GetTextExtentPoint32W, TEXT_WCHAR_PTR, L"_8", 0, L"_12", 0, defaultFilter);

    HookText(L"CharNextA", CharNextA, TEXT_CHAR_PTR,  L"[_4]", 0, L"-1", 0, defaultFilter);
    HookText(L"CharNextW", CharNextW, TEXT_WCHAR_PTR, L"[_4]", 0, 0, 0, defaultFilter);

    HookText(L"CharNextExA", CharNextExA, TEXT_CHAR_PTR,  L"[_8]", 0, L"-1", L"_4", defaultFilter);
    // Doesn't exist...
    //HookText(L"CharNextExW", CharNextExW, TEXT_WCHAR, L"[_8]", 0, 0, L"_4", defaultFilter);

    // Currently can't handle these
    //HookText(L"CharPrevA", CharNextA, TEXT_CHAR,  L"_4", 0, L"-2", 0, defaultFilter);
    //HookText(L"CharPrevW", CharNextW, TEXT_WCHAR, L"_4", 0, 0, 0, defaultFilter);

    //HookText(L"CharPrevExA", CharNextExA, TEXT_CHAR,  L"_8", 0, L"-2", L"_4", defaultFilter);
    //HookText(L"CharPrevExW", CharNextExW, TEXT_WCHAR, L"_8", 0, 0, L"_4", defaultFilter);
  }

  wchar_t *p = hooks;
  while (*p) {
    int len = wcscspn(p, L" \r\n\t");
    if (!len) {
      p++;
      continue;
    }
    wchar_t c = p[len];
    p[len] = 0;

    TryUserHookText(p);

    p[len] = c;
    p+=len;
  }
  return 1;
}
