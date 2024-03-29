#include <Shared/Shrink.h>
#include "FontOverride.h"
#include <Shared/StringUtil.h>

/*

void UniText(int fxn, void *from, wchar_t *text, int len) {
	len = len;
}

void AsciiText(int fxn, void *from, char *text, int len) {
	int unilen = len;
	wchar_t *unitext = StringToUnicode(text, unilen);
	if (unitext) {
		UniText(fxn, from, unitext, unilen);
		free(unitext);
	}
}


void UniText(int fxn, void *from, wchar_t *text, int len, int x, int y) {
	len = len;
}

void AsciiText(int fxn, void *from, char *text, int len, int x, int y) {
	int unilen = len;
	wchar_t *unitext = StringToUnicode(text, unilen);
	if (unitext) {
		UniText(fxn, from, unitext, unilen, x, y);
		free(unitext);
	}
}
/*#define GET_CALL_FROM() \
	void *from = 0;		\
	__asm {				\
		mov from, esp	\
	}					\
	from = (void*)((int*)(((char*)from)+0xCC+4*4))[0];
	//*/



// returns GetTextExtentPoint32W return value.
DWORD InternalGetExtentPoint32PlusW(HDC hDC, wchar_t *string, int numChars, SIZE *s, int *charWidth, int *spacerWidth, int *charHeight, int maxWidth) {
	DWORD res = GetTextExtentPoint32W(hDC, L"の", 1, s);
	*charWidth = s->cx;
	*charHeight = s->cy;
	if (numChars  < 2) {
		if (numChars <= 0) {
			return GetTextExtentPoint32W(hDC, L"の", 0, s);
		}
		return res;
	}
	res = GetTextExtentPoint32W(hDC, L"のの", 2, s);
	if (s->cx <= 2 * *charWidth) {
		s->cx = 2 * *charWidth;
	}
	*spacerWidth = s->cx - 2 * *charWidth;

	s->cx = 0;
	int height = s->cy;
	int lineLen = 0;
	for (int i=0; i< numChars; i++) {
		if (string[i] != '\r' && string[i] != '\n') {
			if (lineLen) lineLen += *spacerWidth;
			lineLen += *charWidth;
			if (lineLen > maxWidth && lineLen != *charWidth) {
				lineLen = *charWidth;
				s->cy ++;
			}
			if (lineLen > s->cx) s->cx = lineLen;
			continue;
		}
		if (i+1 < numChars && string[i] == '\r' && string[i+1] == '\n')
			i++;
		s->cy += height;
		lineLen = 0;
	}
	//s->cx = *charWidth * numChars + (numChars - 1) * *spacerWidth;
	// s->cx = s->cx*3/4;
	//int q = GetTextAlign(hDC);
	return res;
}

BOOL InternalExtTextOut(HDC hDC,          // handle to DC
						int X,            // x-coordinate of reference point
						int Y,            // y-coordinate of reference point
						UINT fuOptions,   // text-output options
						CONST RECT* lprc, // optional dimensions
						wchar_t *lpString, // string
						UINT cbCount,     // number of characters in string
						CONST INT* lpDx,   // array of spacing values
						int width // optional width
						) {
	int charWidth, charWidth2, spacerWidth, charHeight;
	SIZE s;
	//return TextOutW(hDC, X, Y, lpString, cbCount);
	InternalGetExtentPoint32PlusW(hDC, L"　　", 1 + (cbCount != 0), &s, &charWidth2, &spacerWidth, &charHeight, 10000);
	int w = 0;
	//	return ExtTextOutW(hDC, X, Y, fuOptions, lprc, lpString, cbCount, lpDx);
	//return ExtTextOutW(hDC, X, Y, fuOptions, lprc, lpString, cbCount, lpDx);

	HBRUSH hBrush = CreateSolidBrush(GetTextColor(hDC));
	int prefix = charWidth2/12;
	int startX = X += prefix;
	for (UINT w=0; w<cbCount; w++) {
		ExtTextOutW(hDC, X-prefix, Y, fuOptions & (ETO_OPAQUE | ETO_CLIPPED), 0, L"　　", 2, 0);
		if (lpString[w] == '\r' || lpString[w] == '\n') {
			X = startX;
			Y += charHeight;
			if (lpString[w] == '\r' && w < cbCount-1 && lpString[w+1] == '\n') w++;
		}
		if (width>0 && X != startX && X + width-prefix > width) {
			X = startX;
			Y += charHeight;
			w--;
			continue;
		}
		//ExtTextOutW(hDC, X-prefix, Y, fuOptions & (ETO_OPAQUE | ETO_CLIPPED), 0, lpString+w, 1, 0);
		wchar_t c = lpString[w];
		charWidth = charWidth2;
		RECT r = {
			X,
			Y + charHeight * 0 / 6,
			X + charWidth * 5 / 6,
			Y + charHeight * 1 / 6
		};
		FillRect(hDC, &r, hBrush);
		RECT r2 = {
			X + charWidth * 2 / 6,
			Y + charHeight * 0 / 7,
			X + charWidth * 3 / 6,
			Y + charHeight * 6 / 7
		};
		FillRect(hDC, &r2, hBrush);
		//c = 0xFFFF;
		int index = -1;
		for (int i=0; i<20; i++) {
			if (i == 0) {
			}
			else if (i == 4) {
				continue;
			}
			else if (i == 15) {
			}
			else if (i == 11) {
				continue;
			}
			else {
				index++;
				if (!(c & (1<<index))) continue;
			}
			int x = i&3;
			int y = i/4+1;
			//if (y >= x+1) y++;
			if (x >= 2) x++;
			RECT r3 = {
				X + x * charWidth / 6,
				Y + y * charHeight / 7,
				X + (x+1) * charWidth / 6,
				Y + (y+1) * charHeight / 7,
			};
			FillRect(hDC, &r3, hBrush);
		}

		X += spacerWidth + charWidth2;
	}
	DeleteObject(hBrush);
	return 1;
}



BOOL InternalGetTextExtentPoint32(
  HDC hDC,           // handle to DC
  wchar_t *lpString,  // text string
  int cbCount,      // characters in string
  LPSIZE lpSize,      // string size
  int maxWidth
) {
	int charWidth, spacerWidth, charHeight;
	//return GetTextExtentPoint32W(hDC, lpString, cbCount, lpSize);
	return InternalGetExtentPoint32PlusW(hDC, lpString, cbCount, lpSize, &charWidth, &spacerWidth, &charHeight, maxWidth);
	/*
	if (0 && cbCount > 1) {
		wchar_t *temp = (wchar_t*) malloc((cbCount + 1) * sizeof(wchar_t));
		temp[cbCount] = 0;
		memcpy(temp, lpString, cbCount * sizeof(wchar_t));
		int res = 0;
		if (HasJap(temp)) {
			wchar_t *trans = AtlasTrans(temp);
			if (trans) {
				int len = wcslen(trans);
				res = GetTextExtentPoint32W(hDC, trans, len, lpSize);
				free(trans);
			}
		}
		free(temp);
		if (res) return res;
	}
	if (cbCount == 1) {
		return GetTextExtentPoint32W(hDC, L"　", cbCount, lpSize);
	}
	return GetTextExtentPoint32W(hDC, lpString, cbCount, lpSize);
	//*/
}

BOOL __stdcall MyGetTextExtentPoint32A(
  HDC hDC,           // handle to DC
  char *lpString,  // text string
  int c,      // characters in string
  LPSIZE lpSize      // string size
) {
	int len = c;
	wchar_t *unicode = StringToUnicode(lpString, len);
	int res = 0;
	if (unicode) {
		res = InternalGetTextExtentPoint32(hDC, unicode, len, lpSize, 100000);
		free(unicode);
	}
	return res;
}


BOOL __stdcall MyGetTextExtentPoint32W(
  HDC hDC,           // handle to DC
  wchar_t *lpString,  // text string
  int c,      // characters in string
  LPSIZE lpSize      // string size
) {
	return InternalGetTextExtentPoint32(hDC, lpString, c, lpSize, 100000);
}

BOOL __stdcall MyTextOutW(
  HDC hDC,           // handle to DC
  int X,             // x-coordinate of starting position
  int Y,             // y-coordinate of starting position
  wchar_t *lpString, // character string
  int cbString       // number of characters
) {
	return InternalExtTextOut(hDC, X, Y, 0, 0, lpString, cbString, 0);
}

BOOL __stdcall MyExtTextOutW(
  HDC hDC,          // handle to DC
  int X,            // x-coordinate of reference point
  int Y,            // y-coordinate of reference point
  UINT fuOptions,   // text-output options
  CONST RECT* lprc, // optional dimensions
  wchar_t *lpString, // string
  UINT cbString,     // number of characters in string
  CONST INT* lpDx   // array of spacing values
) {
	return InternalExtTextOut(hDC, X, Y, fuOptions, lprc, lpString, cbString, 0);
}


BOOL __stdcall MyTextOutA(
  HDC hDC,           // handle to DC
  int X,       // x-coordinate of starting position
  int Y,       // y-coordinate of starting position
  char *lpString,    // character string
  int cbCount        // number of characters
) {
	int len = cbCount;
	wchar_t *unicode = StringToUnicode(lpString, len);
	int res = 0;
	if (unicode) {
		// res = TextOutW(hDC, X, Y, unicode, len);
		res = InternalExtTextOut(hDC, X, Y, 0, 0, unicode, len, 0);
		free(unicode);
	}
	return res;
	//AsciiText(0, from, lpString, cbString, nXStart, nYStart);
	//return TextOutA(hDC, nXStart, nYStart, lpString, cbString);
}

BOOL __stdcall MyExtTextOutA(
  HDC hDC,          // handle to DC
  int X,            // x-coordinate of reference point
  int Y,            // y-coordinate of reference point
  UINT fuOptions,   // text-output options
  CONST RECT* lprc, // optional dimensions
  char *lpString, // string
  UINT cbCount,     // number of characters in string
  CONST INT* lpDx   // array of spacing values
) {
	int len = cbCount;
	wchar_t *unicode = StringToUnicode(lpString, len);
	int res = 0;
	if (unicode) {
		res = InternalExtTextOut(hDC, X, Y, fuOptions, lprc, unicode, len, 0);
		free(unicode);
	}
	return res;
}

DWORD __stdcall MyGetGlyphOutlineW(
  HDC hDC,             // handle to DC
  UINT uChar,          // character to query
  UINT uFormat,        // data format
  LPGLYPHMETRICS lpgm, // glyph metrics
  DWORD cbBuffer,      // size of data buffer
  LPVOID lpvBuffer,    // data buffer
  CONST MAT2 *lpmat2   // transformation matrix
  ) {
	if (uFormat != GGO_GRAY2_BITMAP &&
		uFormat != GGO_GRAY4_BITMAP &&
		uFormat != GGO_GRAY8_BITMAP &&
		uFormat != GGO_BITMAP &&
		uFormat != GGO_METRICS) {
			return GetGlyphOutlineW(hDC, uChar, uFormat, lpgm, cbBuffer, lpvBuffer, lpmat2);
	}
	wchar_t unicode = uChar;

	// Currently not strictly compatible if you use GGO_METRICS and then one of
	// the type I don't support.  Also currently completely ignore lpmat2.
	SIZE size;
	InternalGetTextExtentPoint32(hDC, &unicode, 1, &size, 10000);
	// return GetGlyphOutlineW(hDC, uChar, uFormat, lpgm, cbBuffer, lpvBuffer, lpmat2);
	/*if (uFormat == 0) {
		uFormat = uFormat;
	}
	//*/
	/*
	int ox = lpgm->gmBlackBoxX;
	int oy = lpgm->gmBlackBoxY;
	int ox2 = lpgm->gmCellIncX;
	//*/
	//size.cx = lpgm->gmCellIncX;

	//size.cx = (size.cx+3) & ~3;
	lpgm->gmBlackBoxX = size.cx;
	lpgm->gmBlackBoxY = size.cy;
	lpgm->gmCellIncX = (SHORT)size.cx;
	lpgm->gmCellIncY = 0;
	lpgm->gmptGlyphOrigin.x = 0;
	lpgm->gmptGlyphOrigin.y = size.cy-1 - size.cy/9;
	int width;
	if (uFormat == GGO_BITMAP) {
		width = 4*((size.cx+31) / 32);
	}
	else {
		width = (size.cx+3) & ~3;
	}

	if (!lpvBuffer || !cbBuffer) return width * size.cy;
	if (cbBuffer < (unsigned int)(width * size.cy)) {
		// if (uFormat == GGO_METRICS) return 0;
		if (uFormat == GGO_METRICS) return width * size.cy;
		return GDI_ERROR;
	}

	unsigned char max = 255;
	// Don't ask me why this isn't GGO_GRAY6_BITMAP
	if (uFormat == GGO_GRAY8_BITMAP) {
		MessageBoxA(0, "8", "8", MB_OK);
		max = 64;
	}
	else if (uFormat == GGO_GRAY4_BITMAP) {
		MessageBoxA(0, "4", "8", MB_OK);
		max = 16;
	}
	else if (uFormat == GGO_GRAY2_BITMAP) {
		MessageBoxA(0, "2", "8", MB_OK);
		max = 4;
	}
	unsigned char *bmp = (unsigned char*)lpvBuffer;
	//memset(bmp, 0, width * size.cy);

	BITMAPINFO info;
	info.bmiHeader.biSize = sizeof(info.bmiHeader);
	info.bmiHeader.biWidth = size.cx;
	info.bmiHeader.biHeight = -size.cy;
	info.bmiHeader.biPlanes = 1;
	info.bmiHeader.biBitCount = 32;
	info.bmiHeader.biCompression = BI_RGB;
	info.bmiHeader.biSizeImage = size.cx * size.cy * 4;
	info.bmiHeader.biClrImportant = 0;
	info.bmiHeader.biClrUsed = 0;

	HBITMAP hMemBmp = CreateCompatibleBitmap(hDC, size.cx, size.cy);
	HDC hMemDC = CreateCompatibleDC(hDC);
	HGDIOBJ hBmpOld = SelectObject(hMemDC, hMemBmp);
	HGDIOBJ hFont = GetCurrentObject(hDC, OBJ_FONT);

	SetTextColor(hMemDC, RGB(max, max, max));
	SetBkColor(hMemDC, 0);
	HBRUSH hBrush = CreateSolidBrush(0);

	hFont = SelectObject(hMemDC, hFont);
	RECT r = {0,0,size.cx, size.cy};
    FillRect(hMemDC, &r, hBrush);
	DeleteObject(hBrush);
	InternalExtTextOut(hMemDC, 0, 0, 0, &r, &unicode, 1, 0);

	GdiFlush();
	unsigned char *temp = (unsigned char*) malloc(r.right * 4 * size.cy);
	int res2 = GetDIBits(hMemDC, hMemBmp, 0, size.cy, temp, &info, DIB_RGB_COLORS);
	if (uFormat != GGO_BITMAP) {
		//int res2 = GetBitmapBits(hBMP, width*size.cy, bmp);
		for (int y=0; y<size.cy; y++) {
			int offset = y * 4 * r.right;
			int offset2 = y * width;
			//char goat[100];
			//MessageBoxA(0, goat, goat, MB_OK);
			for (int x=0; x<size.cx; x++) {
				//bmp[x + offset2] = 0;
				//if (!x) bmp[x + offset2] = max;
				//if (!x) bmp[x + offset2+90] = max;
				//if (y == 2 || y == 4) bmp[x + offset2] = max;
				//if (y == 0 || y == size.cy - 1 || y == 2 || y == 4) bmp[x + offset2] = max;
				//sprintf(goat, "%i\t%i\t%i\t%i", offset, size.cx, offset2, width);
				//if (!x) MessageBoxA(0, goat, "!x", MB_OK);
				//bmp[x + offset2] = max * (x == 0 || x==size.cx/2);
				//if (bmp[x + offset2]) MessageBoxA(0, goat, "bmp[x + offset2]", MB_OK);
				bmp[x + offset2] = temp[offset + x *4 + 1];
			}
		}
	}
	else {
		memset(bmp, 0, width * size.cy);
		for (int y=0; y<size.cy; y++) {
			int offset = y * 4 * r.right;
			int offset2 = y * width;

			for (int x=0; x<size.cx; x++) {
				bmp[x/8 + offset2] |= ((temp[offset + x *4 + 1]&0x80) >> (x%8));
			}
		}
	}
	free(temp);

	SelectObject(hMemDC, hFont);
	SelectObject(hMemDC, hBmpOld);
	DeleteDC(hMemDC);
	DeleteObject(hMemBmp);

	/*char temp2[100];
	sprintf(temp2, "%i, %i, %i\n%i, %i, %i", ox, oy, ox2, lpgm->gmBlackBoxX, lpgm->gmBlackBoxY, lpgm->gmCellIncX);
	MessageBoxA(0, temp2, temp2, MB_OK);
	//*/


	return width * size.cy;
	//return GetGlyphOutlineW(hDC, unicode, uFormat, lpgm, cbBuffer, lpvBuffer, lpmat2);
}


DWORD __stdcall MyGetGlyphOutlineA(
  HDC hDC,             // handle to DC
  UINT uChar,          // character to query
  UINT uFormat,        // data format
  LPGLYPHMETRICS lpgm, // glyph metrics
  DWORD cbBuffer,      // size of data buffer
  LPVOID lpvBuffer,    // data buffer
  CONST MAT2 *lpmat2   // transformation matrix
  ) {
	wchar_t unicode[10];
	char s[3];
	int bits = 1;
	if (uChar >= 80) {
		s[0] = uChar>>8;
		s[1] = uChar;
		bits++;
	}
	else {
		s[0] = uChar;
	}
	int len = MultiByteToWideChar(CP_ACP, 0, (char*)s, bits, unicode, 10);
	return MyGetGlyphOutlineW(hDC, unicode[0], uFormat, lpgm, cbBuffer, lpvBuffer, lpmat2);
}

// Doesn't support most options.
int __stdcall MyDrawTextExW(HDC hDC, wchar_t *lpchText, int nCount, LPRECT lpRect, UINT uFormat, LPDRAWTEXTPARAMS lpDTParams) {
	RECT r = *lpRect;
	SIZE s;
	int charWidth, spacerWidth, charHeight;
	if (uFormat & DT_SINGLELINE) {
		InternalGetExtentPoint32PlusW(hDC, lpchText, nCount, &s, &charWidth, &spacerWidth, &charHeight, 1000000);
		if (uFormat & DT_BOTTOM)
			r.top = r.bottom - s.cy;
		else {
			if (uFormat & DT_VCENTER)
				r.top = (r.top + r.bottom - s.cy)/2;
			r.bottom = r.top + s.cy;
		}
	}
	else {
		InternalGetExtentPoint32PlusW(hDC, lpchText, nCount, &s, &charWidth, &spacerWidth, &charHeight, lpRect->right-lpRect->left);
	}
	if (uFormat & DT_RIGHT)
		r.left = r.right - s.cx;
	else {
		if (uFormat & DT_CENTER)
			r.left = (r.left + r.right - s.cx)/2;
		r.right = r.left + s.cx;
	}
	*lpRect = r;
	if (!(uFormat & DT_CALCRECT)) {
		InternalExtTextOut(hDC, r.left, r.top, 0, 0, 0, nCount, 0, r.right-r.left);
	}
	return s.cy;
}

int __stdcall MyDrawTextExA(HDC hDC, char *lpchText, int nCount, LPRECT lpRect, UINT uFormat, LPDRAWTEXTPARAMS lpDTParams) {
	int len = nCount;
	wchar_t *unicode = StringToUnicode(lpchText, len);
	int res = 0;
	if (unicode) {
		res = MyDrawTextExW(hDC, unicode, len, lpRect, uFormat, lpDTParams);
		free(unicode);
	}
	return res;
}

int __stdcall MyDrawTextW(HDC hDC, wchar_t *lpchText, int nCount, LPRECT lpRect, UINT uFormat) {
	return MyDrawTextExW(hDC, lpchText, nCount, lpRect, uFormat, 0);
}

int __stdcall MyDrawTextA(HDC hDC, char *lpchText, int nCount, LPRECT lpRect, UINT uFormat) {
	return MyDrawTextExA(hDC, lpchText, nCount, lpRect, uFormat, 0);
}
