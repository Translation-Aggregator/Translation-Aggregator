#include <Shared/Shrink.h>
#include "FreeTranslationWindow.h"

FreeTranslationWindow::FreeTranslationWindow() : HttpWindow(L"FreeTranslation", L"http://www.freetranslation.com/")
{
	host = L"tets9.freetranslation.com";
	path = L"/";
	postPrefixTemplate = "sequence=core&charset=UTF-8&language=%s%%2F%s&srctext=%s";
}

wchar_t *FreeTranslationWindow::FindTranslatedText(wchar_t* html)
{
	if (wchar_t *s = wcschr(html, L'《'))
	{
		wchar_t *d = s;
		do
			if (*s == L'《')
				for (s++; *s && *s++ != L'》';);
		while (*d++ = *s++);
	}
	return html;
}

char *FreeTranslationWindow::GetLangIdString(Language lang, int src)
{
	switch (lang)
	{
		case LANGUAGE_English:
			return "English";
		case LANGUAGE_Japanese:
			return "Japanese";
		case LANGUAGE_Danish:
			return "Danish";
		case LANGUAGE_Dutch:
			return "Dutch";
		case LANGUAGE_French:
			return "French";
		case LANGUAGE_German:
			return "German";
		case LANGUAGE_Italian:
			return "Italian";
		case LANGUAGE_Norwegian:
			return "Norwegian";
		case LANGUAGE_Portuguese:
			return "Portuguese";
		case LANGUAGE_Russian:
			return "Russian";
		case LANGUAGE_Spanish:
			return "Spanish";
		case LANGUAGE_Swedish:
			return "Swedish";
		default:
			return 0;
	}
}

char *FreeTranslationWindow::GetTranslationPrefix(Language src, Language dst, const char *text)
{
	if (src != LANGUAGE_English && dst != LANGUAGE_English || src == dst)
		return 0;
	return HttpWindow::GetTranslationPrefix(src, dst, text);
}
