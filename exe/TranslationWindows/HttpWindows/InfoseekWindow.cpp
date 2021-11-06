#include <Shared/Shrink.h>
#include "InfoseekWindow.h"

InfoseekWindow::InfoseekWindow() : HttpWindow(L"Infoseek", L"http://translation.infoseek.ne.jp/")
{
	host = L"translation.infoseek.ne.jp";
	path = L"/clsoap/translate";
	postPrefixTemplate = "e=%s%s&t=%s";
	requestHeaders = L"Content-Type: application/x-www-form-urlencoded\r\nCross-Licence: infoseek/main e3f33620ae053e48cdba30a16b1084b5d69a3a6c";
}

wchar_t *InfoseekWindow::FindTranslatedText(wchar_t* html)
{
	return ParseJSON(html, L"{\"text\":\"", NULL) ? html : NULL;
}

char *InfoseekWindow::GetLangIdString(Language lang, int src)
{
	switch (lang)
	{
		case LANGUAGE_English:
			return "E";
		case LANGUAGE_Japanese:
			return "J";
		case LANGUAGE_Korean:
			return "K";
		case LANGUAGE_Chinese_Traditional:
			if (!src)
				return "CT";
		case LANGUAGE_Chinese_Simplified:
			return "C";
		case LANGUAGE_French:
			return "F";
		case LANGUAGE_Italian:
			return "I";
		case LANGUAGE_Spanish:
			return "S";
		case LANGUAGE_German:
			return "G";
		case LANGUAGE_Portuguese:
			return "P";
		default:
			return 0;
	}
}
