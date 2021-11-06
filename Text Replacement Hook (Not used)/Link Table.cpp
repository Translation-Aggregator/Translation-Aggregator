// From dll root file.
HDC activeDC = 0;
RECT dcRect;

HDC hMemDCTheirs = 0;
HBITMAP hMemBmpTheirs = 0;
HGDIOBJ hOldMemBmpTheirs = 0;

HDC hMemDCMine = 0;
HBITMAP hMemBmpMine = 0;
HGDIOBJ hOldMemBmpMine = 0;
unsigned int *myBmpBits = 0;

int drawing = 0;

HWND hWndActive = 0;

UINT_PTR timer = 0;



inline void CleanupDC(HDC &hMemDC, HBITMAP &hMemBmp, HGDIOBJ &hOldMemBmp) {
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

void CleanupMemDC() {
	CleanupDC(hMemDCTheirs, hMemBmpTheirs, hOldMemBmpTheirs);
	CleanupDC(hMemDCMine, hMemBmpMine, hOldMemBmpMine);
	dcRect.right = 0;
	dcRect.bottom = 0;
	if (timer) KillTimer(hWndActive, timer);
	timer = 0;
	hWndActive = 0;
	drawing = 0;
}

int drawStart = 0;
int drawingTimer = 0;


int MyFlush();
void CheckDrawing(int t);

void CALLBACK TimerProc(HWND hWnd, UINT id, UINT_PTR dunno, DWORD dunno2) {
	CheckDrawing(GetTickCount());
}

void InitMemDC(HWND hWnd, HDC hDC, RECT &r);

void CheckDrawing(int t) {
	// Has severe threading issues.
	if (drawing) {
		int dt = (int)(t - drawStart);
		if (dt > drawingTimer) {
			if (MyFlush()) {
				drawingTimer = 50;
			}
			drawStart = t;
			timer = SetTimer(hWndActive, -9345343, 100, TimerProc);
			//ReleaseDC(hWndActive, activeDC);
			//RECT r;
			//GetClientRect(hWndActive, &r);
			//InitMemDC(hWndActive, GetDC(hWndActive), r);
		}
	}
}


// Doesn't check if really need to do anything.
void InitMemDC(HWND hWnd, HDC hDC, RECT &r) {
	CleanupMemDC();
	drawingTimer = 0;
	hWndActive = hWnd;
	dcRect = r;
	activeDC = hDC;

	hMemDCTheirs = CreateCompatibleDC(activeDC);
	hMemBmpTheirs = CreateCompatibleBitmap(activeDC, dcRect.right, dcRect.bottom);
	hOldMemBmpTheirs = SelectObject(hMemDCTheirs, hMemBmpTheirs);
	BitBlt(hMemDCTheirs, 0, 0, dcRect.right, dcRect.bottom, activeDC, 0, 0, SRCCOPY);

	hMemDCMine = CreateCompatibleDC(activeDC);
	BITMAPINFO info;
	//BitBlt(hMemDC, 0, 0, dcRect.right, dcRect.bottom, hDC, 0, 0, SRCCOPY);
	info.bmiHeader.biSize = sizeof(info.bmiHeader);
	info.bmiHeader.biWidth = dcRect.right;
	info.bmiHeader.biHeight = -dcRect.bottom;
	info.bmiHeader.biPlanes = 1;
	info.bmiHeader.biBitCount = 32;
	info.bmiHeader.biCompression = BI_RGB;
	info.bmiHeader.biSizeImage = dcRect.right * dcRect.bottom * 4;
	info.bmiHeader.biClrImportant = 0;
	info.bmiHeader.biClrUsed = 0;
	hMemBmpMine = CreateDIBSection(activeDC, &info, DIB_RGB_COLORS, (void**)&myBmpBits, 0, 0);//CreateCompatibleBitmap(activeDC, dcRect.right, dcRect.bottom);
	if (hMemBmpMine) {
		hOldMemBmpMine = SelectObject(hMemDCMine, hMemBmpMine);
	}
	else myBmpBits = 0;
}

HDC InitDC(HWND hWnd, HDC hDC) {
	if (!hWnd) return hDC;
	if (activeDC || drawing) return hDC;
	if (hWndActive != hWnd) {
		if (drawing) return hDC;
		CleanupMemDC();
	}
	RECT r;
	if (!GetClientRect(hWnd, &r)) return hDC;
	if (!r.right || !r.bottom) {
		CleanupMemDC();
		return hDC;
	}
	if (dcRect.right != r.right || dcRect.bottom != r.bottom || hWnd != hWndActive) {
		InitMemDC(hWnd, hDC, r);
	}
	else {
		activeDC = hDC;
	}
	if (drawingTimer != 4000) {
		drawingTimer = 4000;
		timer = SetTimer(hWnd, -9345343, 4000, TimerProc);
	}
	drawStart = GetTickCount();
	drawing = 1;
	//if (bufferWidth !=
	return hMemDCTheirs;
}
/*
HDC __stdcall MyGetDC(HWND hWnd) {
	return InitDC(hWnd, GetDC(hWnd));
}

HDC __stdcall MyBeginPaint(HWND hWnd, LPPAINTSTRUCT lpPaint) {
	return InitDC(hWnd, BeginPaint(hWnd, lpPaint));
}


BOOL __stdcall MyEndPaint(HWND hWnd, CONST PAINTSTRUCT *lpPaint) {
	if (drawing && hWnd == hWndActive) {
		drawing = 0;
		MyFlush();
		activeDC = 0;
	}
	return EndPaint(hWnd, lpPaint);
}

int __stdcall MyReleaseDC(HWND hWnd, HDC hDC) {
	if (drawing && hDC == hMemDCTheirs) {
		drawing = 0;
		MyFlush();
		activeDC = 0;
	}
	return ReleaseDC(hWnd, hDC);
}
//*/

/*
BOOL __stdcall MyBitBlt(HDC hdcDest, int nXDest, int nYDest, int nWidth, int nHeight, HDC hdcSrc, int nXSrc, int nYSrc, DWORD dwRop) {
	int res = BitBlt(hdcDest, nXDest, nYDest, nWidth, nHeight, hdcSrc, nXSrc, nYSrc, dwRop);
	//while(1);
	if (hdcDest == hMemDC) {
		hdcDest = hdcDest;
	}
	return res;
}

HBITMAP test = 0;
HBITMAP __stdcall MyCreateDIBSection(HDC hDC, CONST BITMAPINFO *pbmi, UINT iUsage, VOID **ppvBits, HANDLE hSection, DWORD dwOffset) {
	//while(1);
	HBITMAP res = CreateDIBSection(hDC, pbmi, iUsage, ppvBits, hSection, dwOffset);
	if (hDC == hMemDC) {
		test = res;
	}
	return res;
}

BOOL __stdcall MyDeleteObject(HGDIOBJ hObject) {
	BOOL res = DeleteObject(hObject);
	if (hObject == test) {
		test = 0;
	}
	return res;
}
//*/

int MyFlush() {
	if (!myBmpBits) return 0;
	GdiFlush();
	//BitBlt(hMemDC, 0, 0, dcRect.right, dcRect.bottom, hDC, 0, 0, SRCCOPY);
	/*
	BITMAPINFO info;
	info.bmiHeader.biSize = sizeof(info.bmiHeader);
	info.bmiHeader.biWidth = dcRect.right;
	info.bmiHeader.biHeight = -dcRect.bottom;
	info.bmiHeader.biPlanes = 1;
	info.bmiHeader.biBitCount = 32;
	info.bmiHeader.biCompression = BI_RGB;
	info.bmiHeader.biSizeImage = dcRect.right * dcRect.bottom * 4;
	info.bmiHeader.biClrImportant = 0;
	info.bmiHeader.biClrUsed = 0;
	unsigned int *buffer = (unsigned int*) malloc(dcRect.right * dcRect.bottom * sizeof(unsigned int));
	DWORD res = GetDIBits(hMemDCTheirs, hMemBmpTheirs, 0, dcRect.bottom, buffer, &info, DIB_RGB_COLORS);
	//*/
	int res = BitBlt(hMemDCMine, 0, 0, dcRect.right, dcRect.bottom, hMemDCTheirs, 0, 0, SRCCOPY);
	int numStrings = 0;
	if (!res) return 0;
	GdiFlush();
	if (AtlasIsLoaded()) {
		StringInfo* strings = LocateStrings(myBmpBits, dcRect.right, dcRect.bottom, numStrings);
		if (strings) {
			TransStrings(strings, numStrings);
			//BitBlt(hMemDCMine, 0, 0, dcRect.right, dcRect.bottom, hMemDCTheirs, 0, 0, SRCCOPY);
			DisplayStrings(hMemDCMine, strings, numStrings, dcRect.right, dcRect.bottom);
			for (int i=0; i<numStrings; i++) {
				free(strings[i].string);
			}
			free(strings);
		}
	}
	BitBlt(activeDC, 0, 0, dcRect.right, dcRect.bottom, hMemDCMine, 0, 0, SRCCOPY);
	return res;
	//if (res)
	//	res = SetDIBits(hMemDC, hMemBmp, 0, dcRect.bottom, buffer, &info, DIB_RGB_COLORS);
}



HDC __stdcall MyGetWindowDC(HWND hWnd) {
	//return activeDC = GetWindowDC(hWnd);
	return GetWindowDC(hWnd);
}


		{"TextOutA", "Gdi32.dll", MyTextOutA, TextOutA},
		{"TextOutW", "Gdi32.dll", MyTextOutW, TextOutW},

		{"ExtTextOutA", "Gdi32.dll", MyExtTextOutA, ExtTextOutA},
		{"ExtTextOutW", "Gdi32.dll", MyExtTextOutW, ExtTextOutW},

		{"DrawTextA", "User32.dll", MyDrawTextA, DrawTextA},
		{"DrawTextW", "User32.dll", MyDrawTextW, DrawTextW},
		{"DrawTextExA", "User32.dll", MyDrawTextExA, DrawTextExA},
		{"DrawTextExW", "User32.dll", MyDrawTextExW, DrawTextExW},

		{"GetTextExtentPoint32A", "Gdi32.dll", MyGetTextExtentPoint32A, GetTextExtentPoint32A},
		{"GetTextExtentPoint32W", "Gdi32.dll", MyGetTextExtentPoint32W, GetTextExtentPoint32W},

		{"GetGlyphOutlineA", "Gdi32.dll", MyGetGlyphOutlineA, GetGlyphOutlineA},
		{"GetGlyphOutlineW", "Gdi32.dll", MyGetGlyphOutlineW, GetGlyphOutlineW},

		{"GetWindowDC", "User32.dll", MyGetWindowDC, GetWindowDC},
		{"GetDC", "User32.dll", MyGetDC, GetDC},
		{"ReleaseDC", "User32.dll", MyReleaseDC, ReleaseDC},

		{"BeginPaint", "User32.dll", MyBeginPaint, BeginPaint},
		{"EndPaint", "User32.dll", MyEndPaint, EndPaint},

		//{"DirectDrawCreate", "ddraw.dll", MyDirectDrawCreate, DirectDrawCreate},
		//{"DirectDrawCreateEx", "ddraw.dll", MyDirectDrawCreateEx, DirectDrawCreateEx},
		//*/
	};
	*fxns = functs;
	// If not override in game text, only want a couple functions.
	if (!numFuncts) {
		numFuncts = sizeof(functs)/sizeof(functs[0]);
		if (!(settings.injectionFlags & REPLACEMENT_HOOK)) {
			for (int i=0; i<numFuncts; i++) {
				if (functs[i].oldFxn == TrackPopupMenuEx) {
					numFuncts = i+1;
					break;
				}
			}
		}//*/
	}





DWORD WINAPI ThreadProc(void *junk) {
	while(1) {
		if (drawingTimer < 1000 && drawing) Sleep(100);
		else Sleep(sleepTime);
	}
	return 0;
}


BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD fdwReason, void* lpvReserved) {
	if (fdwReason == DLL_PROCESS_ATTACH) {
	}
	else if (fdwReason == DLL_PROCESS_DETACH) {
		CleanupMemDC();
	}
	return 1;
}

