#pragma once
#include "../TranslationWindow.h"
#include "../../util/Dictionary.h"

#define WORD_HAS_TRANS  1

struct FuriganaPartOfSpeech
{
	int id;
	COLORREF color;
	wchar_t *string;
};

extern const FuriganaPartOfSpeech FuriganaPartsOfSpeech[8];

struct FuriganaWord
{
	int line;
	unsigned short width;
	unsigned short furiganaWidth;
	unsigned short hOffset;
	unsigned char pos;
	unsigned char flags;
	union
	{
		wchar_t *strings[3];
		struct
		{
			wchar_t *word;
			wchar_t *pro;
			wchar_t *srcWord;
		};
	};
};

struct Result
{
	wchar_t *string;
	FuriganaWord *words;
	int numWords;
};

enum FuriganaCharacterType
{
	HIRAGANA,
	KATAKANA,
	ROMAJI,
	NO_FURIGANA,
};


class FuriganaWindow : public TranslationWindow
{
protected:
	void CleanupResult();
public:
	RECT toolTipRect;
	int toolTipVisible;
	void CheckToolTip(int x, int y);
	void HideToolTip();

	wchar_t * error;
	union
	{
		struct
		{
			unsigned int defaultBKColor;
			unsigned int furiganaBKColor;
			unsigned int particleBKColor;
			unsigned int transBKColor;
		};
		unsigned int colors[4];
	};

	int GetStartWord();
	int normalFontSize;
	int furiganaFontSize;
	FuriganaCharacterType characterType;

	int tracking;

	int formatWidth;
	int topWord;
	int scrollVisible;
	int heightFontBig;
	int heightFontSmall;
	int heightFontDouble;
	int GetTopLine();
	void Draw();
	void NukeWindow();
	void Reformat(int widthChangeOnly=0);
	Result *data;

	HFONT hFontBig;
	HFONT hFontSmall;

	FuriganaWindow(wchar_t *type, char remote, wchar_t *srcUrl, unsigned int flags, DWORD defaultCharType);

	virtual ~FuriganaWindow();
	void WordHover(int word, RECT &clientRect, RECT &globalRect);

	void UpdateScrollbar();
	int MakeWindow(int showToolbar, HWND hWndParent);
	virtual void SaveWindowTypeConfig();
};

inline unsigned int RGBFlip(unsigned int color)
{
	return ((color>>16)&0xFF) + (color&0xFF00) + ((color&0xFF)<<16);
}
