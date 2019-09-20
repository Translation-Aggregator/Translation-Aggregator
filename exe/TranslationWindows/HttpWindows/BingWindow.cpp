#include <Shared/Shrink.h>
#include "BingWindow.h"

BingWindow::BingWindow() : HttpWindow(L"Bing", L"https://www.bing.com/ttranslatev3/")
{
	host = L"www.bing.com";
	path = L"/ttranslatev3";
	postPrefixTemplate = "&text=%s&fromLang=%s&to=%s";
	port = 80;
	requestHeaders = L"Content-Type: application/x-www-form-urlencoded";
	dontEscapeRequest = true;
}
BingWindow::~BingWindow()
{
}

wchar_t *BingWindow::FindTranslatedText(wchar_t* html)
{
	if (!ParseJSON(html, L"\"text\":\"", L"\"text\":\""))
		return NULL;
	return html;
}

char *EscapeParam(const char *src, int len)
{
	char *out = (char*)malloc(sizeof(char)*(3 * strlen(src) + 1));
	char *dst = out;
	while(len)
	{
		len--;
		char c = *src;
		if(c <= 0x26 || c == '+' || (0x3A <= c && c <= 0x40) || c == '\\' || (0x5B <= c && c <= 0x60) || (0x7B <= c && c <= 0x7E))
		{
			sprintf(dst, "%%%02X", (unsigned char)c);
			dst += 3;
			src++;
			continue;
		}
		dst++[0] = c;
		src++;
	}
	*dst = 0;
	return out;
}

char *BingWindow::GetTranslationPrefix(Language src, Language dst, const char *text)
{
	if (!postPrefixTemplate)
		return 0;

	char *srcString, *dstString;
	if (!(srcString = GetLangIdString(src, 1)) || !(dstString = GetLangIdString(dst, 0)) || !strcmp(srcString, dstString))
		return 0;

	if (!text)
		return (char*)1;

#if 0
	int lines = 1;
	for(const char *p = strchr(text, '\n'); p; p = strchr(p + 1, '\n'))
		lines++;

	int zlen = strlen(text) * 2 + 15 * lines + 4;
	char *text2 = (char*)malloc(strlen(text) * 2 + 15 * lines + 4), *pd = text2;
	strcpy(pd, "[{\"text\":\"\xEF\xBB\xBF");
	pd += 13;
	const char *ps = text;
	for(; *ps; ps++)
		switch(*ps)
		{
			case '\r': break;
			case '\n': strcpy(pd, "\"},{\"text\":\"\xEF\xBB\xBF"); pd += 15; break;
			case '\t': *pd++ = '\\'; *pd++ = 't'; break;
			case '\"': *pd++ = '\\'; *pd++ = '"'; break;
			case '\\': *pd++ = '\\'; *pd++ = '\\'; break;
			default: *pd++ = *ps; break;
		}
	if(ps != text && ps[-1] == '\n')
		pd -= 15; /* Has to be -15 because of the "} at the beginning the , her is equal to the [ in the other string */
	strcpy(pd, "\"}]");

	int zlen2 = strlen(text2);
	return text2;
#endif // 0

	char *text2 = EscapeParam(text, strlen(text));

	char *out = (char*)malloc(strlen(postPrefixTemplate) + strlen(text2) + strlen(srcString) + strlen(dstString) + 1);
	sprintf(out, postPrefixTemplate, text2, srcString, dstString);
	free(text2);
	return out;
}

char* BingWindow::GetLangIdString(Language lang, int src)
{
	switch (lang)
	{
		case LANGUAGE_AUTO:
			return "";
		case LANGUAGE_English:
			return "en";
		case LANGUAGE_Japanese:
			return "ja";

		case LANGUAGE_Hebrew:
			return "he";
		case LANGUAGE_Queretaro_Otomi:
			return "otq";
		case LANGUAGE_Arabic:
			return "ar";
		case LANGUAGE_Hindi:
			return "hi";
		case LANGUAGE_Romanian:
			return "ro";
		case LANGUAGE_Bosnian:
			return "bs-Latn";
		case LANGUAGE_Hmong_Daw:
			return "mww";
		case LANGUAGE_Russian:
			return "ru";
		case LANGUAGE_Bulgarian:
			return "bg";
		case LANGUAGE_Hungarian:
			return "hu";
		case LANGUAGE_Serbian:
			return "sr-Cyrl";
		case LANGUAGE_Catalan:
			return "ca";
		case LANGUAGE_Indonesian:
			return "id";
		case LANGUAGE_Chinese_Simplified:
			return "zh-CHS";
		case LANGUAGE_Italian:
			return "it";
		case LANGUAGE_Slovak:
			return "sk";
		case LANGUAGE_Chinese_Traditional:
			return "zh-CHT";
		case LANGUAGE_Slovenian:
			return "sl";
		case LANGUAGE_Croatian:
			return "hr";
		case LANGUAGE_Klingon:
			return "tlh";
		case LANGUAGE_Spanish:
			return "es";
		case LANGUAGE_Czech:
			return "cs";
		case LANGUAGE_Swedish:
			return "sv";
		case LANGUAGE_Danish:
			return "da";
		case LANGUAGE_Korean:
			return "ko";
		case LANGUAGE_Thai:
			return "th";
		case LANGUAGE_Dutch:
			return "nl";
		case LANGUAGE_Latvian:
			return "lv";
		case LANGUAGE_Turkish:
			return "tr";
		case LANGUAGE_Lithuanian:
			return "lt";
		case LANGUAGE_Ukrainian:
			return "uk";
		case LANGUAGE_Estonian:
			return "et";
		case LANGUAGE_Malay:
			return "ms";
		case LANGUAGE_Urdu:
			return "ur";
		case LANGUAGE_Finnish:
			return "fi";
		case LANGUAGE_Maltese:
			return "mt";
		case LANGUAGE_Vietnamese:
			return "vi";
		case LANGUAGE_French:
			return "fr";
		case LANGUAGE_Norwegian:
			return "no";
		case LANGUAGE_Welsh:
			return "cy";
		case LANGUAGE_German:
			return "de";
		case LANGUAGE_Persian:
			return "fa";
		case LANGUAGE_Yucatec_Maya:
			return "yua";
		case LANGUAGE_Greek:
			return "el";
		case LANGUAGE_Polish:
			return "pl";
		case LANGUAGE_Haitian_Creole:
			return "ht";
		case LANGUAGE_Portuguese:
			return "pt";
		default:
			return 0;
	}
}
