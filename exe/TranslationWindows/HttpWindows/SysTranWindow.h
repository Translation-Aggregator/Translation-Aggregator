#pragma once
#include "HttpWindow.h"

class SysTranWindow : public HttpWindow
{
public:
	SysTranWindow();
	wchar_t *FindTranslatedText(wchar_t* html);
	char *GetLangIdString(Language lang, int src);
	char *GetTranslationPrefix(Language src, Language dst, const char *text);
};
