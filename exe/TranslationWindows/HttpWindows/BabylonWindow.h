#pragma once
#include "HttpWindow.h"

class BabylonWindow : public HttpWindow
{
public:
	BabylonWindow();
	wchar_t *FindTranslatedText(wchar_t* html);
	char *GetLangIdString(Language lang, int src);
};
