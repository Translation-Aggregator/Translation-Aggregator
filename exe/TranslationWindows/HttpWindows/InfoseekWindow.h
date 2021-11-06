#pragma once
#include "HttpWindow.h"

class InfoseekWindow : public HttpWindow
{
public:
	InfoseekWindow();
	wchar_t *FindTranslatedText(wchar_t* html);
	char *GetLangIdString(Language lang, int src);
};
