#pragma once
#include "HttpWindow.h"

class BingWindow : public HttpWindow
{
public:
	BingWindow();
	~BingWindow();
	wchar_t *FindTranslatedText(wchar_t* html);
	char* GetLangIdString(Language lang, int src);
	wchar_t* GetTranslationPath(Language src, Language dst, const wchar_t* text);
	char *GetTranslationPrefix(Language src, Language dst, const char *text);

private:
	void getToken();

private:
	std::string m_token;
	std::string m_key;
	std::string m_ig;
	std::string m_iid;
	uint32_t m_useCnt;
};
