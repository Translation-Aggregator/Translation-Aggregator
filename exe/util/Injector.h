#pragma once

#include <string>

struct InjectionSettings;

std::wstring GetAGTHParams(const InjectionSettings *cfg);

int Inject(const InjectionSettings *cfg, DWORD pid, HWND hWnd);
int Inject(wchar_t *path);
