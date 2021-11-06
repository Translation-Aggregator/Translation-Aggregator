#pragma once
#include "HttpWindow.h"

class ExciteWindow : public HttpWindow
{
public:
	ExciteWindow();
	wchar_t *FindTranslatedText(wchar_t* html);
	char *GetLangIdString(Language lang, int src);
	wchar_t *GetTranslationPath(Language src, Language dst, const wchar_t *text);
};
