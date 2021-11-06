#pragma once

// Needed for registers.
#include <Shared/HookEval.h>
typedef void HookFxn(Registers *regs, void *param, wchar_t *id);

// HookAfterFunction only works for stdcall functions.  Arguments to function are
// the output of the orignal function (Must be <= 32 bits) followed by the arguments
// to the original function.  Must return return value itself.
// Currently doesn't pass a parameter around, just takes param for future extension.
int HookAfterFunction(void *addr, wchar_t *id, void *param, void *hookFxn);

// hookFxn passed param and stack from the middle of a code point.
int HookRawAddress(void *addr, wchar_t *id, void *param, HookFxn *hookFxn);

int HookBeforeFunction(wchar_t *module, wchar_t *fxn, void *param, HookFxn *hookFxn);

struct SuspendedThreads {
  int numThreads;
  HANDLE threads[1];
};

SuspendedThreads *SuspendThreads();
void ResumeThreads(SuspendedThreads *threads);
