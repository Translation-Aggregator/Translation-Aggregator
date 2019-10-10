#pragma once
// Global file.  Takes care of memory allocation functions and includes
// Windows.h.

#ifndef UNICODE
#define UNICODE
#endif

#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0501
#define INITGUID
#include <Windows.h>
#include <WinSock2.h>
#include <stdio.h>
#ifndef NO_STD
#include <string>
#include <sstream>
#include <list>
#include <vector>
#include <map>
#endif

#include <commdlg.h>
#include <mmsystem.h>
#include <shellapi.h>

// Needed for typedefs used by http windows.
#include <Winhttp.h>

// Needed all over the place
#include <CommCtrl.h>

// Here because so many files use its messages.
#include <Richedit.h>

// Used by most of the external dll-dependent cpp files (Both Atlas ones, Mecab)
#include <shlobj.h>

#include <time.h>

#include <Psapi.h>

//#include <D2d1.h>
//#include <Dwrite.h>

// Simple way of ensuring a clean build before release.
#define APP_NAME L"Translation Aggregator"
#define APP_VERSION GIT_TAG L" (Unofficial)"

#ifdef SETSUMI_CHANGES
#define HTTP_REQUEST_ID L"Mozilla/4.0 (compatible; MSIE 8.0; Windows NT 5.1; Trident/4.0; .NET CLR 2.0.50727; .NET CLR 3.0.04506.648; .NET CLR 3.5.21022; .NET CLR 3.0.4506.2152; .NET CLR 3.5.30729; .NET CLR 1.1.4322; Creative AutoUpdate v1.40.01)"
#else
#define HTTP_REQUEST_ID L"TRAG/" APP_VERSION
#endif

#define MAIN_WINDOW_TITLE APP_NAME L" " APP_VERSION
#define MAIN_WINDOW_CLASS APP_NAME L" Main Window"
#define TRANSLATION_WINDOW_CLASS APP_NAME L" Translation Window"
#define DRAG_WINDOW_CLASS APP_NAME L" Drag Window"

// Makes different versions incompatible with each other's dlls,
// often unnecessarily, but saves me the effort of any other versioning.

extern HINSTANCE ghInst;
extern HWND hWndSuperMaster;

#define WMA_TRANSLATE_ALL            (WM_APP + 0x11)
#define WMA_TRANSLATE                (WM_APP + 0x12)
#define WMA_AUTO_TRANSLATE_CLIPBOARD (WM_APP + 0x13)
#define WMA_TRANSLATE_HIGHLIGHTED    (WM_APP + 0x14)
#define WMA_AUTO_COPY                (WM_APP + 0x15)
#define WMA_AUTO_TRANSLATE_CONTEXT   (WM_APP + 0x16)
#define WMA_THREAD_DONE              (WM_APP + 0x20)
#define WMA_DRAGSTART                (WM_APP + 0x30)
#define WMA_CLEANUP_REQUEST          (WM_APP + 0x40)
#define WMA_COMPLETE_REQUEST         (WM_APP + 0x41)
#define WMA_SOCKET_MESSAGE           (WM_APP + 0x50)
#define WMA_BACK                     (WM_APP + 0x60)
#define WMA_FORWARD                  (WM_APP + 0x61)
#define WMA_JPARSER_STATE            (WM_APP + 0x70)

// #ifndef _DEBUG
// 	#if (_MSC_VER<1300)
// 		#pragma comment(linker,"/RELEASE")
// 		#pragma comment(linker,"/opt:nowin98")
// 	#endif
// #endif

__forceinline unsigned short PASCAL FAR ntohs(unsigned short s)
{
	return (s<<8) + (s>>8);
}

__forceinline int ToCOutput(int v)
{
	if (v) return v-2;
	return 0;
}

__forceinline int __cdecl strnicmp(const char *s1, const char *s2, size_t len)
{
	return ToCOutput(CompareStringA(LOCALE_INVARIANT, NORM_IGNORECASE, s1, (int)len, s2, (int)len));
}

__forceinline int __cdecl stricmp(const char *s1, const char *s2)
{
	return ToCOutput(CompareStringA(LOCALE_INVARIANT, NORM_IGNORECASE, s1, -1, s2, -1));
}
#define wcscmp MyWcsCmp
__forceinline int __cdecl wcscmp(const wchar_t *s1, const wchar_t *s2)
{
	return ToCOutput(CompareStringW(LOCALE_INVARIANT, 0, s1, -1, s2, -1));
}

__forceinline int __cdecl wcsncmp(const wchar_t *s1, const wchar_t *s2, size_t len)
{
	return ToCOutput(CompareStringW(LOCALE_INVARIANT, 0, s1, (int)len, s2, (int)len));
}

__forceinline int __cdecl wcsnicmp(const wchar_t *s1, const wchar_t *s2, size_t len)
{
	return ToCOutput(CompareStringW(LOCALE_INVARIANT, NORM_IGNORECASE, s1, (int)len, s2, (int)len));
}

__forceinline int __cdecl wcsicmp(const wchar_t *s1, const wchar_t *s2)
{
	return ToCOutput(CompareStringW(LOCALE_INVARIANT, NORM_IGNORECASE, s1, -1, s2, -1));
}

__forceinline int __cdecl wcsijcmp_(const wchar_t *s1, const wchar_t *s2)
{
	while (*s1 || *s2)
	{
		// Have to do this character by character so comparing entire strings is consistent with comparing substrings.
		int w = ToCOutput(CompareStringW(LOCALE_INVARIANT, NORM_IGNOREKANATYPE | NORM_IGNORECASE, s1, 1, s2, 1));
		if (w) return w;
		// Fix for long Japanese dash matching every other funky Japanese symbol.
		// Dash is the only one common enough to be annoying.
		if (*s1 != *s2)
		{
			if (*s1 == 0x30fc)
				return -1;
			if (*s2 == 0x30fc)
				return 1;
		}
		s1++;
		s2++;
	}
	return 0;
	// Not consistent.  MS is the devil.
	// return ToCOutput(CompareStringW(LOCALE_USER_DEFAULT, NORM_IGNOREKANATYPE | NORM_IGNORECASE, s1, -1, s2, -1));
}

__forceinline int __cdecl wcsnijcmp_(const wchar_t *s1, const wchar_t *s2, size_t len)
{
	while ((*s1 || *s2) && len)
	{
		// Have to do this character by character so comparing entire strings is consistent with comparing substrings.
		int w = ToCOutput(CompareStringW(LOCALE_INVARIANT, NORM_IGNOREKANATYPE | NORM_IGNORECASE, s1, 1, s2, 1));
		if (w) return w;
		// Fix for long Japanese dash matching every other funky Japanese symbol.
		// Dash is the only one common enough to be annoying.
		if (*s1 != *s2)
		{
			if (*s1 == 0x30fc)
				return -1;
			if (*s2 == 0x30fc)
				return 1;
		}
		s1++;
		s2++;
		len--;
	}
	return 0;
	// Not consistent.  MS is the devil.
	// return ToCOutput(CompareStringW(LOCALE_INVARIANT, NORM_IGNOREKANATYPE | NORM_IGNORECASE, s1, (int)len, s2, (int)len));
}

__forceinline int unify(unsigned int x)
{
	// hiragana -> katakana
	if (x - 0x3041u < 0x3096u - 0x3041u)
		x += 0x30A1 - 0x3041;
	// fullwidth -> ascii
	else if (x - 0xFF01u < 0xFF20u - 0xFF01u) // use 0xFF5Eu instead of 0xFF20u to map everything, including letters too
		x += 0x0021 - 0xFF01;
	// lowercase english -> uppercase
	if (x - 0x0061u < 0x007Au - 0x0061u)
		x += 0x0041 - 0x0061;
	return x;
}

__forceinline int __cdecl wcsnijcmp(const wchar_t *s1, const wchar_t *s2, size_t len)
{
	while (len)
	{
		int a = unify(*s1++);
		int b = unify(*s2++);
		if (a - b || !b)
			return a - b;
		len--;
	}
	return 0;
}

__forceinline int __cdecl wcsijcmp(const wchar_t *s1, const wchar_t *s2)
{
	for (;;)
	{
		int a = unify(*s1++);
		int b = unify(*s2++);
		if (a - b || !b)
			return a - b;
	}
}

__forceinline wchar_t* __cdecl mywcstok(wchar_t *s1, const wchar_t *delim)
{
	static wchar_t *temp;
	if (s1) temp = s1;
	size_t len;
	while (1)
	{
		len = wcscspn(temp, delim);
		if (!len)
		{
			if (!*temp) return 0;
			temp++;
			continue;
		}
		wchar_t *out = temp;
		if (temp[len])
		{
			temp[len] = 0;
			temp += len+1;
		}
		else temp+=len;
		return out;
	}
}

/*
#define strnicmp(s1, s2, len) ToCOutput(CompareStringA(LOCALE_INVARIANT, NORM_IGNORECASE, s1, (int)len, s2, (int)len))

#define wcsnicmp(s1, s2, len) ToCOutput(CompareStringW(LOCALE_INVARIANT, NORM_IGNORECASE, s1, (int)len, s2, (int)len))

#define wcsicmp(s1, s2) ToCOutput(CompareStringW(LOCALE_INVARIANT, NORM_IGNORECASE, s1, -1, s2, -1))

//*/
/*
inline void WriteLogLine(char *s)
{
	HANDLE hFile = CreateFileA("Temp.txt", FILE_APPEND_DATA , 0, 0, OPEN_ALWAYS, 0, 0);
	DWORD d;
	WriteFile(hFile, s, strlen(s), &d, 0);
	WriteFile(hFile, "\n", 1, &d, 0);
	CloseHandle(hFile);
}
inline void WriteLogLineW(wchar_t *s)
{
	HANDLE hFile = CreateFileA("Temp.txt", FILE_APPEND_DATA , 0, 0, OPEN_ALWAYS, 0, 0);
	DWORD d;
	for (int i=0; s[i]; i++)
		WriteFile(hFile, s+i, 1, &d, 0);
	WriteFile(hFile, "\n", 1, &d, 0);
	CloseHandle(hFile);
}
//*/
