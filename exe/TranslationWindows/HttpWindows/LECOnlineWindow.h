#pragma once
#include "HttpWindow.h"

class LECOnlineWindow : public HttpWindow
{
public:
	LECOnlineWindow();
	wchar_t *FindTranslatedText(wchar_t* html);
	char *GetLangIdString(Language lang, int src);
};
