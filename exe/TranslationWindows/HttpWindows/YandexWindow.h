#pragma once
#include "HttpWindow.h"

class YandexWindow : public HttpWindow
{
	char *SessionId;
	unsigned int counter;
public:
	YandexWindow();
	~YandexWindow();
	wchar_t *FindTranslatedText(wchar_t* html);
	char* GetLangIdString(Language lang, int src);
	wchar_t *GetTranslationPath(Language src, Language dst, const wchar_t *text);
};
