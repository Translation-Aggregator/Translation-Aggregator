#include <Shared/Shrink.h>
#include "ExciteWindow.h"
#include <Shared/StringUtil.h>
#include "../../util/HttpUtil.h"

ExciteWindow::ExciteWindow() : HttpWindow(L"Excite", L"https://www.excite.co.jp/world/english_japanese/")
{
	host = L"www.excite.co.jp";
	postPrefixTemplate = "%s%s&before=%s";
	impersonateIE = 1;
	port = 443;
}

wchar_t *ExciteWindow::FindTranslatedText(wchar_t* html)
{
	int slen;
	wchar_t *start = GetSubstring(html, L"name=\"after\"", L"</textarea>", &slen);
	if (!slen)
	{
		start = GetSubstring(html, L"name=after", L"</textarea>", &slen);
		if (!slen)
			return NULL;
	}
	wchar_t *out = wcschr(start, L'>');
	if (!out || out - start >= slen)
		return NULL;
	start[slen] = 0;
	UnescapeHtml(++out, 0);
	return out;
}

char *ExciteWindow::GetLangIdString(Language lang, int src)
{
	char *p;
	switch (lang)
	{
		case LANGUAGE_English:
			p = "wb_lp=EN"; break;
		case LANGUAGE_Japanese:
			p = "wb_lp=JA"; break;
		//TODO: fix char vs const char
		// case LANGUAGE_Chinese_Traditional:
		// 	return src ? "big5=yes&big5_lang=yes&wb_lp=CH" : "CH&big5=yes";
		case LANGUAGE_Chinese_Simplified:
			p = "wb_lp=CH"; break;
		case LANGUAGE_Korean:
			p = "wb_lp=KO"; break;
		case LANGUAGE_French:
			p = "wb_lp=FR"; break;
		case LANGUAGE_German:
			p = "wb_lp=DE"; break;
		case LANGUAGE_Italian:
			p = "wb_lp=IT"; break;
		case LANGUAGE_Spanish:
			p = "wb_lp=ES"; break;
		case LANGUAGE_Portuguese:
			p = "wb_lp=PT"; break;
		case LANGUAGE_Russian:
			p = "wb_lp=RU"; break;
		default:
			return NULL;
	}
	return src ? p : p + 6;
}

wchar_t *ExciteWindow::GetTranslationPath(Language src, Language dst, const wchar_t *text)
{
	if (src != LANGUAGE_Japanese && dst != LANGUAGE_Japanese || src == dst)
		return NULL;
	const wchar_t *p;
	switch (dst != LANGUAGE_Japanese ? dst : src)
	{
		case LANGUAGE_English:
			p = L"/world/english_japanese/"; break;
		case LANGUAGE_Chinese_Simplified:
		case LANGUAGE_Chinese_Traditional:
			p = L"/world/chinese/"; break;
		case LANGUAGE_Korean:
			p = L"/world/korean/"; break;
		case LANGUAGE_French:
			p = L"/world/french/"; break;
		case LANGUAGE_German:
			p = L"/world/german/"; break;
		case LANGUAGE_Italian:
			p = L"/world/italian/"; break;
		case LANGUAGE_Spanish:
			p = L"/world/spanish/"; break;
		case LANGUAGE_Portuguese:
			p = L"/world/portuguese/"; break;
		case LANGUAGE_Russian:
			p = L"/world/russian/"; break;
		default:
			return NULL;
	}
	return _wcsdup(p);
}
