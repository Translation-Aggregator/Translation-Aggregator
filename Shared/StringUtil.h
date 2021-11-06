#pragma once

#define BREAK_ON_ELLIPSES                   0x001
#define BREAK_ON_COMMAS                     0x002
#define BREAK_ON_QUOTES                     0x004
#define BREAK_ON_DOUBLE_QUOTES              0x008
#define BREAK_ON_PARENTHESES                0x010
#define BREAK_ON_SQUARE_BRACKETS            0x020
#define BREAK_ON_SINGLE_LINE_BREAKS         0x040

int IsOpenBracket(wchar_t c);
int IsCloseBracket(wchar_t c);

wchar_t *GetSubstring(wchar_t *string, wchar_t *prefix, wchar_t* suffix, int *len);

int IsKatakana(wchar_t c);
int IsHiragana(wchar_t c);
int IsHalfWidthKatakana(wchar_t c);
int IsKanji(wchar_t c);

int IsJap(wchar_t c);
int HasJap(const wchar_t *s);

// ntdll.dll's iswspace seems to be broken.
#define MyIsWspace(x) (iswspace(x) || x == L'　')

size_t EUCJPtoUTF16(const char *s, size_t len, wchar_t *out);

// len includes terminating null.  If len is <= 0, uses strlen.
wchar_t *ToWideChar(const char *s, int cp, int &len);
char *ToMultiByte(const wchar_t *s, int cp, int &len);

#ifndef NO_STD
std::wstring ToWstring(const std::string& string, int code_page = CP_UTF8);
std::string ToMultiByteString(const std::wstring& wstring, int code_page = CP_UTF8);
#endif

//inline wchar_t *StringToUnicode(char *s, int &len) {
//  return ToWideChar(s, CP_ACP, len);
//}

// Removes carriage returns (Replacing with LFs when not followed by LFs),
// leading whitespace, etc.
void SpiffyUp(wchar_t *s);
int IsMajorPunctuation(wchar_t c, DWORD flags);
int IsPunctuation(wchar_t c);
wchar_t* UnescapeQuotes(wchar_t* data);
wchar_t* ParseJSON(wchar_t *data, const wchar_t *start, const wchar_t *next, bool nolinebreak = false);

// Raw = 1 will return size in bytes, not wchar's.
void LoadFile(const wchar_t *path, wchar_t ** data, int *size, int raw = 0);
#ifndef NO_STD
bool LoadFile(const std::wstring& path, std::wstring* out);
#endif

int ChunkToRomaji(wchar_t *in, wchar_t *out, int *eaten);
int ChunkToKatakana(wchar_t *in, wchar_t *out, int *eaten);

int ToKatakana(wchar_t *in, wchar_t *out);
int ToHiragana(wchar_t *in, wchar_t *out);
int ToRomaji(wchar_t *in, wchar_t *out);

struct SharedString
{
	int refs;
	int length;
	wchar_t string[1];

	inline void AddRef()
	{
		refs++;
	}
	inline void Release()
	{
		if (!--refs)
			free(this);
	}
};
SharedString *GetSourceText();

// length does not include terminating null.
inline SharedString *CreateSharedString(int length)
{
	SharedString *out = (SharedString*) malloc(sizeof(wchar_t)*length + sizeof(SharedString));
	out->refs = 1;
	out->length = length;
	out->string[length] = 0;
	return out;
}

// Must have only one reference, or will cause...issues.
inline void ResizeSharedString(SharedString *&string, int length)
{
	string = (SharedString*) realloc(string, sizeof(wchar_t)*length + sizeof(SharedString));
	string->length = length;
	string->string[length] = 0;
}

inline SharedString *CreateSharedString(const wchar_t *string, int length = -1)
{
	if (length < 0) length = wcslen(string);
	SharedString *out = CreateSharedString(length);
	memcpy(out->string, string, length * sizeof(wchar_t));
	return out;
}
