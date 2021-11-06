#include <Shared/Shrink.h>
#include "YandexWindow.h"

YandexWindow::YandexWindow() : HttpWindow(L"Yandex", L"https://translate.yandex.com/")
{
	host = L"translate.yandex.net";
	path = L"/api/v1/tr.json/translate?lang=%hs-%hs&srv=tr-text&id=%hs-%u-0";
	postPrefixTemplate = "from=%s&to=%s&text=%s";
	port = 443;
	SessionId = NULL;
	counter = 0;
}
YandexWindow::~YandexWindow()
{
	free(SessionId);
}

wchar_t *YandexWindow::FindTranslatedText(wchar_t* html)
{
	if (!ParseJSON(html, L"\"text\":[\"", NULL))
		return NULL;
	return html;
}

char *DownloadFile(HINTERNET hConnect, const wchar_t *url, DWORD flags)
{
	char *data = NULL;
	if (HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", url, 0, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags))
	{
		if (WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
			WinHttpReceiveResponse(hRequest, NULL))
		{
			size_t size = 0;
			for (;;)
			{
				DWORD len;
				if (!WinHttpQueryDataAvailable(hRequest, &len) || !len)
					break;
				char *data2 = (char*)realloc(data, size + len + 1);
				if (!data2)
					break;
				data = data2;
				if (!WinHttpReadData(hRequest, data + size, len, &len) || !len)
					break;
				size += len;
			}
			if (data)
				data[size] = 0;
		}
		WinHttpCloseHandle(hRequest);
	}
	return data;
}

wchar_t *YandexWindow::GetTranslationPath(Language src, Language dst, const wchar_t *text)
{
	char *srcString, *dstString;
	if (src == dst || !(srcString = GetLangIdString(src, 1)) || !(dstString = GetLangIdString(dst, 0)))
		return NULL;

	if (!text)
		return _wcsdup(L"");

	if (!SessionId)
	{
		extern HINTERNET hHttpSession[3];
		if (HINTERNET hConnect = WinHttpConnect(hHttpSession[impersonateIE], L"translate.yandex.com", port, 0))
		{
			if (char *data = DownloadFile(hConnect, L"/", port == 443 ? WINHTTP_FLAG_SECURE : 0))
			{
				if (char *p = strstr(data, "SID: '"))
				{
					p += 6;
					if (char *e = strchr(p, '\''))
					{
						*e = 0;
						char *ns = p;
						for (;;)
						{
							char *ne = strchr(ns, '.');
							if (!ne)
								ne = e;
							for (char *n = ne - 1; n > ns;)
							{
								char t = *ns;
								*ns++ = *n;
								*n-- = t;
							}
							if (ne == e)
								break;
							ns = ne + 1;
						}
						SessionId = strdup(p);
					}
				}
				free(data);
			}
			WinHttpCloseHandle(hConnect);
		}
		if (!SessionId)
			SessionId = strdup("");
	}
	size_t len = wcslen(path) + strlen(SessionId) + 15;
	wchar_t *data = (wchar_t*)malloc(len*sizeof(wchar_t));
	if (data)
		swprintf(data, len, path, srcString, dstString, SessionId, counter++);
	return data;
}

char* YandexWindow::GetLangIdString(Language lang, int src)
{
	switch (lang)
	{
		case LANGUAGE_AUTO:
			return "";
		case LANGUAGE_English:
			return "en";
		case LANGUAGE_Japanese:
			return "ja";

		case LANGUAGE_Afrikaans:
			return "af";
		case LANGUAGE_Albanian:
			return "sq";
		case LANGUAGE_Arabic:
			return "ar";
		//case LANGUAGE_Armenian:
		//	return "hy";
		//case LANGUAGE_Azerbaijani:
		//	return "az";
		//case LANGUAGE_Bashkir:
		//	return "ba";
		//case LANGUAGE_Basque:
		//	return "eu";
		case LANGUAGE_Belarusian:
			return "be";
		case LANGUAGE_Bengali:
			return "bn";
		case LANGUAGE_Bosnian:
			return "bs";
		case LANGUAGE_Bulgarian:
			return "bg";
		case LANGUAGE_Catalan:
			return "ca";
		//case LANGUAGE_Cebuano:
		//	return "ceb";
		case LANGUAGE_Chinese_Simplified:
		case LANGUAGE_Chinese_Traditional:
			return "zh";
		case LANGUAGE_Croatian:
			return "hr";
		case LANGUAGE_Czech:
			return "cs";
		case LANGUAGE_Danish:
			return "da";
		case LANGUAGE_Dutch:
			return "nl";
		//case LANGUAGE_Elvish_(Sindarin):
		//	return "sjn";
		case LANGUAGE_Esperanto:
			return "eo";
		case LANGUAGE_Estonian:
			return "et";
		case LANGUAGE_Finnish:
			return "fi";
		case LANGUAGE_French:
			return "fr";
		case LANGUAGE_Galician:
			return "gl";
		//case LANGUAGE_Georgian:
		//	return "ka";
		case LANGUAGE_German:
			return "de";
		case LANGUAGE_Greek:
			return "el";
		//case LANGUAGE_Gujarati:
		//	return "gu";
		//case LANGUAGE_Haitian:
		//	return "ht";
		case LANGUAGE_Hebrew:
			return "he";
		//case LANGUAGE_Hill_Mari:
		//	return "mrj";
		case LANGUAGE_Hindi:
			return "hi";
		case LANGUAGE_Hungarian:
			return "hu";
		case LANGUAGE_Icelandic:
			return "is";
		case LANGUAGE_Indonesian:
			return "id";
		case LANGUAGE_Irish:
			return "ga";
		case LANGUAGE_Italian:
			return "it";
		//case LANGUAGE_Javanese:
		//	return "jv";
		//case LANGUAGE_Kannada:
		//	return "kn";
		//case LANGUAGE_Kazakh:
		//	return "kk";
		//case LANGUAGE_Kirghiz:
		//	return "ky";
		case LANGUAGE_Korean:
			return "ko";
		case LANGUAGE_Latin:
			return "la";
		case LANGUAGE_Latvian:
			return "lv";
		case LANGUAGE_Lithuanian:
			return "lt";
		case LANGUAGE_Macedonian:
			return "mk";
		//case LANGUAGE_Malagasy:
		//	return "mg";
		case LANGUAGE_Malay:
			return "ms";
		//case LANGUAGE_Malayalam:
		//	return "ml";
		case LANGUAGE_Maltese:
			return "mt";
		//case LANGUAGE_Maori:
		//	return "mi";
		//case LANGUAGE_Marathi:
		//	return "mr";
		//case LANGUAGE_Mari:
		//	return "mhr";
		//case LANGUAGE_Mongolian:
		//	return "mn";
		//case LANGUAGE_Nepali:
		//	return "ne";
		case LANGUAGE_Norwegian:
			return "no";
		case LANGUAGE_Persian:
			return "fa";
		case LANGUAGE_Polish:
			return "pl";
		case LANGUAGE_Portuguese:
			return "pt";
		//case LANGUAGE_Punjabi:
		//	return "pa";
		case LANGUAGE_Romanian:
			return "ro";
		case LANGUAGE_Russian:
			return "ru";
		//case LANGUAGE_Scottish_Gaelic:
		//	return "gd";
		case LANGUAGE_Serbian:
			return "sr";
		//case LANGUAGE_Sinhalese:
		//	return "si";
		case LANGUAGE_Slovak:
			return "sk";
		case LANGUAGE_Slovenian:
			return "sl";
		case LANGUAGE_Spanish:
			return "es";
		//case LANGUAGE_Sundanese:
		//	return "su";
		case LANGUAGE_Swahili:
			return "sw";
		case LANGUAGE_Swedish:
			return "sv";
		//case LANGUAGE_Tagalog:
		//	return "tl";
		//case LANGUAGE_Tajik:
		//	return "tg";
		//case LANGUAGE_Tamil:
		//	return "ta";
		//case LANGUAGE_Tatar:
		//	return "tt";
		//case LANGUAGE_Telugu:
		//	return "te";
		case LANGUAGE_Thai:
			return "th";
		case LANGUAGE_Turkish:
			return "tr";
		//case LANGUAGE_Udmurt:
		//	return "udm";
		case LANGUAGE_Ukrainian:
			return "uk";
		case LANGUAGE_Urdu:
			return "ur";
		//case LANGUAGE_Uzbek:
		//	return "uz";
		case LANGUAGE_Vietnamese:
			return "vi";
		case LANGUAGE_Welsh:
			return "cy";
		case LANGUAGE_Yiddish:
			return "yi";

		default:
			return 0;
	}
}
