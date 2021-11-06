#pragma once

#include <Shared/Atlas.h>

#define AGTH_HOOK                        0x0002
#define TRANSLATE_MENUS                  0x0004
#define INJECT_PROCESS                   0x0008
#define NEW_PROCESS                      0x0010
#define DCBS_OVERRIDE                    0x0020
#define LOG_TRANSLATIONS                 0x0040
#define INTERNAL_HOOK                    0x0080
#define INTERNAL_NO_DEFAULT_FILTERS      0x0100
#define INTERNAL_NO_DEFAULT_HOOKS        0x0200
#define NO_INJECT_CHILDREN               0x0400
#define NO_INJECT_CHILDREN_SAME_SETTINGS 0x0800

#define FLAG_AGTH_COPYDATA         1
#define FLAG_AGTH_SUPPRESS_SYMBOLS 2
#define FLAG_AGTH_SUPPRESS_PHRASES 4
#define FLAG_AGTH_ADD_PARAMS       8

struct InjectionSettings
{
	unsigned int injectionFlags;
	unsigned int agthFlags;
	unsigned int maxLogLen;
	int agthSymbolRepeatCount, agthPhraseCount1, agthPhraseCount2;

	unsigned int internalHookTime;
	unsigned int internalHookDelay;

	AtlasConfig atlasConfig;
	LCID forceLocale;

	wchar_t agthParams[MAX_PATH];
	wchar_t exePath[MAX_PATH];
	wchar_t *exeName;
	wchar_t *exeNamePlusFolder;
	wchar_t logPath[MAX_PATH];
};
