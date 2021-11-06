#include <Shared/Shrink.h>
#include "BaiduWindow.h"
#include <cstdint>


int64_t n(int64_t r, std::string o)
{
	for(size_t t = 0; t < o.length() - 2; t += 3)
	{
		int64_t a = o.at(t + 2);
		a = a >= 'a' ? a - 87 : a - '0';
		a = '+' == o.at(t + 1) ? r >> a : r << (int32_t)a;
		r = '+' == o.at(t) ? r + a & 4294967295 : r ^ a;
	}

	return r;
}

std::wstring s2ws(const std::string& str)
{
	int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
	std::wstring wstrTo(size_needed, 0);
	MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
	return wstrTo;
}

std::string e(const char *pStr)
{
	std::string str(pStr);
	std::wstring r = s2ws(str);
	size_t t = r.length();
	if(t > 30)
		r = L"" + r.substr(0, 10) + r.substr((t / 2) - 5, 10) + r.substr(r.length() - 10, 10);

	int64_t m = 320305;
	int64_t s = 131321201;
	std::vector<int64_t> S;

	for(size_t v = 0; v < r.length(); v++)
	{
		int64_t A = (int32_t)r.at(v);
		if(128 > A)
			S.push_back(A);
		else
		{
			if(2048 > A)
				S.push_back(A >> 6 | 192);
			else if(55296 == (64512 & A) && v + 1 < r.length() && 56320 == (64512 & r.at(v + 1)))
			{
				A = 65536 + ((1023 & A) << 10) + (1023 & r.at(++v));
				S.push_back(A >> 18 | 240);
				S.push_back(A >> 12 & 63 | 128);
			}
			else
			{
				S.push_back(A >> 12 | 224);
				S.push_back(A >> 6 & 63 | 128);
			}

			S.push_back(63 & A | 128);
		}
	}

	std::string F = "+-a^+6";
	std::string D = "+-3^+b+-f";
	int64_t p = m;

	for(size_t b = 0; b < S.size(); b++)
	{
		p += S[b];
		p = n(p, F);
	}

	p = n(p, D);
	p ^= s;
	if(0 > p)
		p = (2147483647 & p) + 2147483648;

	p %= (int64_t)1e6;

	return std::to_string(p) + "." + std::to_string(p ^ m);
}


BaiduWindow::BaiduWindow() : HttpWindow(L"Baidu", L"https://fanyi.baidu.com/")
{
	host = L"fanyi.baidu.com";
	path = L"/v2transapi";
	port = 443;
	postPrefixTemplate = "from=%s&to=%s&query=%s&token=%s&sign=%s";
	requestHeaders = L"Content-Type: application/x-www-form-urlencoded; charset=UTF-8;";
	dontEscapeRequest = true;
	memset(m_token, 0, 33);
}

BaiduWindow::~BaiduWindow()
{
}

wchar_t *BaiduWindow::FindTranslatedText(wchar_t* html)
{
	if(ParseJSON(html, L"{\"dst\":\"", L"{\"dst\":\""))
		return html;
	if(ParseJSON(html, L"\"result\":\"", NULL))
		if(ParseJSON(html, L"\"cont\": {\"", NULL))
			return html;
	return NULL;
}

void BaiduWindow::GetToken()
{
	if(m_token[0] != 0) return;

	extern HINTERNET hHttpSession[3];
	if(HINTERNET hConnect = WinHttpConnect(hHttpSession[impersonateIE], host, port, 0))
	{
		if(HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", L"/", 0, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, (port == 443 ? WINHTTP_FLAG_SECURE : 0)))
		{
			if(WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0))
			{
				if(WinHttpReceiveResponse(hRequest, NULL))
				{
					DWORD dwSize = 0;
					DWORD dwDownloaded = 0;
					do
					{
						// Check for available data.
						dwSize = 0;
						if(WinHttpQueryDataAvailable(hRequest, &dwSize))
						{
							// Allocate space for the buffer.
							LPSTR pszOutBuffer = new char[dwSize + 1];
							if(pszOutBuffer)
							{
								// Read the Data.
								ZeroMemory(pszOutBuffer, dwSize + 1);

								if(WinHttpReadData(hRequest, (LPVOID)pszOutBuffer, dwSize, &dwDownloaded))
								{
									const char *pPos = strstr(pszOutBuffer, "token: '");
									if(pPos != NULL)
									{
										memcpy(m_token, pPos + strlen("token: '"), 32);
										delete[] pszOutBuffer;
										WinHttpCloseHandle(hRequest);
										WinHttpCloseHandle(hConnect);
										return;
									}
								}

								// Free the memory allocated to the buffer.
								delete[] pszOutBuffer;
							}
						}
					} while(dwSize > 0);
				}
			}
			WinHttpCloseHandle(hRequest);
		}
		WinHttpCloseHandle(hConnect);
	}
}


char *BaiduWindow::GetLangIdString(Language lang, int src)
{
	switch(lang)
	{
		case LANGUAGE_English:
			return "en";
		case LANGUAGE_Japanese:
			return "jp";
		case LANGUAGE_Chinese_Simplified:
			return "zh";
		case LANGUAGE_Thai:
			return "th";
		case LANGUAGE_Portuguese:
			return "pt";
		case LANGUAGE_Spanish:
			return "spa";
		default:
			return 0;
	}
}

//postPrefixTemplate = "ie=utf-8&from=%s&to=%s&query=%s&token=%s&sign=%s";

char *BaiduWindow::GetTranslationPrefix(Language src, Language dst, const char *text)
{
	if(src == LANGUAGE_Chinese_Traditional)
		src = LANGUAGE_Chinese_Simplified;
	if(dst == LANGUAGE_Chinese_Traditional)
		dst = LANGUAGE_Chinese_Simplified;
	if(src != LANGUAGE_English && dst != LANGUAGE_English &&
		(src != LANGUAGE_Japanese || dst != LANGUAGE_Chinese_Simplified) &&
	   (dst != LANGUAGE_Japanese || src != LANGUAGE_Chinese_Simplified))
		return 0;

	char *pSrcString;
	char *pDstString;
	if(!(pSrcString = GetLangIdString(src, 1)) || !(pDstString = GetLangIdString(dst, 0)) || !strcmp(pSrcString, pDstString))
		return 0;

	if(!text)
		return (char*)1;

	if(m_cookie.empty())
	{
		GetCookie();
		wchar_t *pBuf = (wchar_t*)malloc((wcslen(requestHeaders) + 1) * sizeof(wchar_t) + (m_cookie.length() + 1) * sizeof(wchar_t));
		swprintf(pBuf, L"%s%s", requestHeaders, m_cookie.c_str());
		requestHeaders = pBuf;
	}
	if(m_token[0] == 0)
		GetToken();

	std::string signStr = e(text);

	char *out = (char*)malloc(strlen(postPrefixTemplate) + strlen(pSrcString) + strlen(pDstString) + strlen(text) + 33 + signStr.length() + 1);
	sprintf(out, postPrefixTemplate, pSrcString, pDstString, text, m_token, signStr.c_str());
	return out;
}
