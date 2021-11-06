#include <Shared/Shrink.h>
#include "LECOnlineWindow.h"
#include "../../util/HttpUtil.h"

LECOnlineWindow::LECOnlineWindow() : HttpWindow(L"LEC Online", L"http://www.lec.com/translate-demos.asp")
{
	host = L"www.lec.com";
	path = L"/translate-demos.asp";
	postPrefixTemplate = "selectSourceLang=%hs&selectTargetLang=%hs&DoTransText=go&SourceText=%s";
	impersonateIE = 1;
}

wchar_t *LECOnlineWindow::FindTranslatedText(wchar_t* html)
{
	html = wcsstr(html, L"<textarea ReadOnly ");
	if (!html)
		return NULL;
	html = wcschr(html, '>');
	if (!html)
		return NULL;
	wchar_t *e = wcsstr(++html, L"</textarea>");
	if (!e)
		return NULL;
	*e = 0;
	UnescapeHtml(html, 0);
	return html;
}

char *LECOnlineWindow::GetLangIdString(Language lang, int src)
{
	switch (lang)
	{
		case LANGUAGE_English:
			return "en";
		case LANGUAGE_Japanese:
			return "ja";
		case LANGUAGE_Arabic:
			return "ar";
		case LANGUAGE_Chinese_Simplified:
		case LANGUAGE_Chinese_Traditional:
			return "zh";
		case LANGUAGE_Dutch:
			return "nl";
		case LANGUAGE_French:
			return "fr";
		case LANGUAGE_German:
			return "de";
		case LANGUAGE_Hebrew:
			return "he";
		case LANGUAGE_Indonesian:
			return "id";
		case LANGUAGE_Italian:
			return "it";
		case LANGUAGE_Korean:
			return "ko";
		case LANGUAGE_Persian:
			return "fa";
		case LANGUAGE_Polish:
			return "pl";
		case LANGUAGE_Portuguese:
			return "pt";
		case LANGUAGE_Russian:
			return "ru";
		case LANGUAGE_Spanish:
			return "es";
		//case LANGUAGE_Tagalog:
		//	return "tl";
		case LANGUAGE_Turkish:
			return "tr";
		case LANGUAGE_Ukrainian:
			return "uk";
		case LANGUAGE_Pashto:
			return "ps";
		case LANGUAGE_Urdu:
			return "ur";
		default:
			return 0;
	}
}
