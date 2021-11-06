#include <Shared/Shrink.h>
#include "BabylonWindow.h"
#include <Shared/StringUtil.h>

BabylonWindow::BabylonWindow() : HttpWindow(L"Babylon", L"http://translation.babylon-software.com")
{
	host = L"translation.babylon-software.com";
	path = L"/translate/babylon.php?v=1.0&langpair=%hs%%7C%hs&callback=babylonTranslator.callback&context=babylon.8.0._babylon_api_response&q=%s";
	requestHeaders = L"";
	impersonateIE = 1;
}

wchar_t *BabylonWindow::FindTranslatedText(wchar_t* html)
{
	if (!ParseJSON(html, L"\"translatedText\":\"", NULL))
		return NULL;
	return html;
}

char *BabylonWindow::GetLangIdString(Language lang, int src)
{
	switch (lang)
	{
		case LANGUAGE_English:
			return "0";
		case LANGUAGE_Japanese:
			return "8";
		case LANGUAGE_French:
			return "1";
		case LANGUAGE_Italian:
			return "2";
		case LANGUAGE_German:
			return "6";
		case LANGUAGE_Portuguese:
			return "5";
		case LANGUAGE_Spanish:
			return "3";
		case LANGUAGE_Arabic:
			return "15";
		case LANGUAGE_Catalan:
			return "99";
		case LANGUAGE_Castilian:
			return "344";
		case LANGUAGE_Czech:
			return "31";
		case LANGUAGE_Chinese_Simplified:
			return "10";
		case LANGUAGE_Chinese_Traditional:
			return "9";
		case LANGUAGE_Danish:
			return "43";
		case LANGUAGE_Greek:
			return "11";
		case LANGUAGE_Hebrew:
			return "14";
		case LANGUAGE_Hindi:
			return "60";
		case LANGUAGE_Hungarian:
			return "30";
		case LANGUAGE_Persian:
			return "51";
		case LANGUAGE_Korean:
			return "12";
		case LANGUAGE_Dutch:
			return "4";
		case LANGUAGE_Norwegian:
			return "46";
		case LANGUAGE_Polish:
			return "29";
		case LANGUAGE_Romanian:
			return "47";
		case LANGUAGE_Russian:
			return "7";
		case LANGUAGE_Swedish:
			return "48";
		case LANGUAGE_Turkish:
			return "13";
		case LANGUAGE_Thai:
			return "16";
		case LANGUAGE_Ukrainian:
			return "49";
		case LANGUAGE_Urdu:
			return "39";
		default:
			return 0;
	}
}
