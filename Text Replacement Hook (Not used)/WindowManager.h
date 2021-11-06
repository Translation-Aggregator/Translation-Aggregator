#pragma once
#include "GetText.h"

// Keeps track of hDCs for each window.  Note that only one
// of these can be accessed at a time, so mutexes should be
// gotten before messing with any of this stuff.
struct WindowInfo {
	HWND hWnd;

	// Only use these when still have the DC.
	// If released, moved data over to the buffer.
	HDC hDCActive;
	// Used for begin/end paint.  Note that I used the
	// same memory BMP in both cases, and I also keep
	// it between calls.
	HDC hDCActivePaint;

	// Hack for games that create two copies of a DC.
	// Not sure if problems are caused by giving two copies of
	// one DC instead of two copies created by different calls
	// to GetDC.
	int copies;
	// currently forced to always be 1.  Otherwise, I don't return
	// my memory dc but rather the real one.
	int paintCopies;

	// If keep for over xxx ms, assume it's not going to be released.
	// time_t timeStolen;

	// Buffer these.
	HDC hMemDCTheirs;
	HBITMAP hMemBmpTheirs;
	HGDIOBJ hOldMemBmpTheirs;

	HDC hMemDCMine;
	HBITMAP hMemBmpMine;
	HGDIOBJ hOldMemBmpMine;

	unsigned int *bmpBitsMine;
	unsigned int *bmpBitsTheirs;

	// Size of bitmap.  Currently has window resizing issues.
	int w;
	int h;

	int numStrings;
	StringInfo* strings;
	// Only buffer in multi-threaded mode on close.
	// unsigned int *buffer;
};

// Creates window info if needed, calls the necessary HDC creation function if necessary.
// returns either their memory DC, on success, or the real DC on my failure, or 0 when
// windows fails.
HDC InitWindowInfoDC(HWND hWnd, PAINTSTRUCT *paint);
void ClearAllWindowInfo();
void ClearWindowInfo(WindowInfo *info);

HDC __stdcall MyGetDC(HWND hWnd);
HDC __stdcall MyBeginPaint(HWND hWnd, LPPAINTSTRUCT lpPaint);
BOOL __stdcall MyEndPaint(HWND hWnd, CONST PAINTSTRUCT *lpPaint);
int __stdcall MyReleaseDC(HWND hWnd, HDC hDC);