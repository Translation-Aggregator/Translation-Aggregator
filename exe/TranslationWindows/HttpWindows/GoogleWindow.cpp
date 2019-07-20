#include <Shared/Shrink.h>
#include "GoogleWindow.h"
#include <Shared/StringUtil.h>
#include <ctime>
#include <string>
#include <optional>
#include <atomic>
#include <memory>
#include <functional>
#include <regex>
#include <cstdio>
#include <locale>
#include <codecvt>

#define TKK


#ifdef TKK

//Code pulled from Textractor, which is under GPLv3 license

static unsigned tkk_id = 0;

inline std::wstring StringToWideString(const std::string& text)
{
	std::vector<wchar_t> buffer(text.size() + 1);
	MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, buffer.data(), buffer.size());
	return buffer.data();
}

template <auto F>
struct Functor
{
	template <typename... Args>
	auto operator()(Args&&... args) const { return std::invoke(F, std::forward<Args>(args)...); }
};

template <typename HandleCloser = Functor<CloseHandle>>
class AutoHandle
{
public:
	AutoHandle(HANDLE h) : h(h) {}
	operator HANDLE() { return h.get(); }
	PHANDLE operator&() { static_assert(sizeof(*this) == sizeof(HANDLE)); return (PHANDLE)this; }
	operator bool() { return h.get() != NULL && h.get() != INVALID_HANDLE_VALUE; }

private:
	struct HandleCleaner { void operator()(void* h) { if (h != INVALID_HANDLE_VALUE) HandleCloser()(h); } };
	std::unique_ptr<void, HandleCleaner> h;
};

inline std::string WideStringToString(const std::wstring& text)
{
	std::vector<char> buffer((text.size() + 1) * 4);
	WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, buffer.data(), buffer.size(), nullptr, nullptr);
	return buffer.data();
}
using InternetHandle = AutoHandle<Functor<WinHttpCloseHandle>>;

inline std::optional<std::wstring> ReceiveHttpRequest(HINTERNET request)
{
	WinHttpReceiveResponse(request, NULL);
	std::string data;
	DWORD dwSize, dwDownloaded;
	do
	{
		dwSize = 0;
		WinHttpQueryDataAvailable(request, &dwSize);
		if (!dwSize) break;
		std::vector<char> buffer(dwSize);
		WinHttpReadData(request, buffer.data(), dwSize, &dwDownloaded);
		data.append(buffer.data(), dwDownloaded);
	} while (dwSize > 0);

	if (data.empty()) return {};
	return StringToWideString(data);
}

static HINTERNET internet = NULL;

std::wstring tk(const wchar_t *pStr, std::string translateFrom, std::string translateTo)
{
	if (!tkk_id)
		if (!internet) internet = WinHttpOpen(L"Mozilla/5.0 TranslationAggregator", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, NULL, NULL, 0);
			if (InternetHandle connection = WinHttpConnect(internet, L"translate.google.com", INTERNET_DEFAULT_HTTPS_PORT, 0))
				if (InternetHandle request = WinHttpOpenRequest(connection, L"GET", L"/", NULL, NULL, NULL, WINHTTP_FLAG_SECURE))
					if (WinHttpSendRequest(request, NULL, 0, NULL, 0, 0, NULL))
						if (auto response = ReceiveHttpRequest(request))
							if (std::wsmatch results; std::regex_search(response.value(), results, std::wregex(L"(\\d{7,})'"))) tkk_id = stoll(results[1]);
	if (!tkk_id)
		return L"";
	std::wstring escapedText;
	unsigned a = _time64(NULL) / 3600, b = a; // <- the first part of TKK
	for (unsigned char ch : WideStringToString(pStr))
	{
		wchar_t escapedChar[4] = {};
		swprintf_s<4>(escapedChar, L"%%%02X", (int)ch);
		escapedText += escapedChar;
		a += ch;
		a += a << 10;
		a ^= a >> 6;
	}
	a += a << 3;
	a ^= a >> 11;
	a += a << 15;
	a ^= tkk_id;
	a %= 1000000;
	b ^= a;

	return L"/translate_a/single?client=webapp&dt=ld&dt=rm&dt=t&sl=" + StringToWideString(translateFrom) + L"&tl=" + StringToWideString(translateTo) + L"&ie=UTF-8&oe=UTF-8&tk=" + std::to_wstring(a) + L"." + std::to_wstring(b) + L"&q=" + escapedText;
}
#endif

GoogleWindow::GoogleWindow() : HttpWindow(L"Google", L"https://translate.google.com/")
{
	host = L"translate.google.com";
#ifndef TKK
	path = L"/translate_a/single?client=gtx&dt=t&sl=%hs&tl=%hs&ie=UTF-8&oe=UTF-8&q=%s";
#else
	path = L"/translate_a/single?client=webapp&dt=t&sl=%hs&tl=%hs&ie=UTF-8&oe=UTF-8&tk=%hs&q=%s";
#endif
	port = 443;
	dontEscapeRequest = true;
	// m_pCookie = nullptr;
	impersonateIE = 0;
	// m_pOrgRequestHeaders = requestHeaders;
	// m_tlCnt = 0;
}

GoogleWindow::~GoogleWindow()
{
	free(m_pCookie);
}

wchar_t *GoogleWindow::GetTranslationPath(Language src, Language dst, const wchar_t *text)
{
	char *srcString, *dstString;
	if (!(srcString = GetLangIdString(src, 1)) || !(dstString = GetLangIdString(dst, 0)) || !strcmp(srcString, dstString))
		return 0;

	if (!text)
		return _wcsdup(L"");

	// wchar_t *HttpEscapeParamW(const wchar_t *src, int len);
	// wchar_t *etext = HttpEscapeParamW(text, wcslen(text));
	// int len = wcslen(path) + strlen(srcString) + strlen(dstString) + wcslen(etext) + 1 + 16;
	// wchar_t *out = (wchar_t*)malloc(len*sizeof(wchar_t));

	// if(!m_pCookie || m_tlCnt >= 100)
	// {
	// 	free(m_pCookie);
	// 	m_tlCnt = 0;
	// 	GetCookie();
	// 	wchar_t *pBuf = (wchar_t*)malloc((wcslen(m_pOrgRequestHeaders) + 10) * sizeof(wchar_t) + (wcslen(m_pCookie) + 1) * sizeof(wchar_t));
	// 	swprintf(pBuf, L"%s;Cookie: %s", m_pOrgRequestHeaders, m_pCookie);
	// 	free(m_pCookie);
	// 	m_pCookie = pBuf;
	// 	requestHeaders = m_pCookie;
	// }

	// m_tlCnt++;

#ifndef TKK
	swprintf(out, len, path, srcString, dstString, etext);
#else

	// std::string tkStr = ;

	// if (tkStr != "")
	// 	swprintf(out, len, path, srcString, dstString, tkStr.c_str(), etext);
	// else
	// 	return _wcsdup(L"");
    using convert_type = std::codecvt_utf8<wchar_t>;
    std::wstring_convert<convert_type, wchar_t> converter;

	return _wcsdup(tk(text, srcString, dstString).c_str());
#endif
	// free(etext);
	// return out;
}

static bool IsHash(const std::wstring& result)
{
	return std::all_of(result.begin(), result.end(), [](auto ch) { return (ch >= L'0' && ch <= L'9') || (ch >= L'a' && ch <= L'z'); });
}

static std::wstring ReplaceString(std::wstring subject, const std::wstring& search, const std::wstring& replace) {
	size_t pos = 0;
	while ((pos = subject.find(search, pos)) != std::wstring::npos) {
		subject.replace(pos, search.length(), replace);
		pos += replace.length();
	}
	return subject;
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
	if (html[0] == L'[')
	{
		std::wstring response(html);
		std::wstring translation;
		for (std::wsmatch results; std::regex_search(response, results, std::wregex(L"\\[\"(.*?)\",[n\"]")); response = results.suffix())
			if (!IsHash(results[1])) translation += ReplaceString(std::wstring(results[1]), L"\\n", L"\n") + L" ";
		if (!translation.empty()) return const_cast<wchar_t*>(translation.c_str());
	}
	return NULL;
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
