#pragma once
#include "HttpWindow.h"

class BabelfishWindow : public HttpWindow
{
public:
	BabelfishWindow();
	wchar_t *FindTranslatedText(wchar_t* html);
	char *GetLangIdString(Language lang, int src);
};
