#include <Shared/Shrink.h>
#include "SysTranWindow.h"
#include "../../util/HttpUtil.h"

SysTranWindow::SysTranWindow() : HttpWindow(L"SysTran", L"http://www.systranet.com/translate")
{
	host = L"www.systranet.com";
	path = L"/sai?lp=%hs_%hs&service=systranettranslate";
	postPrefixTemplate = "";
	dontEscapeRequest = true;
}

wchar_t *SysTranWindow::FindTranslatedText(wchar_t* html)
{
	html = wcsstr(html, L"<meta ");
	if (!html)
		return NULL;
	size_t len = wcslen(html);
	if (html[len - 1] == ';')
		html[len - 1] = 0;
	UnescapeHtml(html, 0);
	return html;
}

char *SysTranWindow::GetLangIdString(Language lang, int src)
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
		case LANGUAGE_Greek:
			return "el";
		case LANGUAGE_Italian:
			return "it";
		case LANGUAGE_Korean:
			return "ko";
		case LANGUAGE_Polish:
			return "pl";
		case LANGUAGE_Portuguese:
			return "pt";
		case LANGUAGE_Russian:
			return "ru";
		case LANGUAGE_Spanish:
			return "es";
		case LANGUAGE_Swedish:
			return "sv";
		default:
			return 0;
	}
}

char *SysTranWindow::GetTranslationPrefix(Language src, Language dst, const char *text)
{
	if (src != LANGUAGE_English && dst != LANGUAGE_English || src == dst)
		return NULL;

	if (!text)
		return (char*)1;

	size_t len = 27;
	const char *p;
	for (p = text; *p; p++)
		switch (*p)
		{
			case '\n':
			case '<':
			case '>': len += 3; break;
			case '&': len += 4; break;
		}
	len += p - text;
	char *data = (char*)malloc(len);
	strcpy(data, "<html><body>");
	char *d = data + 12;
	for (p = text; *p; p++)
		switch (*p)
		{
			case '\n': strcpy(d, "<br>"); d += 4; break;
			case '<': strcpy(d, "&lt;"); d += 4; break;
			case '>': strcpy(d, "&gt;"); d += 4; break;
			case '&': strcpy(d, "&amp;"); d += 5; break;
			default: *d++ = *p;
		}
	strcpy(d, "</body></html>");

	return data;
}
