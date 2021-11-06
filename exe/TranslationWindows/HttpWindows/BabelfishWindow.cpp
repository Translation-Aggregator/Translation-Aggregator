#include <Shared/Shrink.h>
#include "BabelfishWindow.h"
#include <Shared/StringUtil.h>
#include "../../util/HttpUtil.h"

BabelfishWindow::BabelfishWindow() : HttpWindow(L"Babel Fish", L"http://www.babelfish.com/")
{
	host = L"www.babelfish.com";
	path = L"";
	postPrefixTemplate = "slang=%s&dlang=%s&uText=%s&submit=";
	secondRequest = L"/success";
	port = 443;
}

wchar_t *BabelfishWindow::FindTranslatedText(wchar_t* html)
{
	int slen;
	wchar_t *start = GetSubstring(html, L"<h4>Translation</h4>", L"</div>", &slen);
	if (!slen)
		return NULL;
	start[slen] = 0;
	if (start[slen - 1] == ')')
	{
		wchar_t *p;
		for (p = start + slen - 2; p > start && (*p | 0x20u) - 'a' <= 'z' - 'a'; p--);
		if (p[0] == '(' && p[-1] == ' ')
			p[-1] = 0;
	}
	UnescapeHtml(start, 0);
	return start;
}

char *BabelfishWindow::GetLangIdString(Language lang, int src)
{
	switch (lang)
	{
		case LANGUAGE_English:
			return "en";
		case LANGUAGE_Japanese:
			return "ja";
		case LANGUAGE_Chinese_Simplified:
			return "zh-CHS";
		case LANGUAGE_Chinese_Traditional:
			return "zh-CHT";
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
		case LANGUAGE_Portuguese:
			return "pt";
		case LANGUAGE_Spanish:
			return "es";
		case LANGUAGE_Russian:
			return "ru";
		case LANGUAGE_Arabic:
			return "ar";
		case LANGUAGE_Bulgarian:
			return "bg";
		case LANGUAGE_Danish:
			return "da";
		case LANGUAGE_Hindi:
			return "hi";
		case LANGUAGE_Ukrainian:
			return "uk";
		default:
			return 0;
	}
}
