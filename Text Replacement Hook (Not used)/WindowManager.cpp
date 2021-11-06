#include <Shared/Shrink.h>
#include "WindowManager.h"
#include <Shared/Atlas.h>
#include <time.h>
WindowInfo *windows = 0;
int numWindows = 0;

static CRITICAL_SECTION drawingCS;

inline void LockData() {
	EnterCriticalSection(&drawingCS);
}

inline void UnlockData() {
	LeaveCriticalSection(&drawingCS);
}

WindowInfo *GetWindowInfo(HWND hWnd) {
	for (int i=0; i<numWindows; i++) {
		if (windows[i].hWnd == hWnd) return &windows[i];
	}
	return 0;
}

WindowInfo *GetWindowInfo(HDC hDC) {
	for (int i=0; i<numWindows; i++) {
		if (windows[i].hMemDCTheirs == hDC) return &windows[i];
	}
	return 0;
}

static inline void CleanupDC(HDC &hMemDC, HBITMAP &hMemBmp, HGDIOBJ &hOldMemBmp) {
	if (hMemDC) {
		if (hOldMemBmp)
			SelectObject(hMemDC, hOldMemBmp);
		DeleteDC(hMemDC);
		hMemDC = 0;
		hOldMemBmp = 0;
	}
	if (hMemBmp) {
		DeleteObject(hMemBmp);
		hMemBmp = 0;
	}
}

void CleanupWindowInfoMemory(WindowInfo *info) {
	CleanupDC(info->hMemDCTheirs, info->hMemBmpTheirs, info->hOldMemBmpTheirs);
	CleanupDC(info->hMemDCMine, info->hMemBmpMine, info->hOldMemBmpMine);
}

void CleanupWindowStrings(WindowInfo *info) {
	for (int i=0; i<info->numStrings; i++) {
		free(info->strings[i].string);
	}
	free(info->strings);
}

void ClearWindowInfo(WindowInfo *info) {
	LockData();
	CleanupWindowInfoMemory(info);
	// if (info->buffer) free(info->buffer);
	if (info->hDCActive) ReleaseDC(info->hWnd, info->hDCActive);
	CleanupWindowStrings(info);
	// Don't have a paint struct, so can't release the other DC if I have it.
	// Should only call this for an active window on quit;
	*info = windows[--numWindows];
	UnlockData();
	if (!numWindows)
		DeleteCriticalSection(&drawingCS);
}

void InitWindowInfoMemory(WindowInfo *info, HDC hDC) {
	BITMAPINFO header;
	header.bmiHeader.biSize = sizeof(header.bmiHeader);
	header.bmiHeader.biWidth = info->w;
	header.bmiHeader.biHeight = -info->h;
	header.bmiHeader.biPlanes = 1;
	header.bmiHeader.biBitCount = 32;
	header.bmiHeader.biCompression = BI_RGB;
	header.bmiHeader.biSizeImage = 0;
	header.bmiHeader.biClrImportant = 0;
	header.bmiHeader.biClrUsed = 0;

	// If resizing, keep the old hDCs.  Absolutely no clue if it works.
	if (info->hMemDCTheirs) {
		HBITMAP newBmpTheirs = CreateDIBSection(hDC, &header, DIB_RGB_COLORS, (void**)&info->bmpBitsTheirs, 0, 0);
		HBITMAP newBmpMine = CreateDIBSection(hDC, &header, DIB_RGB_COLORS, (void**)&info->bmpBitsMine, 0, 0);
		// Loop is just for multi-threading issues...  Currently not needed.
		while (1) {
			if (newBmpTheirs) {
				HGDIOBJ old = SelectObject(info->hMemDCTheirs, newBmpTheirs);
				if (old) {
					DeleteObject(info->hMemBmpTheirs);
					info->hMemBmpTheirs = newBmpTheirs;
					newBmpTheirs = 0;
				}
				else Sleep(10);
			}
			else if (newBmpMine) {
				HGDIOBJ old = SelectObject(info->hMemDCMine, newBmpMine);
				if (old) {
					DeleteObject(info->hMemBmpMine);
					info->hMemBmpMine = newBmpMine;
					newBmpMine = 0;
				}
				else Sleep(10);
			}
			else break;
		}
	}
	else {
		info->hMemDCTheirs = CreateCompatibleDC(hDC); 
		info->hMemBmpTheirs = CreateDIBSection(hDC, &header, DIB_RGB_COLORS, (void**)&info->bmpBitsTheirs, 0, 0);
		info->hOldMemBmpTheirs = SelectObject(info->hMemDCTheirs, info->hMemBmpTheirs);

		info->hMemDCMine = CreateCompatibleDC(hDC); 
		info->hMemBmpMine = CreateDIBSection(hDC, &header, DIB_RGB_COLORS, (void**)&info->bmpBitsMine, 0, 0);
		info->hOldMemBmpMine = SelectObject(info->hMemDCMine, info->hMemBmpMine);
	}
	BitBlt(info->hMemDCTheirs, 0, 0, info->w, info->h, hDC, 0, 0, SRCCOPY);
}

HDC InitWindowInfoDC(HWND hWnd, PAINTSTRUCT *paint) {
	int w = 0;
	int h = 0;
	if (hWnd) {
		RECT r;
		GetClientRect(hWnd, &r);
		w = r.right;
		h = r.bottom;
	}
	if (!w || !h) {
		if (!paint)
			return GetDC(hWnd);
		else
			return BeginPaint(hWnd, paint);
	}
	WindowInfo *info = GetWindowInfo(hWnd);
	if (!info) {
		windows = (WindowInfo*) realloc(windows, sizeof(WindowInfo) * (numWindows+1));
		if (!numWindows) {
			InitializeCriticalSection(&drawingCS);
		}
		info = &windows[numWindows++];
		memset(info, 0, sizeof(WindowInfo));
		info->hWnd = hWnd;
	}
	if (!paint) {
		if (!info->hDCActive)
			if (!(info->hDCActive = GetDC(hWnd))) return 0;
		info->copies ++;
	}
	else {
		if (info->paintCopies) return BeginPaint(hWnd, paint);
		if (!info->hDCActivePaint)
			if (!(info->hDCActivePaint = BeginPaint(hWnd, paint))) return 0;
		info->paintCopies++;
	}
	if (!info->hMemDCMine || info->w != w || info->h != h) {
		info->w = w;
		info->h = h;
		HDC hDC = info->hDCActive;
		if (!hDC) hDC = info->hDCActivePaint;
		InitWindowInfoMemory(info, hDC);
	}
	// info->timeStolen = time(0);
	return info->hMemDCTheirs;
}

int UpdateText(WindowInfo *info, unsigned int *buffer) {
	if (!AtlasIsLoaded()) return 0;
	unsigned int *data = buffer;
	GdiFlush();

	LockData();
	info = GetWindowInfo(info->hWnd);
	if (!info) {
		UnlockData();
		return 0;
	}
	int w = info->w;
	int h = info->h;
	if (!buffer) {
		data = (unsigned int*) malloc(w * h * 4);
	}
	memcpy(data, info->bmpBitsTheirs, info->w * info->h * 4);
	UnlockData();

	int numStrings;
	StringInfo* strings = LocateStrings(data, w, h, numStrings);
	if (strings) {
		TransStrings(strings, numStrings);
	}

	LockData();
	info = GetWindowInfo(info->hWnd);
	if (!info) {
		UnlockData();
		for (int i=0; i<numStrings; i++) {
			free(strings[i].string);
		}
		free(strings);
		return 0;
	}
	CleanupWindowStrings(info);
	info->strings = strings;
	info->numStrings = numStrings;
	UnlockData();
	if (!buffer) free(data);
	return 1;
}

int ReleaseWindowInfoDC(HWND hWnd, const PAINTSTRUCT *paint, HDC hDC) {
	WindowInfo *info = GetWindowInfo(hWnd);
	if (info) {
		if ((hDC && hDC == info->hMemDCTheirs) || (paint && info->paintCopies)) {
			GdiFlush();
			memcpy(info->bmpBitsMine, info->bmpBitsTheirs, 4 * info->w * info->h);
			int res = UpdateText(info, info->bmpBitsMine);
			if (info->strings) {
				DisplayStrings(info->hMemDCMine, info->strings, info->numStrings, info->w, info->h);
			}
			if (paint) {
				BitBlt(info->hDCActivePaint, 0, 0, info->w, info->h, info->hMemDCMine, 0, 0, SRCCOPY);
				info->paintCopies = 0;
				info->hDCActivePaint = 0;
				return EndPaint(hWnd, paint);
			}
			else {
				BitBlt(info->hDCActive, 0, 0, info->w, info->h, info->hMemDCMine, 0, 0, SRCCOPY);
				if (0 == --info->copies) {
					HDC hDC = info->hDCActive;
					info->hDCActive = 0;
					return ReleaseDC(hWnd, hDC);
				}
				return 1;
			}
		}
	}
	if (paint) {
		return EndPaint(hWnd, paint);
	}
	else {
		return ReleaseDC(hWnd, hDC);
	}
}

void ClearAllWindowInfo() {
	while(numWindows) ClearWindowInfo(windows);
	free(windows);
	windows = 0;
	numWindows = 0;
}

HDC __stdcall MyGetDC(HWND hWnd) {
	//while(1);
	return InitWindowInfoDC(hWnd, 0);
}

HDC __stdcall MyBeginPaint(HWND hWnd, LPPAINTSTRUCT lpPaint) {
	return InitWindowInfoDC(hWnd, lpPaint);
}


BOOL __stdcall MyEndPaint(HWND hWnd, CONST PAINTSTRUCT *lpPaint) {
	return (BOOL)ReleaseWindowInfoDC(hWnd, lpPaint, 0);
}

int __stdcall MyReleaseDC(HWND hWnd, HDC hDC) {
	return (BOOL)ReleaseWindowInfoDC(hWnd, 0, hDC);
}
