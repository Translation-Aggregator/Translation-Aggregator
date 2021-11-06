#include <Shared/Shrink.h>
#include "SysTranWindow.h"
#include "../../util/HttpUtil.h"

#include <locale>
#include <codecvt>
#include <3rdParty/PicoSHA2/picosha2.h>

SysTranWindow::SysTranWindow() : HttpWindow(L"SysTran", L"https://translate.systran.net")
{
	host = L"translate.systran.net";
	path = L"/translate/html";
	m_cookiePath = L"/translationTools/text";
	port = 443;
	postPrefixTemplate = "{\"profileId\":null,\"owner\":null,\"domain\":null,\"size\":\"M\",\"input\":\"%s\",\"source\":\"%s\",\"target\":\"%s\"}";
	m_baseRequestHeaders = L"X-Requested-With: XMLHttpRequest\nContent-Type: application/json";
	requestHeaders = nullptr;
	//	requestHeaders = nullptr;
	impersonateIE = 1;
	dontEscapeRequest = true;
}

#define TARGET_START L"\"target\":\"\\n\\n"

wchar_t *SysTranWindow::FindTranslatedText(wchar_t* html)
{
	html = wcsstr(html, L"<meta ");
	if (!html)
		return NULL;
	size_t len = wcslen(html);
	if (html[len - 1] == ';')
		html[len - 1] = 0;
	UnescapeHtml(html, 0);
	html = wcsstr(html, TARGET_START) + wcslen(TARGET_START);
	wchar_t* end = wcsstr(html, L"\",\"selectedRoutes");
	end[0] = 0;
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

	char *pSrcString;
	char *pDstString;
	if (!(pSrcString = GetLangIdString(src, 1)) || !(pDstString = GetLangIdString(dst, 0)) || !strcmp(pSrcString, pDstString))
		return 0;

	std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;

	if (m_cookie.empty())
	{
		GetCookie();
		m_baseRequestHeaders += L"\nCookie: " + m_cookie;

		//		m_baseRequestHeaders = L"X-Requested-With: XMLHttpRequest\nContent-Type: application/json\nCookie: lang=en; ses.sid=s%3AN0exZKvgp7PyB2YnBR0I2cSkUj3Od32G.DbvLd%2B2bzR2aUp1Mb2k3TL97aONazub%2F%2FgUg7WVuveY";

		getToken();
		m_baseRequestHeaders += L"\nX-CSRF-Token: " + converter.from_bytes(m_token);
		//		m_baseRequestHeaders += L"\nX-CSRF-Token: SffUjstv-6lfEZZV9lMDYiJ-n0evPnvg9-Vs";

	}

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
	char *data = (char*)calloc(len, sizeof(char));
	char *d = data;

	for (p = text; *p; p++)
		switch (*p)
		{
			case '\n': strcpy(d, "<br>"); d += 4; break;
			case '<': strcpy(d, "&lt;"); d += 4; break;
			case '>': strcpy(d, "&gt;"); d += 4; break;
			case '&': strcpy(d, "&amp;"); d += 5; break;
			case '"': strcpy(d, "&quot;"); d += 6; break;
			default: *d++ = *p;
		}

	std::string seq = m_token + "." + std::string(pSrcString) + "." + std::string(pDstString) + "." + std::string(data);
	std::string hash;
	picosha2::hash256_hex_string(seq, hash);


	m_requestHeaders = m_baseRequestHeaders + L"\nx-translation-signature: " + converter.from_bytes(hash);
	//	m_requestHeaders = L"X-Requested-With: XMLHttpRequest\nX-CSRF-Token: SffUjstv-6lfEZZV9lMDYiJ-n0evPnvg9-Vs\nContent-Type: application/json\nCookie: lang=en; ses.sid=s%3AN0exZKvgp7PyB2YnBR0I2cSkUj3Od32G.DbvLd%2B2bzR2aUp1Mb2k3TL97aONazub%2F%2FgUg7WVuveY\nx-translation-signature: b53bf3d37a8d53de3bf8c8673aba6d654e8ef25cccf5d4d551e52113b11d00ff";

	size_t size = strlen(data) + strlen(pSrcString) + strlen(pDstString) + strlen(postPrefixTemplate);

	char *pJson = reinterpret_cast<char*>(calloc(size, sizeof(char)));

	snprintf(pJson, size * sizeof(char), postPrefixTemplate, data, pSrcString, pDstString);

	free(data);

	delete requestHeaders;
	requestHeaders = reinterpret_cast<wchar_t*>(calloc(m_requestHeaders.size() + 1, sizeof(wchar_t)));
	wcscpy(requestHeaders, m_requestHeaders.c_str());

	return pJson;
}

#define X_TOKEN "'X-CSRF-Token': '"

void SysTranWindow::getToken()
{
	extern HINTERNET hHttpSession[3];
	DWORD dwSize = 0;
	LPSTR pszOutBuffer = "";
	DWORD dwDownloaded = 0;
	std::string site;

	if (HINTERNET hConnect = WinHttpConnect(hHttpSession[impersonateIE], host, port, 0))
	{
		if (HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", L"/translationTools/text", 0, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, port == 443 ? WINHTTP_FLAG_SECURE : 0))
		{
			// TODO: error here, no cookie forwarding
			if (WinHttpSendRequest(hRequest, m_baseRequestHeaders.c_str(), m_baseRequestHeaders.length(), WINHTTP_NO_REQUEST_DATA, 0, 0, 0))
			{
				if (WinHttpReceiveResponse(hRequest, NULL))
				{
					DWORD dwSize = 0;
					DWORD dwDownloaded = 0;
					do
					{
						// Check for available data.
						dwSize = 0;
						if (WinHttpQueryDataAvailable(hRequest, &dwSize))
						{
							// Allocate space for the buffer.
							LPSTR pszOutBuffer = new char[dwSize + 1];
							if (pszOutBuffer)
							{
								// Read the Data.
								ZeroMemory(pszOutBuffer, dwSize + 1);

								if (WinHttpReadData(hRequest, (LPVOID)pszOutBuffer, dwSize, &dwDownloaded))
									site.append(pszOutBuffer);

								// Free the memory allocated to the buffer.
								delete[] pszOutBuffer;
							}
						}
					} while (dwSize > 0);
				}
			}
			int err = GetLastError();
			WinHttpCloseHandle(hRequest);
		}
		WinHttpCloseHandle(hConnect);
	}

	if (!site.empty())
	{
		uint32_t posS = site.find(X_TOKEN) + std::string(X_TOKEN).length();
		uint32_t posE = site.find("'", posS);
		m_token = site.substr(posS, posE - posS);
	}
}
