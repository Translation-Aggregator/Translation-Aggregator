#pragma once

#include "Config.h"

int AutoFilter(wchar_t *s, int len, const ContextSettings *settings);

#define DEFAULT_PHRASE_MIN 4
#define DEFAULT_PHRASE_MAX 100

// Here mostly for debugging.
int InfiniteRepeatFilter(wchar_t *s, int len);
int AutoConstantRepeatFilter(wchar_t *s, int len);
int AutoAdvancedRepeatFilter(wchar_t *s, int len);
int CustomRepeatFilter(wchar_t *s, int len, unsigned int *repeats, int numRepeats);
int RepeatExtensionFilter(wchar_t *s, int len);
int PhraseRepeatFilter(wchar_t *s, int len, const int minDist = DEFAULT_PHRASE_MIN, const int maxDist = DEFAULT_PHRASE_MAX);
int LineBreakRemoveSome(wchar_t *s, int len, unsigned int first, unsigned int last);
int LineBreakRemoveAll(wchar_t *s, int len);
