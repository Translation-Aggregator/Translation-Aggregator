#pragma once

struct CharInfo {
  CharInfo *next;
  CharInfo *prev;
  unsigned int color;
  wchar_t c;

  short x;
  short y;
  short w;
  short h;
  short mid;
  short fontH;
};

struct StringInfo {
  // Only string needs to be freed.  The occupy the same block of memory, trans
  // may well be string.
  wchar_t *string;
  wchar_t *trans;
  // temp data.
  CharInfo *chars;

  unsigned int color;
  short x;
  short y;
  short w;
  short h;
  short fontH;
  char multiline;
};
StringInfo* LocateStrings(unsigned int *bmp, int w, int h, int &numStrings);
void TransStrings(StringInfo*, int numStrings);
void DisplayStrings(HDC hDC, StringInfo* s, int numStrings, int w, int h);
void ClearCache();
void SetCacheSize(int size);
