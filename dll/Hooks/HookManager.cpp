#include <Shared/Shrink.h>
#include "HookManager.h"
#include "Disasm.h"
#include <tlhelp32.h>

#define JUNK 0xDEADBEEF

#define HOOK_BEFORE    0
#define HOOK_AFTER    1

struct HookMem {
  unsigned char *base;
  int used;
};

struct HookInfo {
  void *fxnBase;
  HookMem *mem;
  HookFxn *fxn;
  wchar_t *id;
  void *param;
  int type;
};

static HookInfo *hooks = 0;
static int numHooks = 0;

static HookMem *hookMem = 0;
static int numHookMem = 0;

__declspec(naked) void PreCallStub() {
  int id;
  Registers *regs;
  __asm {
    pushad
    mov eax, esp
    pushfd
    mov ebp, esp
    sub esp, 0x30
    mov regs, eax
    mov id, JUNK
  }

  hooks[id].fxn(regs, hooks[id].param, hooks[id].id);

  __asm {
    add esp, 0x30
    popfd
    popad
    push JUNK
  }
}
// Note:  Assumes __stdcall calling convention.
__declspec(naked) void PostStdCallStub() {
  __asm {
    // Modifies eax, ecx, edx without restoring them.
    // Only safe to hook the start of a stdcall function.
    push ebp
    mov ebp, esp
    // Overestimate of parameter count.
    sub esp, 0x100
    // Actually move 0x100/4 - 1 parameters.
    mov eax, 0x100
DupArgs:
    // Duplicate arguments on stack.
    mov ecx, [ebp+eax+4]
    mov [esp+eax-4], ecx
    sub eax, 4
    jnz DupArgs
    // Serves two purposes: 
    // 1) Lets me stick original code and jump to rest of
    //    function at end of stub, just like with PreCallStub.
    // 2) Pushes the correct address to return to.  Much Simpler
    //    than calculating it myself and pushing it.
    // As it's a relative call, no need to do anything to it while
    // putting together the hook, too.
    call OriginalFunctionStart
    mov esp, ebp
    pop ebp
    // Add output of original function to stack, as first param.
    // Must also move return address to top of stack.
    pop edx
    push eax
    push edx

    // Replace with call to my function.
    push JUNK
OriginalFunctionStart:
    push JUNK
  }
}
//*/
/*
__declspec(naked) void Stub2() {
  __asm {
    mov ebp, esp
    sub esp, 0x100
    mov eax, 0x9C
DupArgs:
    mov ecx, [ebp-eax]
    mov [esp-eax+4], ecx
    sub eax, 4
    jnz DupArgs
    // address of return position
    mov [esp], JUNK
  }
  // Do stuff and jump
  push JUNK
  // return here.  Need to replace
  call JUNK

  hooks[id].fxn(regs, hooks[id].param, hooks[id].id);

  __asm {
    add esp, 0x30
    popfd
    popad
    push JUNK
  }
}
//*/

// If true, returns length of opcode, calculates destination address, and 32-bit replacement opcode.
// Currently does *not* support jcxz, jecxz
static int IsRelativeJump(unsigned char *op, unsigned char **dest, unsigned char *replacePrefix, int *replacePrefixLen) {
  int size = 4;
  int opSize = 0;
  if (*op == 0x66) {
    size = 2;
    op++;
    opSize++;
  }
  replacePrefixLen[0] = 1;
  switch (*op) {
    case 0x0F: // conditional jump (16/32)
      if (op[1]<0x80 || op[1]>0x8F)
        return 0;
      replacePrefix++[0] = op++[0];
      replacePrefixLen[0]++;
      opSize++;
    case 0xE8: // call (16/32)
    case 0xE9: // jmp (16/32)
      replacePrefix[0] = *op;
      op++;
      if (size == 4)
        *dest = op + 4 + *(int*)op;
      else
        *dest = op + 2 + *(short*)op;
      return 1 + size + opSize;
    // Conditional jump (8)
    case 0x70: case 0x71: case 0x72: case 0x73:
    case 0x74: case 0x75: case 0x76: case 0x77:
    case 0x78: case 0x79: case 0x7A: case 0x7B:
    case 0x7C: case 0x7D: case 0x7E: case 0x7F:
      replacePrefixLen[0] = 2;
      replacePrefix[0] = 0x0F;
      replacePrefix[1] = *op + 0x10;
      *dest = op+2 + (char)op[1];
      return 2;
    case 0xEB: // jmp (0)
      replacePrefixLen[0] = 1;
      replacePrefix[0] = 0xE9;
      *dest = op+2 + (char)op[1];
      return 2;
    case 0xE3: // jcxz / jecxz.  Really doubt I have this one right.
      if (size == 2) {
        replacePrefixLen[0] = 6;
        replacePrefix++[0] = 0x66;
      }
      else {
        replacePrefixLen[0] = 5;
      }
      // Might be able to do with one less jump, but can pretend this is a single
      // long opcode followed by a 32-bit destination, so can just add original
      // destination afterwards, like in all other cases.

      // Jump over next jump.  (jcxz or jecxz)
      replacePrefix++[0] = 0xE3;
      replacePrefix++[0] = 0x02;
      // Jump over next jump.  (8-bit jump)
      replacePrefix++[0] = 0xEB;
      replacePrefix++[0] = 0x05;
      // Jump to where original moved jump would have taken us. (32-bit jump)
      replacePrefix[0] = 0xE9;
      *dest = op+2 + (char)op[1];
      return opSize + 2;
    default:
      return 0;
  }
}

int HookFunction(wchar_t *module, wchar_t *fxn, void *param, HookFxn *hookFxn) {
  //Stub();
  HMODULE hMod = GetModuleHandleW(module);
  if (!hMod) {
    return 0;
  }
  int len = wcslen(fxn);
  char *fxn2 = (char*) calloc(len+1, 1);
  for (int i=0; i<=len; i++) {
    fxn2[i] = (char)fxn[i];
  }
  void *addr = GetProcAddress(hMod, fxn2);
  free(fxn2);
  if (addr) {
    return HookRawAddress(addr, fxn, param, hookFxn);
  }
  return 0;
}

int HookAddress(void *vaddr, wchar_t *id, void *param, HookFxn *hookFxn, int type) {
  static DWORD pageSize = 0;
  static int stubLen = 0;
  unsigned char* stubBase = (unsigned char*)PreCallStub;
  if (HOOK_AFTER == type) {
    stubBase = (unsigned char*)PostStdCallStub;
  }

  if (!pageSize)
  {
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    pageSize = info.dwPageSize;
  }
  {
    unsigned char *test = stubBase;
#ifdef _DEBUG
    // In debug mode, with incremental linking, with MSVC
    // Stub points to a jump table rather than an actual function.
    // This resolves the address of what it points to, allowing
    // debug build to work, even after modifying code while running.
    int *jadder = (int*) (test+1);
    stubBase = test = *jadder + (unsigned char*)(jadder+1);
#endif
    int count = 0;
    while (count < 2) {
      if (*(int *)test == JUNK) {
        count++;
        test += 4;
        continue;
      }
      test++;
    }
    // Doesn't include return I'll have to add at the end.
    stubLen = test - stubBase - 5;
  }

  int len = 0;
  unsigned char *addr = (unsigned char*) vaddr;

  int readLen = 0;
  // Need 5 bytes for the jump.
  while (readLen < 5) {
    int delta = disasm((BYTE*) addr + readLen);
    if (!delta) return 0;
    unsigned char *dest;
    unsigned char prefix[10];
    int prefixLen;
    if (IsRelativeJump(addr+readLen, &dest, prefix, &prefixLen)) {
      len += prefixLen + 4;
    }
    else {
      len += delta;
    }
    readLen += delta;
  }

  unsigned int memNeeded = (len + 5 + stubLen + 63) & ~63;
  int mem = 0;
  while (mem < numHookMem) {
    if (hookMem->used + memNeeded < pageSize) break;
    mem++;
  }
  if (mem == numHookMem) {
    hookMem = (HookMem*) realloc(hookMem, sizeof(HookMem) * (numHookMem+1));
    memset(hookMem + numHookMem, 0, sizeof(HookMem));
    hookMem->base = (unsigned char*) VirtualAlloc(0, pageSize, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!hookMem->base) return 0;
    numHookMem++;
  }
  unsigned char *base = hookMem[mem].base + hookMem[mem].used;

  if (numHooks % 16 == 0) {
    hooks = (HookInfo*) realloc(hooks, sizeof(HookInfo) * (numHooks + 16));
  }
  HookInfo *hook = hooks + numHooks;
  memset(hook, 0, sizeof(HookInfo));

  int pos = 0;
  int count = 0;
  DWORD old = 0, old2;
  if (!VirtualProtect(addr, readLen, PAGE_EXECUTE_READWRITE, &old) || !VirtualProtect(base, memNeeded, PAGE_EXECUTE_READWRITE, &old2)) {
    return 0;
  }
  for (int i=0; i<stubLen; ) {
    if (*(int*)(stubBase + i) == JUNK) {
      count++;
      if (count == 1) {
        if (HOOK_BEFORE == type) {
          *(int*)&base[pos] = numHooks;
        }
        else if (HOOK_AFTER == type) {
          // Relative jump to function.  Stack should look just like
          // it did initially, except with the return value on top.
          base[pos-1] = 0xE9;
          *(int*)&base[pos] = ((ULONG_PTR)hookFxn) - ((ULONG_PTR)base+pos+4);
        }
        pos += 4;
        i += 4;
        continue;
      }
    }
#ifdef _DEBUG
    // Remote stack corruption check in debug build.
    // Just removing because calls use relative addresses.
    if (stubBase[i] == 0x3B && stubBase[i+1] == 0xF4 && stubBase[i+2] == 0xE8) {
      i += 7;
      continue;
    }
#endif
    base[pos++] = stubBase[i++];
  }

  readLen = 0;
  while (readLen < 5) {
    int delta = disasm((BYTE*) addr + readLen);
    if (!delta) return 0;
    unsigned char *dest;
    unsigned char prefix[10];
    int prefixLen;
    // Modify relative jumps as needed.
    if (IsRelativeJump(addr+readLen, &dest, prefix, &prefixLen)) {
      memcpy(base+pos, prefix, prefixLen);
      pos += prefixLen;
      *(int*)(base+pos) = dest - (base+pos+4);
      pos += 4;
    }
    else {
      memcpy(base+pos, addr+readLen, delta);
      pos += delta;
    }
    readLen += delta;
  }

  base[pos++] = 0xE9;
  *(int*)&base[pos] = (int)(addr+readLen) - (4+(int)&base[pos]);

  *addr = 0xE9;
  *(int*) (addr+1) = (int)base - (int)(addr + 5);

  VirtualProtect(addr, readLen, old, &old);
  VirtualProtect(base, memNeeded, PAGE_EXECUTE, &old);
  HANDLE hProcess = GetCurrentProcess();
  FlushInstructionCache(hProcess, addr, len);
  FlushInstructionCache(hProcess, base, memNeeded);


  hook->fxn = hookFxn;
  hook->param = param;
  hook->id = wcsdup(id);
  hookMem[mem].used += memNeeded;
  numHooks++;
  /*
  {
    wchar_t temp[10];
    int test = *(int*)addr;
    //if (VirtualProtect((void*)baseAddr, 
    void *q = MultiByteToWideChar;
    MultiByteToWideChar(932, 0, "Goat", -1, temp, 10);
    printf("test");
    addr=addr;
  }
  //*/
  return 1;
}

int HookAfterFunction(void *addr, wchar_t *id, void *param, void *hookFxn) {
  return HookAddress(addr, id, param, (HookFxn*) hookFxn, HOOK_AFTER);
}

int HookRawAddress(void *addr, wchar_t *id, void *param, HookFxn *hookFxn) {
  return HookAddress(addr, id, param, hookFxn, HOOK_BEFORE);
}

SuspendedThreads *SuspendThreads() {
  HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
  if (hSnap == INVALID_HANDLE_VALUE) return 0;
  int numThreads = 0;
  HANDLE hThread = GetCurrentThread();
  SuspendedThreads *out = (SuspendedThreads *)calloc(1, sizeof(SuspendedThreads));

  THREADENTRY32 entry;
  entry.dwSize = sizeof(entry);
  int pid = GetCurrentProcessId();
  int tid = GetCurrentThreadId();
  if (Thread32First(hSnap, &entry)) {
    do {
      if (entry.dwSize >= 4 * sizeof(DWORD) && entry.th32OwnerProcessID == pid && entry.th32ThreadID != tid) {
        HANDLE hThread = OpenThread(THREAD_SUSPEND_RESUME, 0, entry.th32ThreadID);
        if (hThread && SuspendThread(hThread) != DWORD(-1)) {
          if (out->numThreads % 16 == 0)
            out = (SuspendedThreads *)realloc(out, sizeof(SuspendedThreads) + sizeof(HANDLE) * (out->numThreads+16));
          out->threads[out->numThreads++] = hThread;
        }
      }
      entry.dwSize = sizeof(entry);
    }
    while (Thread32Next(hSnap, &entry));
  }
  CloseHandle(hSnap);
  return out;
}

void ResumeThreads(SuspendedThreads *threads) {
  for (int i=0; i<threads->numThreads; i++) {
    ResumeThread(threads->threads[i]);
  }
  free(threads);
}
