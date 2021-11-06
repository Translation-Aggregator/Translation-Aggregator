#pragma once
#include <Shared/Atlas.h>
#include <Shared/InjectionSettings.h>

#include <Shared/TAPlugin.h>

#define CONFIG_ENABLE_SUBSTITUTIONS  0x1
#define CONFIG_ENABLE_AUTO_HIRAGANA  0x2
#define CONFIG_CONTEXT_FULL_HISTORY  0x4

#define JPARSER_DISPLAY_VERB_CONJUGATIONS 0x01
#define JPARSER_JAPANESE_OWN_LINE         0x02
#define JPARSER_SINGLE_KANJI              0x04
#define JPARSER_SINGLE_HIRAGANA           0x08
#define JPARSER_USE_MECAB                 0x10


// Not to be confused with LANG_ENGLISH, etc, defined by MS.
// I don't use those just so I can deal with a nice, closed range.
enum Language
{
	LANGUAGE_AUTO,
	LANGUAGE_English,
	LANGUAGE_Japanese,

	LANGUAGE_Chinese_Simplified,
	LANGUAGE_Chinese_Traditional,
	LANGUAGE_Dutch,
	LANGUAGE_French,
	LANGUAGE_German,
	LANGUAGE_Greek,
	LANGUAGE_Italian,
	LANGUAGE_Portuguese,
	LANGUAGE_Spanish,
	LANGUAGE_Korean,
	LANGUAGE_Russian,

	LANGUAGE_Afrikaans,
	LANGUAGE_Albanian,
	LANGUAGE_Arabic,
	LANGUAGE_Belarusian,
	LANGUAGE_Bengali,
	LANGUAGE_Bosnian,
	LANGUAGE_Bulgarian,
	LANGUAGE_Catalan,
	LANGUAGE_Castilian,
	LANGUAGE_Croatian,
	LANGUAGE_Czech,
	LANGUAGE_Danish,
	LANGUAGE_Dari,
	LANGUAGE_Esperanto,
	LANGUAGE_Estonian,
	LANGUAGE_Filipino,
	LANGUAGE_Finnish,
	LANGUAGE_Galician,
	LANGUAGE_Haitian_Creole,
	LANGUAGE_Hausa,
	LANGUAGE_Hebrew,
	LANGUAGE_Hindi,
	LANGUAGE_Hmong_Daw,
	LANGUAGE_Hungarian,
	LANGUAGE_Icelandic,
	LANGUAGE_Indonesian,
	LANGUAGE_Irish,
	LANGUAGE_Klingon,
	LANGUAGE_Latin,
	LANGUAGE_Latvian,
	LANGUAGE_Lithuanian,
	LANGUAGE_Macedonian,
	LANGUAGE_Malay,
	LANGUAGE_Maltese,
	LANGUAGE_Norwegian,
	LANGUAGE_Pashto,
	LANGUAGE_Persian,
	LANGUAGE_Polish,
	LANGUAGE_Queretaro_Otomi,
	LANGUAGE_Romanian,
	LANGUAGE_Serbian,
	LANGUAGE_Slovak,
	LANGUAGE_Slovenian,
	LANGUAGE_Somali,
	LANGUAGE_Swahili,
	LANGUAGE_Swedish,
	LANGUAGE_Thai,
	LANGUAGE_Turkish,
	LANGUAGE_Ukrainian,
	LANGUAGE_Urdu,
	LANGUAGE_Vietnamese,
	LANGUAGE_Welsh,
	LANGUAGE_Yiddish,
	LANGUAGE_Yucatec_Maya,
	LANGUAGE_Zulu,
	LANGUAGE_NONE,
};

struct PluginInfo
{
	HMODULE hDll;
	TAPluginInitializeType *Initialize;
	TAPluginModifyStringPreSubstitutionType *ModifyStringPreSubstitution;
	TAPluginModifyStringPostSubstitutionType *ModifyStringPostSubstitution;
	TAPluginActiveProfileListType *ActiveProfileList;
	TAPluginFreeType *Free;
};

wchar_t *GetLanguageString(Language lang);

#define RANGE_MAX 100000

struct ReplaceString
{
	wchar_t *old;
	wchar_t *rep;
	int len;
};

struct ReplaceSubList
{
	ReplaceString *strings;
	int numStrings;
	int active;
	wchar_t profile[MAX_PATH];

	int DeleteString(wchar_t *old);
	ReplaceString *AddString(wchar_t *old, wchar_t *rep);
	int FindString(wchar_t *old);
	ReplaceString *FindReplacement(wchar_t *string);
	void Clear();
};

struct ReplaceList
{
	ReplaceSubList *lists;
	int numLists;

	wchar_t path[MAX_PATH];

	inline void FindActive()
	{
		FindActiveAndPopulateCB(0);
	}

	int AddProfile(wchar_t *profile);

	// If it finds something active in hWndCombo, returns its index, even
	// if there's no associated string list.
	int FindActiveAndPopulateCB(HWND hWndCombo);

	ReplaceString *AddString(wchar_t *old, wchar_t *rep, wchar_t *profile);
	void DeleteString(wchar_t *old, wchar_t *profile);
	void Save();
	void Load();
	int FindProfile(wchar_t *profile);

	ReplaceString *FindReplacement(wchar_t *string);

	inline void Clear()
	{
		for (int list=0; list<numLists; list++)
		{
			for (int i=0; i<lists[list].numStrings; i++) free(lists[list].strings[i].old);
			free(lists[list].strings);
		}
		free(lists);
		numLists = 0;
		lists = 0;
	}
	inline ReplaceList()
	{
		lists = 0;
		numLists = 0;
	}
};

struct MyFontInfo
{
	wchar_t face[32];
	int height;
	unsigned int color;
	unsigned char bold;
	unsigned char italic;
	unsigned char strikeout;
	unsigned char underline;
};

void InitLogFont(LOGFONT *lf, MyFontInfo *font);
int MyChooseFont(HWND hWnd, MyFontInfo *font);

// Holds all config and state info.
struct Config
{
	MyFontInfo font;
	MyFontInfo toolTipFont;

	unsigned char autoHalfWidthToFull;

	unsigned char jParserHideCrossRefs;
	unsigned char jParserHideUsage;
	unsigned char jParserHidePos;

	unsigned char jParserDefinitionLines;
	unsigned char jParserReformatNumbers;
	unsigned char jParserNoKanaBrackets;

	unsigned short port;

	unsigned int jParserFlags;

	unsigned int flags;

	Language langSrc;
	Language langDst;

	union
	{
		struct
		{
			unsigned int toolTipKanji;
			unsigned int toolTipHiragana;
			unsigned int toolTipParen;
			unsigned int toolTipConj;
		};
		unsigned int colors[4];
	};

	HFONT hIdFont;

	wchar_t myPath[MAX_PATH];
	wchar_t ini[MAX_PATH];

	int numReplaceLists;
	ReplaceList replace;

	int numPlugins;
	PluginInfo *plugins;

	bool global_hotkeys;
};

extern Config config;

void SaveGeneralConfig();
void LoadGeneralConfig();

void WritePrivateProfileInt(wchar_t *v1, wchar_t *v2, int i, wchar_t *v3);

// Currently returns 1 when game's ini isn't found but sets defaults, and 2 if all goes well.
// 0 reserved for complete failure, not currently used.  Path is full exe path.  Overwrites
// old one, if any disagreement.  Ignored if null.
int LoadInjectionSettings(InjectionSettings &cfg, wchar_t *name, wchar_t *path, std::wstring* internalHooks);
int SaveInjectionSettings(InjectionSettings &cfg, wchar_t *name, const wchar_t* internalHooks);

// Code actually exists in main file, as it uses TranslationWindow objects.
void SaveConfig();

// Also saves.
void CleanupConfig();

// Returns 0 if file doesn't exist or name is not default and not a fully resolved path.
// If name is default or name not a fully resolved path, sets path value to main ini's path.
int GetConfigPath(wchar_t *path, wchar_t *name);
void ConfigDialog(HWND hWnd);

#define CONTEXT_SETTINGS_VERSION  0x3

// 0's are default.
#define CHAR_REPEAT_AUTO_CONSTANT    0x0
#define CHAR_REPEAT_NONE        0x1
#define CHAR_REPEAT_INFINITE      0x2
#define CHAR_REPEAT_AUTO_ADVANCED    0x3
// Should be last
#define CHAR_REPEAT_CUSTOM        0x4

#define PHRASE_REPEAT_AUTO    0x0
#define PHRASE_REPEAT_NONE    0x1
#define PHRASE_REPEAT_CUSTOM  0x2

#define PHRASE_EXTENSION_NONE    0x0
#define PHRASE_EXTENSION_BASIC    0x1
#define PHRASE_EXTENSION_AGGRESSIVE  0x2

#define LINE_BREAK_REMOVE    0x0
#define LINE_BREAK_KEEP      0x1
#define LINE_BREAK_REMOVE_SOME  0x2

#define MERGED_CONTEXT L"::Merged"

struct ContextSettings
{
	unsigned int version;
	unsigned char charRepeatMode:4;
	unsigned char phaseRepeatMode:3;
	unsigned char disable:1;
	unsigned char phaseExtensionMode:2;
	unsigned char lineBreakMode:2;
	unsigned char autoTrans:1;
	unsigned char japOnly:1;
	unsigned char instaMerge:1;
	unsigned char flushMerge:1;
	unsigned char textLoops:1;
	unsigned char autoClipboard:1;
	unsigned char customRepeats[5];
	unsigned char lineBreaksFirst;
	unsigned char lineBreaksLast;
	unsigned short phraseMin;
	unsigned short phraseMax;
};

int LoadInjectionContextSettings(wchar_t *name, wchar_t *contextName, ContextSettings *settings);
int SaveInjectionContextSettings(wchar_t *name, wchar_t *contextName, ContextSettings *settings);

void SetDlgButton(HWND hWndDlg, unsigned int id, int pressed);
