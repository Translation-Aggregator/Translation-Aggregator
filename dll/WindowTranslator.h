#pragma once

struct Buffer {
  wchar_t *text;
  int size;
  int updated;
};
int TranslateThreadWindows(int threadID, Buffer &buffer);

BOOL WINAPI MyTrackPopupMenu(HMENU hMenu, UINT uFlags, int x, int y, int nReserved, HWND hWnd, CONST RECT * prcRect);
BOOL WINAPI MyTrackPopupMenuEx(HMENU hMenu, UINT uFlags, int x, int y, HWND hWnd, LPTPMPARAMS lptpm);

int WINAPI MyMessageBoxA(HWND hWnd, char* lpText, char* lpCaption, UINT uType);
int WINAPI MyMessageBoxW(HWND hWnd, wchar_t* lpText, wchar_t* lpCaption, UINT uType);
