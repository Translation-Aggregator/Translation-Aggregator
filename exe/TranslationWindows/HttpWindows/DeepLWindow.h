#pragma once
#include "HttpWindow.h"

class DeepLWindow : public HttpWindow
{
public:
	DeepLWindow();
	~DeepLWindow();
	wchar_t* FindTranslatedText(wchar_t* html);
	char* GetLangIdString(Language lang, int src);
	char* GetTranslationPrefix(Language src, Language dst, const char* text);

private:
	wchar_t* m_pResult;
	int64_t m_id;
};
