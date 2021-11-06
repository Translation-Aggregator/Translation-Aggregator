#include <Shared/Shrink.h>
#include "GoogleWindow.h"
#include <Shared/StringUtil.h>

#include <algorithm>
#include <sstream>
#include <fstream>

#include <json.hpp>

using json = nlohmann::json;

#define TKK

#ifdef TKK
int64_t vi(int64_t r, std::string o)
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

std::string tk(const wchar_t *pStr)
{
	std::wstring r(pStr);

	int64_t m = 425635;
	int64_t s = 1953544246;
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
		p = vi(p, F);
	}

	p = vi(p, D);
	p ^= s;
	if(0 > p)
		p = (2147483647 & p) + 2147483648;

	p %= (int64_t)1e6;

	return std::to_string(p) + "." + std::to_string(p ^ m);
}
#endif

// From: https://stackoverflow.com/a/24315631
std::string ReplaceAll(std::string str, const std::string& from, const std::string& to)
{
	if (from.empty()) return str;

	size_t start_pos = 0;
	while ((start_pos = str.find(from, start_pos)) != std::string::npos)
	{
		str.replace(start_pos, from.length(), to);
		start_pos += to.length(); // Handles case where 'to' is a substring of 'from'
	}
	return str;
}


char* GoogleEscapeParamA(const char* src, int len)
{
	char* out = (char*)malloc(sizeof(char) * (3 * strlen(src) + 1));
	char* dst = out;
	while (len)
	{
		len--;
		char c = *src;
		if (c <= 0x26 || c == '+' || c == ',' || (0x3A <= c && c <= 0x40) || c == '\\' || (0x5B <= c && c <= 0x60) || (0x7B <= c && c <= 0x7E))
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

GoogleWindow::GoogleWindow() : HttpWindow(L"Google", L"http://translate.google.com/"), m_pResult(nullptr)
{
	host = L"translate.google.com";
#ifndef TKK
	path = L"/translate_a/single?client=gtx&dt=t&sl=%hs&tl=%hs&ie=UTF-8&oe=UTF-8&q=%s";
#else
	path = L"/_/TranslateWebserverUi/data/batchexecute?rpcids=MkEWBc&f.sid=%lld&bl=boq_translate-webserver_20211006.11_p0&hl=en-US&soc-app=1&soc-platform=1&soc-device=1&_reqid=%d&rt=c";
//	path = L"/translate_a/single?client=webapp&dt=t&sl=%hs&tl=%hs&ie=UTF-8&oe=UTF-8&tk=%hs&q=%s";
#endif
	port = 443;
	dontEscapeRequest = true;
	impersonateIE = 2;
	postPrefixTemplate = "[[[\"MkEWBc\",\"[[\\\"%s\\\",\\\"%s\\\",\\\"%s\\\",true],[null]]\",null,\"generic\"]]]";
	requestHeaders = L"Content-Type: application/x-www-form-urlencoded;charset=utf-8;";
	m_pOrgRequestHeaders = L"Content-Type: application/x-www-form-urlencoded; charset=UTF-8;";
	m_tlCnt = 0;
	m_cookiePath = L"/";
}

GoogleWindow::~GoogleWindow()
{
}

wchar_t *GoogleWindow::GetTranslationPath(Language src, Language dst, const wchar_t *text)
{
	char *srcString, *dstString;
	if (!(srcString = GetLangIdString(src, 1)) || !(dstString = GetLangIdString(dst, 0)) || !strcmp(srcString, dstString))
		return 0;

	if (!text)
		return _wcsdup(L"");

	//std::wstring eText = L"";

	//std::vector<char> vec((wcslen(text) + 1) * 4);
	//WideCharToMultiByte(CP_UTF8, 0, text, -1, vec.data(), vec.size(), nullptr, nullptr);
	//std::string str(vec.data());

	//for (const unsigned char& c : str)
	//{
	//	wchar_t e[4] = { 0 };
	//	swprintf_s<4>(e, L"%%%02X", static_cast<int32_t>(c));
	//	eText.append(e);
	//}

	//const int len = wcslen(path) + strlen(srcString) + strlen(dstString) + eText.length() + 1 + 16;

	//if(m_cookie.empty() || m_tlCnt >= 100)
	//{
	//	//free(requestHeaders);
	//	m_tlCnt = 0;
	//	GetCookie();
	//	int32_t bufSize = (wcslen(m_pOrgRequestHeaders) + 10) * sizeof(wchar_t) + (m_cookie.length() + 1) * sizeof(wchar_t);
	//	wchar_t *pBuf = reinterpret_cast<wchar_t*>(malloc(bufSize));
	//	swprintf_s(pBuf, bufSize, L"%s;Cookie: %s", m_pOrgRequestHeaders, m_cookie.c_str());
	//	requestHeaders = pBuf;
	//}

	m_tlCnt++;

#ifndef TKK
	swprintf(out, len, path, srcString, dstString, etext);
#else

	//std::string tkStr = tk(text);

	constexpr int64_t sid = 8313535539834510881;
	constexpr int32_t reqid = 53678;

	const int len = wcslen(path) + 64; // 64 - more than enough space to store the sid and reqid
	wchar_t *out = static_cast<wchar_t*>(calloc(len, sizeof(wchar_t)));
	swprintf(out, len, path, sid, reqid);
	//swprintf(out, len, path, srcString, dstString, tkStr.c_str(), eText.c_str());
#endif
	return out;
}

char* GoogleWindow::GetTranslationPrefix(Language src, Language dst, const char* text)
{
	if (!postPrefixTemplate)
		return 0;

	char* srcString, * dstString;
	if (!(srcString = GetLangIdString(src, 1)) || !(dstString = GetLangIdString(dst, 0)) || !strcmp(srcString, dstString))
		return 0;

	if (!text)
		return reinterpret_cast<char*>(1);

	std::string rawText(text);
	rawText = ReplaceAll(rawText, "\"", "\\\\\\\"");
	rawText = ReplaceAll(rawText, "\n", "\\\\n");
	rawText = ReplaceAll(rawText, "\r", "");

	int32_t len = strlen(postPrefixTemplate) + rawText.length() + strlen(srcString) + strlen(dstString) + 1;
	char* pOut = static_cast<char*>(calloc(len, sizeof(char)));
	snprintf(pOut, len, postPrefixTemplate, rawText.c_str(), srcString, dstString);

	rawText = std::string(pOut);
	char* pEsc = GoogleEscapeParamA(rawText.c_str(), rawText.length());
	rawText = std::string(pEsc);
	free(pEsc);
	free(pOut);

	rawText.insert(0, "f.req=");
	rawText.append("&");
	len = rawText.length() + 1;
	pOut = static_cast<char*>(calloc(len, sizeof(char)));

	snprintf(pOut, len, "%s", rawText.c_str());

	//free(text2);
	return pOut;
}


wchar_t *GoogleWindow::FindTranslatedText(wchar_t* html)
{
#if 0
	if (html[0] != '"' || html[wcslen(html) - 1] != '"')
		return NULL;
	wchar_t *ps = html + 1;
	ptrdiff_t d = -1;
	for (; *ps; ps++)
	{
		if (*ps == '\\')
			if (d--, *++ps == 'n')
			{
				ps[d] = '\n';
				continue;
			}
		ps[d] = *ps;
	}
	ps[d - 1] = 0;
	return html;
#else
	//if ((html = wcsstr(html, L"[[[\"")))
	//{
	//	html += 4;
	//	ParseJSON(html, NULL, L"]\n,[\"", true);
	//	return html;
	//}

	if (m_pResult) delete[] m_pResult;

	std::string str;
	std::string out = "";

	try
	{
		std::vector<char> vec((wcslen(html) + 1) * 4);
		WideCharToMultiByte(CP_UTF8, 0, html, -1, vec.data(), vec.size(), nullptr, nullptr);
		str = std::string(vec.data());
		int32_t p = str.find("[");
		str = str.substr(p);
		p = str.find("\n");
		str = str.substr(0, p);
		json j = json::parse(str);
		j = json::parse(std::string(j[0][2]));
		for (const auto& entry : j[1][0][0][5])
			out += entry[0];
	}
	catch (const std::exception& e)
	{
		std::ofstream err("error2.txt");
		err << e.what() << std::endl;
		err << str << std::endl;
		err.close();
	}


	std::wstring result = ToWstring(out);

//	m_pResult = new wchar_t[result.size() * 2 + 1]();
//	wsprintf(m_pResult, L"%s", result.c_str());
	m_pResult = _wcsdup(result.c_str());


	return m_pResult;
#endif
}

char* GoogleWindow::GetLangIdString(Language lang, int src)
{
	if (src && lang == LANGUAGE_Chinese_Traditional)
		lang = LANGUAGE_Chinese_Simplified;
	switch (lang)
	{
		case LANGUAGE_AUTO:
			return "auto";
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
		//case LANGUAGE_Chichewa:
		//	return "ny";
		case LANGUAGE_Chinese_Simplified:
			return "zh-CN";
		case LANGUAGE_Chinese_Traditional:
			return "zh-TW";
		case LANGUAGE_Croatian:
			return "hr";
		case LANGUAGE_Czech:
			return "cs";
		case LANGUAGE_Danish:
			return "da";
		case LANGUAGE_Dutch:
			return "nl";
		case LANGUAGE_Esperanto:
			return "eo";
		case LANGUAGE_Estonian:
			return "et";
		case LANGUAGE_Filipino:
			return "tl";
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
		case LANGUAGE_Haitian_Creole:
			return "ht";
		case LANGUAGE_Hausa:
			return "ha";
		case LANGUAGE_Hebrew:
			return "iw";
		case LANGUAGE_Hindi:
			return "hi";
		//case LANGUAGE_Hmong:
		//	return "hmn";
		case LANGUAGE_Hungarian:
			return "hu";
		case LANGUAGE_Icelandic:
			return "is";
		//case LANGUAGE_Igbo:
		//	return "ig";
		case LANGUAGE_Indonesian:
			return "id";
		case LANGUAGE_Irish:
			return "ga";
		case LANGUAGE_Italian:
			return "it";
		//case LANGUAGE_Javanese:
		//	return "jw";
		//case LANGUAGE_Kannada:
		//	return "kn";
		//case LANGUAGE_Kazakh:
		//	return "kk";
		//case LANGUAGE_Khmer:
		//	return "km";
		case LANGUAGE_Korean:
			return "ko";
		//case LANGUAGE_Lao:
		//	return "lo";
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
		//case LANGUAGE_Mongolian:
		//	return "mn";
		//case LANGUAGE_Myanmar:
		//	return "my";
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
		case LANGUAGE_Serbian:
			return "sr";
		//case LANGUAGE_Sesotho:
		//	return "st";
		//case LANGUAGE_Sinhala:
		//	return "si";
		case LANGUAGE_Slovak:
			return "sk";
		case LANGUAGE_Slovenian:
			return "sl";
		case LANGUAGE_Somali:
			return "so";
		case LANGUAGE_Spanish:
			return "es";
		//case LANGUAGE_Sundanese:
		//	return "su";
		case LANGUAGE_Swahili:
			return "sw";
		case LANGUAGE_Swedish:
			return "sv";
		//case LANGUAGE_Tajik:
		//	return "tg";
		//case LANGUAGE_Tamil:
		//	return "ta";
		//case LANGUAGE_Telugu:
		//	return "te";
		case LANGUAGE_Thai:
			return "th";
		case LANGUAGE_Turkish:
			return "tr";
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
		//case LANGUAGE_Yoruba:
		//	return "yo";
		case LANGUAGE_Zulu:
			return "zu";
		default:
			return 0;
	}
}
