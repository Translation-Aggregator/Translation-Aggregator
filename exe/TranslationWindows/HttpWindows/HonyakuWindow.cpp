#include <Shared/Shrink.h>
#include "HonyakuWindow.h"
#include <Shared/StringUtil.h>

HonyakuWindow::HonyakuWindow() : HttpWindow(L"Honyaku", L"http://honyaku.yahoo.co.jp/")
{
	host = L"honyaku.yahoo.co.jp";
	path = L"/transtext";
	postPrefixTemplate = "both=TH&eid=CR-%s%s&text=%s";
}

wchar_t *HonyakuWindow::FindTranslatedText(wchar_t* html)
{
	int slen;
	wchar_t *start = GetSubstring(html, L" readonly>", L"</textarea>", &slen);
	if (!slen) return 0;
	start[slen] = 0;
	return start;
}

char *HonyakuWindow::GetLangIdString(Language lang, int src)
{
	switch (lang)
	{
		case LANGUAGE_English:
			return "E";
		case LANGUAGE_Japanese:
			return "J";
		case LANGUAGE_Chinese_Simplified:
			return "C-CN";
		case LANGUAGE_Chinese_Traditional:
			return "C";
		case LANGUAGE_Korean:
			return "K";
		default:
			return 0;
	}
}

char *HonyakuWindow::GetTranslationPrefix(Language src, Language dst, const char *text)
{
	if (src != LANGUAGE_Japanese && dst != LANGUAGE_Japanese)
		return NULL;
	if (src == LANGUAGE_Chinese_Simplified)
		src = LANGUAGE_Chinese_Traditional;
	return HttpWindow::GetTranslationPrefix(src, dst, text);
}
