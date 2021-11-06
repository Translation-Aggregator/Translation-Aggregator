#pragma once
#include "HttpWindow.h"

class SysTranWindow : public HttpWindow
{
public:
	SysTranWindow();
	wchar_t *FindTranslatedText(wchar_t* html);
	char *GetLangIdString(Language lang, int src);
	char *GetTranslationPrefix(Language src, Language dst, const char *text);

private:
	void getToken();

private:
	std::string m_token;
	std::wstring m_baseRequestHeaders;
	std::wstring m_requestHeaders;
};
