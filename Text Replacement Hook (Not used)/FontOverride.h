// returns GetTextExtentPoint32W return value.
DWORD InternalGetExtentPoint32PlusW(HDC hDC, wchar_t *string, int numChars, SIZE *s, int *charWidth, int *spacerWidth, int *charHeight, int maxWidth);
BOOL InternalExtTextOut(HDC hDC,          // handle to DC
						int X,            // x-coordinate of reference point
						int Y,            // y-coordinate of reference point
						UINT fuOptions,   // text-output options
						CONST RECT* lprc, // optional dimensions
						wchar_t *lpString, // string
						UINT cbCount,     // number of characters in string
						CONST INT* lpDx,   // array of spacing values
						int width = 0 // optional width
						);

BOOL InternalGetTextExtentPoint32(
  HDC hDC,           // handle to DC
  wchar_t *lpString,  // text string
  int cbCount,      // characters in string
  LPSIZE lpSize,      // string size
  int maxWidth
);

BOOL __stdcall MyGetTextExtentPoint32A(
  HDC hDC,           // handle to DC
  char *lpString,  // text string
  int c,      // characters in string
  LPSIZE lpSize      // string size
);


BOOL __stdcall MyGetTextExtentPoint32W(
  HDC hDC,           // handle to DC
  wchar_t *lpString,  // text string
  int c,      // characters in string
  LPSIZE lpSize      // string size
);

BOOL __stdcall MyTextOutW(
  HDC hDC,           // handle to DC
  int X,             // x-coordinate of starting position
  int Y,             // y-coordinate of starting position
  wchar_t *lpString, // character string
  int cbString       // number of characters
);

BOOL __stdcall MyExtTextOutW(
  HDC hDC,          // handle to DC
  int X,            // x-coordinate of reference point
  int Y,            // y-coordinate of reference point
  UINT fuOptions,   // text-output options
  CONST RECT* lprc, // optional dimensions
  wchar_t *lpString, // string
  UINT cbString,     // number of characters in string
  CONST INT* lpDx   // array of spacing values
);


BOOL __stdcall MyTextOutA(
  HDC hDC,           // handle to DC
  int X,       // x-coordinate of starting position
  int Y,       // y-coordinate of starting position
  char *lpString,    // character string
  int cbCount        // number of characters
);

BOOL __stdcall MyExtTextOutA(
  HDC hDC,          // handle to DC
  int X,            // x-coordinate of reference point
  int Y,            // y-coordinate of reference point
  UINT fuOptions,   // text-output options
  CONST RECT* lprc, // optional dimensions
  char *lpString, // string
  UINT cbCount,     // number of characters in string
  CONST INT* lpDx   // array of spacing values
);

DWORD __stdcall MyGetGlyphOutlineW(
  HDC hDC,             // handle to DC
  UINT uChar,          // character to query
  UINT uFormat,        // data format
  LPGLYPHMETRICS lpgm, // glyph metrics
  DWORD cbBuffer,      // size of data buffer
  LPVOID lpvBuffer,    // data buffer
  CONST MAT2 *lpmat2   // transformation matrix
  );


DWORD __stdcall MyGetGlyphOutlineA(
  HDC hDC,             // handle to DC
  UINT uChar,          // character to query
  UINT uFormat,        // data format
  LPGLYPHMETRICS lpgm, // glyph metrics
  DWORD cbBuffer,      // size of data buffer
  LPVOID lpvBuffer,    // data buffer
  CONST MAT2 *lpmat2   // transformation matrix
  );

// Doesn't support most options.
int __stdcall MyDrawTextExW(HDC hDC, wchar_t *lpchText, int nCount, LPRECT lpRect, UINT uFormat, LPDRAWTEXTPARAMS lpDTParams);

int __stdcall MyDrawTextExA(HDC hDC, char *lpchText, int nCount, LPRECT lpRect, UINT uFormat, LPDRAWTEXTPARAMS lpDTParams) ;

int __stdcall MyDrawTextW(HDC hDC, wchar_t *lpchText, int nCount, LPRECT lpRect, UINT uFormat);

int __stdcall MyDrawTextA(HDC hDC, char *lpchText, int nCount, LPRECT lpRect, UINT uFormat);
