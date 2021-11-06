#include <Shared/Shrink.h>
#include "DeepLWindow.h"
#include <Shared/StringUtil.h>

#include <json.hpp>

#include <vector>
#include <sstream>
#include <fstream>
#include <stdint.h>
#include <list>

using json = nlohmann::json;

std::list<uint32_t> newLines;

// .!?;":？。．！\u037e

const std::vector<std::vector<uint8_t>> SPLIT_SEQUENCES = {
	{0x0A}, // \n
	{0x2E}, // .
	{0x21}, // !
	{0x3F}, // ?
	{0x3B}, // ;
	{0x22}, // "
	{0x3A}, // :
	{0xEF, 0xBC, 0x9F}, // ？
	{0xE3, 0x80, 0x82}, // 。
	{0xEF, 0xBC, 0x8E}, // ．
	{0xEF, 0xBC, 0x81}, // ！
	{0xCD, 0xBE} // \u037e
};

bool containsSplitChar(const char* pStr, const uint32_t& strLen, uint32_t& seqLen)
{
	seqLen = 0;

	for (const std::vector<uint8_t>& seq : SPLIT_SEQUENCES)
	{
		if (strLen < seq.size()) continue;
		bool found = true;
		int32_t i = 0;
		for (const uint8_t& elem : seq)
		{
			if (static_cast<uint8_t>(pStr[i++]) != elem)
			{
				found = false;
				break;
			}
		}

		if (found)
		{
			seqLen = seq.size();
			return true;
		}
	}

	return false;
}

static inline std::vector<std::string> splitStringAtEndToken(const std::string& s)
{
	std::vector<std::string> split;
	std::string item;
	std::istringstream stream(s);
	uint32_t start = 0;
	uint32_t seqLen = 0;

	for (uint32_t i = 0; i < s.size(); i++)
	{
		if (containsSplitChar(&s.at(i), s.size() - i, seqLen))
		{
			split.push_back(s.substr(start, (i + seqLen) - start));

			if(seqLen == 1 && s.at(i) == '\n')
				newLines.push_back(split.size() - 1);


			start = i + seqLen;
			i = start;

			while (i < s.size() && s.at(i) == '\n')
			{
				newLines.push_back(split.size() - 1);
				start++;
				i++;
			}
		}
	}

	if(start != s.size())
		split.push_back(s.substr(start, std::string::npos));

	return split;
}


DeepLWindow::DeepLWindow() :
	HttpWindow(L"DeepL", L"https://www2.deepl.com/jsonrpc/"),
	m_pResult(nullptr)
{
	host = L"www2.deepl.com";
	path = L"/jsonrpc";
	postPrefixTemplate = "";

	port = 443;
	dontEscapeRequest = true;
	requestHeaders = L"Content-Type: application/json";
}

DeepLWindow::~DeepLWindow()
{}


char* DeepLWindow::GetTranslationPrefix(Language src, Language dst, const char* text)
{
	newLines.clear();
	char* srcString, * dstString;
	if (!(srcString = GetLangIdString(src, 1)) || !(dstString = GetLangIdString(dst, 0)) || !strcmp(srcString, dstString))
		return 0;

	if (!text)
		return (char*)1;

	std::string input = std::string(text);
	std::vector<std::string> split = splitStringAtEndToken(input);

	char* out = nullptr;

	try
	{
		json j = {
		{ "id", 1337 },
		{ "jsonrpc", "2.0" },
		{ "method", "LMT_handle_jobs"},
		{ "params", {
			{ "commonJobParams", {{"regionalVariant", "en-US"}}},
			{ "jobs", json::array() },
			{"lang", {
				{ "source_lang_computed", srcString },
				{ "target_lang", dstString },
				{ "user_preferred_langs", json::array({dstString, srcString}) },
			}},
			{ "priority", 1 },
			{"timestamp", 1615054788975}
		  }} };

		std::vector<std::string> before;

		for (std::size_t i = 0; i < split.size(); i++)
		{
			json job = "{\"kind\":\"default\",\"raw_en_sentence\":\"\",\"raw_en_context_before\":[],\"raw_en_context_after\":[],\"preferred_num_beams\":1}"_json;
			const std::string str = split.at(i);
			if (str.empty()) continue;

			job["raw_en_sentence"] = str;

			for (const std::string& s : before)
				job["raw_en_context_before"].push_back(s);

			if (i + 1 < split.size())
				job["raw_en_context_after"].push_back(split.at(i + 1));

			before.push_back(str);

			j["params"]["jobs"].push_back(job);
		}

		//std::ofstream out("test.txt");
		//out << j;
		//out.close();

		std::string postData = j.dump();
		uint32_t pos = postData.find("\"LMT_handle_jobs\"");
		postData.insert(pos, " "); // For some reason they require a space here for the request to work
		out = (char*)malloc(postData.length() + 1);
		sprintf(out, "%s", postData.c_str());
	}
	catch (const std::exception& e)
	{
		std::ofstream err("error.txt");
		err << e.what();
		err.close();
	}


	return out;
}

wchar_t* DeepLWindow::FindTranslatedText(wchar_t* html)
{
	std::vector<char> vec((wcslen(html) + 1) * 4);
	WideCharToMultiByte(CP_UTF8, 0, html, -1, vec.data(), vec.size(), nullptr, nullptr);
	std::string str(vec.data());

	if(m_pResult) delete[] m_pResult;

	std::string out = "";
	uint32_t i = 0;
	try
	{
		json j = json::parse(str);

		if (j.contains("result") && j["result"].contains("translations"))
		{
			for (const auto& val : j["result"]["translations"])
			{
				if (val.contains("beams") && val["beams"][0].contains("postprocessed_sentence"))
				{
					std::string c = " ";
					while (!newLines.empty() && newLines.front() == i)
					{
						if (c == " ") c.clear();
						c.append("\n");
						newLines.pop_front();
					}

					out.append(val["beams"][0]["postprocessed_sentence"].get<std::string>() + c);
					i++;
				}
			}
		}
	}
	catch (const std::exception& e)
	{
		std::ofstream err("error2.txt");
		err << e.what();
		err.close();
	}

	std::wstring result = ToWstring(out);

	m_pResult = new wchar_t[result.size() + 1]();
	wsprintf(m_pResult, L"%s", result.c_str());

	return m_pResult;
}

char* DeepLWindow::GetLangIdString(Language lang, int src)
{
	if (src && lang == LANGUAGE_Chinese_Traditional)
		lang = LANGUAGE_Chinese_Simplified;
	switch (lang)
	{
		case LANGUAGE_AUTO:
			return "AUTO";
		case LANGUAGE_English:
			return "EN";
		case LANGUAGE_German:
			return "DE";
		case LANGUAGE_French:
			return "FR";
		case LANGUAGE_Spanish:
			return "ES";
		case LANGUAGE_Italian:
			return "IT";
		case LANGUAGE_Portuguese:
			return "PT";
		case LANGUAGE_Dutch:
			return "NL";
		case LANGUAGE_Polish:
			return "PL";
		case LANGUAGE_Russian:
			return "RU";
		case LANGUAGE_Japanese:
			return "JA";
		case LANGUAGE_Chinese_Simplified:
			return "ZH";
		case LANGUAGE_Bulgarian:
			return "BG";
		case LANGUAGE_Czech:
			return "CS";
		case LANGUAGE_Danish:
			return "DA";
		case LANGUAGE_Estonian:
			return "ET";
		case LANGUAGE_Finnish:
			return "FI";
		case LANGUAGE_Greek:
			return "EL";
		case LANGUAGE_Hungarian:
			return "HU";
		case LANGUAGE_Latvian:
			return "LV";
		case LANGUAGE_Lithuanian:
			return "LT";
		case LANGUAGE_Romanian:
			return "RO";
		case LANGUAGE_Slovak:
			return "SK";
		case LANGUAGE_Slovenian:
			return "SL";
		case LANGUAGE_Swedish:
			return "SV";

		default:
			return 0;
	}
}
