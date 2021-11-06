#include <Shared/Shrink.h>
#include "SDLWindow.h"

SDLWindow::SDLWindow() : HttpWindow(L"SDL", L"https://www.freetranslation.com/")
{
	host = L"api.freetranslation.com";
	path = L"/freetranslation/translations/text";
	requestHeaders = L"Content-Type: application/json";
	postPrefixTemplate = "{\"from\":\"%s\",\"to\":\"%s\",\"text\":\"%s\"}";
	impersonateIE = 1;
	port = 443;
}

wchar_t *SDLWindow::FindTranslatedText(wchar_t* html)
{
/*	wchar_t *start = wcsstr(html, L",\"translation\":\"");
	if (!start)
		return NULL;
	start += 16;
	size_t slen = wcslen(start);
	if (slen >= 2)
		start[slen - 2] = 0;
	if (wchar_t *s = wcschr(start, '\\'))
	{
		wchar_t *d = s;
		do
			if (*s == '\\')
				if (*++s == 'n')
					*s = '\n';
		while (*d++ = *s++);
	}
	return start;*/

	if(!ParseJSON(html, L"\"translation\":\"", L"\"translation\":\""))
		return NULL;
	return html;
}

char *SDLWindow::GetLangIdString(Language lang, int src)
{
	switch (lang)
	{
		case LANGUAGE_English:
			return "eng";
		case LANGUAGE_Japanese:
			return "jpn";
		case LANGUAGE_Arabic:
			return "ara";
		case LANGUAGE_Bengali:
			return "ben";
		case LANGUAGE_Bulgarian:
			return "bul";
		case LANGUAGE_Chinese_Simplified:
			return "chi";
		case LANGUAGE_Chinese_Traditional:
			return "cht";
		case LANGUAGE_Czech:
			return "cze";
		case LANGUAGE_Danish:
			return "dan";
		case LANGUAGE_Dari:
			return "fad";
		case LANGUAGE_Dutch:
			return "dut";
		case LANGUAGE_Estonian:
			return "est";
		case LANGUAGE_Finnish:
			return "fin";
		case LANGUAGE_French:
			return "fra";
		case LANGUAGE_German:
			return "ger";
		case LANGUAGE_Greek:
			return "gre";
		case LANGUAGE_Hausa:
			return "hau";
		case LANGUAGE_Hebrew:
			return "heb";
		case LANGUAGE_Hindi:
			return "hin";
		case LANGUAGE_Hungarian:
			return "hun";
		case LANGUAGE_Indonesian:
			return "ind";
		case LANGUAGE_Italian:
			return "ita";
		case LANGUAGE_Korean:
			return "kor";
		case LANGUAGE_Lithuanian:
			return "lit";
		case LANGUAGE_Malay:
			return "may";
		case LANGUAGE_Norwegian:
			return "nor";
		case LANGUAGE_Pashto:
			return "pus";
		case LANGUAGE_Persian:
			return "per";
		case LANGUAGE_Polish:
			return "pol";
		case LANGUAGE_Portuguese:
			return "por";
		case LANGUAGE_Romanian:
			return "rum";
		case LANGUAGE_Russian:
			return "rus";
		case LANGUAGE_Serbian:
			return "srp";
		case LANGUAGE_Slovak:
			return "slo";
		case LANGUAGE_Slovenian:
			return "slv";
		case LANGUAGE_Somali:
			return "som";
		case LANGUAGE_Spanish:
			return "spa";
		case LANGUAGE_Swedish:
			return "swe";
		case LANGUAGE_Thai:
			return "tha";
		case LANGUAGE_Turkish:
			return "tur";
		case LANGUAGE_Ukrainian:
			return "ukr";
		case LANGUAGE_Urdu:
			return "urd";
		case LANGUAGE_Vietnamese:
			return "vie";
		default:
			return 0;
	}
}

char *SDLWindow::GetTranslationPrefix(Language src, Language dst, const char *text)
{
	if (src != LANGUAGE_English && dst != LANGUAGE_English || src == dst)
		return 0;
	return HttpWindow::GetTranslationPrefix(src, dst, text);
}
