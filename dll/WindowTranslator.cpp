#include <Shared/Shrink.h>
#include "WindowTranslator.h"
#include <Shared/Atlas.h>

int ProcessMenu(HMENU hMenu, Buffer *buffer) {
  int count = GetMenuItemCount(hMenu);
  if (count <= 0) return 0;
  MENUITEMINFOW info;
  info.cbSize = sizeof(info);
  int changed = 0;
  for (int i=0; i<count; i++) {
    info.fMask = MIIM_SUBMENU | MIIM_TYPE;
    info.cch = buffer->size;
    info.dwTypeData = buffer->text;
    if (GetMenuItemInfoW(hMenu, i, 1, &info)) {
      if (info.cch) {
        if (HasJap(buffer->text)) {
          wchar_t *tab = wcschr(buffer->text, '\t');
          if (tab) *tab = 0;
          wchar_t *eng = TranslateFullLog(buffer->text);
          if (eng) {
            if (tab) {
              *tab = '\t';
              wcscat(eng, tab);
            }
            info.dwTypeData = eng;
            info.fMask = MIIM_TYPE;
            SetMenuItemInfoW(hMenu, i, 1, &info);
            free(eng);
            changed = 1;
          }
        }
      }
      if (info.hSubMenu) {
        changed |= ProcessMenu(info.hSubMenu, buffer);
      }
    }
  }
  return changed;
}

DWORD CALLBACK TranslateMenuThreadProc(void *menu) {
  if (!AtlasIsLoaded()) return 0;
  HMENU hMenu = (HMENU) menu;
  Buffer buffer = {0,0};
  buffer.size = 3000;
  buffer.text = (wchar_t*) malloc(sizeof(wchar_t) * 3000);
  ProcessMenu(hMenu, &buffer);
  free(buffer.text);
  return 0;
}

struct FitInfo {
  HWND hWnd;
  RECT r;
  RECT target;
};

BOOL CALLBACK EnumFitWndProc(HWND hWnd, FitInfo *fit) {
  if (fit->hWnd == hWnd) return 1;
  RECT r;
  if (!GetWindowRect(hWnd, &r)) return 1;
  // inside the other.
  if (fit->r.right  < r.right  && fit->r.left > r.left &&
    fit->r.bottom < r.bottom && fit->r.top  > r.top) {
      if (fit->target.right > r.right-4) {
        fit->target.right = r.right-4;
      }
      if (fit->target.bottom > r.bottom-4) {
        fit->target.bottom = r.bottom-4;
      }
      return 1;
  }
  if (r.left < fit->target.right && r.right > fit->target.left) {
    if (r.bottom > fit->target.top && r.top <= fit->target.bottom) {
      fit->target.bottom = r.top - 1;
      if (fit->target.bottom < fit->r.bottom) {
        fit->target.bottom = fit->r.bottom;
      }
    }
  }
  if (r.top < fit->target.bottom && r.bottom > fit->target.top) {
    if (r.right > fit->target.left && r.left <= fit->target.right) {
      fit->target.right = r.left - 1;
      if (fit->target.right < fit->r.right) {
        fit->target.right = fit->r.right;
      }
    }
  }
  return 1;
}


BOOL CALLBACK EnumThreadWndProc(HWND hWnd, Buffer *buffer) {
  char type[400];
  if (RealGetWindowClassA(hWnd, type, sizeof(type)/sizeof(char))) {
    if (!strcmp(type, "ComboBox") ||
      !strcmp(type, "ListBox") ||
      !strcmp(type, "SysListView32") ||
      !strcmp(type, "SysTreeView32") ||
      !strcmp(type, "ComboBoxEx32") ||
      !strcmp(type, "ListBox") ||
      !strcmp(type, "SysLink") ||          
      !strcmp(type, "Edit")) {
      return 1;
    }
    if (!strcmp(type, "SysTabControl32")) {
      int changed = 0;
      LRESULT count = SendMessage(hWnd, TCM_GETITEMCOUNT, 0, 0);
      TCITEM item;
      for (int i=0; i<count; i++) {
        item.mask = TCIF_TEXT;
        item.pszText = buffer->text;
        item.cchTextMax = buffer->size;
        if (!SendMessage(hWnd, TCM_GETITEM, i, (LPARAM)&item)) break;
        if (HasJap(buffer->text)) {
          wchar_t *eng = TranslateFullLog(buffer->text);
          if (eng) {
            item.mask = TCIF_TEXT;
            item.pszText = eng;
            SendMessage(hWnd, TCM_SETITEM,  i, (LPARAM)&item);
            changed = 1;
            free(eng);
            buffer->updated = 1;
          }
        }
      }
      if (changed) {
        Sleep(100);
        InvalidateRect(hWnd, 0, 1);
      }
    }
    else if (!strcmp(type, "SysListView32")) {
      int changed = 0;
      for (int i = 0; true; ++i) {
        LVCOLUMN column;
        column.mask = LVCF_TEXT;
        column.pszText = buffer->text;
        column.cchTextMax = buffer->size;
        if (TRUE != ListView_GetColumn(hWnd, i, &column))
          break;
        if (HasJap(buffer->text)) {
          wchar_t *eng = TranslateFullLog(buffer->text);
          if (eng) {
            column.mask = TCIF_TEXT;
            column.pszText = eng;
            ListView_SetColumn(hWnd, i, (LPARAM) &column);
            changed = 1;
            buffer->updated = 1;
            free(eng);
          }
        }
      }
      if (changed) {
        Sleep(100);
        InvalidateRect(hWnd, 0, 1);
      }
    }
  }

  int top = 0;
  if (buffer->updated < 0) {
    top = 1;
    buffer->updated = 0;
  }

  int len = GetWindowTextLengthW(hWnd);
  if (len > 0) {
    if (buffer->size < len+1) {
      buffer->size = len+150;
      buffer->text = (wchar_t*) realloc(buffer->text, sizeof(wchar_t)*buffer->size);
    }
    if (GetWindowTextW(hWnd, buffer->text, buffer->size)) {
      if (HasJap(buffer->text)) {
        wchar_t *eng = TranslateFullLog(buffer->text);
        if (eng) {
          SetWindowTextW(hWnd, eng);
          buffer->updated = 1;
          if (!strcmp(type, "Static") || !strcmp(type, "Button")) {
            HDC hDC = GetDC(hWnd);
            SIZE s;
            if (hDC) {
              int changed = 0;
              if (GetTextExtentPointW(hDC, eng, wcslen(eng), &s)) {
                RECT client, window;
                if (GetClientRect(hWnd, &client) && GetWindowRect(hWnd, &window)) {
                  int wantx = s.cx - client.right;
                  if (wantx < 0) wantx = 0;
                  int wanty = s.cy - client.bottom;
                  if (wanty < 0) wanty = 0;
                  if (wantx || wanty) {
                    HWND hParent = GetParent(hWnd);
                    if (hParent) {
                      FitInfo info;
                      info.hWnd = hWnd;
                      info.r = window;
                      info.target = window;
                      info.target.right += wantx;
                      info.target.bottom += wanty;
                      RECT pRect;
                      GetWindowRect(hParent, &pRect);
                      if (info.target.right >= pRect.right) {
                        info.target.right = pRect.right-5;
                        info.target.bottom = pRect.bottom;
                      }
                      EnumChildWindows(hParent, (WNDENUMPROC)EnumFitWndProc, (LPARAM)&info);
                      SetWindowPos(hWnd, 0, 0, 0, info.target.right-info.target.left, info.target.bottom-info.target.top, SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOOWNERZORDER | SWP_NOZORDER);
                    }
                  }
                }
              }
              ReleaseDC(hWnd, hDC);
            }
          }
          free(eng);
        }
      }
    }
  }
  HMENU hMenu = GetMenu(hWnd);
  if (hMenu) {
    if (ProcessMenu(hMenu, buffer)) {
      DrawMenuBar(hWnd);
    }
  }
  EnumChildWindows(hWnd, (WNDENUMPROC)EnumThreadWndProc, (LPARAM)buffer);
  if (top) {
    if (buffer->updated > 0)
      InvalidateRect(hWnd, 0, 1);
    buffer->updated = -1;
  }
  return 1;
}

int TranslateThreadWindows(int threadID, Buffer &buffer) {
  if (!AtlasIsLoaded()) return 0;
  buffer.updated = -1;
  return EnumThreadWindows(threadID, (WNDENUMPROC)EnumThreadWndProc, (LPARAM)&buffer);
}

BOOL WINAPI MyTrackPopupMenu(HMENU hMenu, UINT uFlags, int x, int y, int nReserved, HWND hWnd, CONST RECT * prcRect) {
  HANDLE hThread = CreateThread(0, 0, TranslateMenuThreadProc, hMenu, 0, 0);
  if (hThread) CloseHandle(hThread);
  return TrackPopupMenu(hMenu, uFlags, x, y, nReserved, hWnd, prcRect);
}

BOOL WINAPI MyTrackPopupMenuEx(HMENU hMenu, UINT uFlags, int x, int y, HWND hWnd, LPTPMPARAMS lptpm) {
  HANDLE hThread = CreateThread(0, 0, TranslateMenuThreadProc, hMenu, 0, 0);
  if (hThread) CloseHandle(hThread);
  return TrackPopupMenuEx(hMenu, uFlags, x, y, hWnd, lptpm);
}

int WINAPI MyMessageBoxA(HWND hWnd, char* lpText, char* lpCaption, UINT uType) {
	if (!lpText)
		lpText = "";
	if (!lpCaption)
		lpCaption = "";
	size_t lt = strlen(lpText) + 1;
	size_t lc = strlen(lpCaption + 1);
	wchar_t *w = (wchar_t*)malloc((lt + lc)*sizeof(wchar_t));
	MultiByteToWideChar(CP_ACP, 0, lpText, lt, w, lt);
	MultiByteToWideChar(CP_ACP, 0, lpCaption, lc, w + lt, lc);
	int res = MyMessageBoxW(hWnd, w, w + lt, uType);
	free(w);
	return res;
}

int WINAPI MyMessageBoxW(HWND hWnd, wchar_t* lpText, wchar_t* lpCaption, UINT uType) {
  int res = 0;
  if (AtlasIsLoaded()) {
    wchar_t *uni1 = TranslateFullLog(lpText);
    wchar_t *uni2 = TranslateFullLog(lpCaption);
    res = MessageBoxW(hWnd, uni1, uni2, uType);
    free(uni1);
    free(uni2);
  }
  else {
    res = MessageBoxW(hWnd, lpText, lpCaption, uType);
  }
  return res;
}

