#include <Shared/Shrink.h>
#include "HttpWindow.h"
#include <Shared/StringUtil.h>
#include "../../util/HttpUtil.h"
#include "exe/History/History.h"

extern const wchar_t userAgent[] = L"Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:91.0) Gecko/20100101 Firefox/91.0";

int numHttpWindows = 0;
HINTERNET hHttpSession[3] = {0, 0, 0}; // Connection 2 (3) is a google specific connection which will be reset every n connections

void CALLBACK HttpCallback(HINTERNET hInternet, DWORD_PTR dwContext, DWORD dwInternetStatus, LPVOID lpvStatusInformation, DWORD dwStatusInformationLength);
void CALLBACK HttpCookieCallback(HINTERNET hInternet, DWORD_PTR dwContext, DWORD dwInternetStatus, LPVOID lpvStatusInformation, DWORD dwStatusInformationLength);

int MakeInternet(int impersonateIE)
{
	if (!hHttpSession[impersonateIE])
	{
		const wchar_t* userAgent = ::userAgent;// HTTP_REQUEST_ID;
		if (impersonateIE >= 1)
			userAgent = ::userAgent;
		hHttpSession[impersonateIE] = WinHttpOpen(userAgent, WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, 0, 0, WINHTTP_FLAG_ASYNC);
	}
	return hHttpSession[impersonateIE] != 0;
}

void HttpWindow::CleanupRequest()
{
	in_progress_history_id_ = -1;
	if (hRequest)
	{
		// Just in case...
		WinHttpSetStatusCallback(hRequest, 0, 0, 0);
		if (hRequest) WinHttpCloseHandle(hRequest);
		hRequest = 0;
	}
	// Don't keep it around because of error handling...
	// would have to detect its timeout, etc, and
	// MS's WinHttp specs are crap.  And testing timeouts
	// is a pain.
	if (hConnect)
	{
		WinHttpCloseHandle(hConnect);
		hConnect = 0;
	}
	if (buffer) free(buffer);
	Stopped();
	buffer = 0;
	reading = 0;
	bufferSize = 0;
	if (postData)
	{
		free(postData);
		postData = 0;
	}

	// See if anything queued up while we were occupied.
	TryMakeRequest();
}

struct ContentCoding
{
	int codePage;
	char *string;
};

const ContentCoding contentCodings[] =
{
	{932, "SHIFT_JIS"},
	{CP_UTF8, "UTF-8"},
	{20932, "EUC-JP"},
	{28591, "ISO-8859-1"},
	{28591, "Latin-1"},
	{28595, "ISO-8859-5"}
};

char *HttpWindow::GetTranslationPrefix(Language src, Language dst, const char *text)
{
	if (!postPrefixTemplate)
		return 0;

	char *srcString, *dstString;
	if (!(srcString = GetLangIdString(src, 1)) || !(dstString = GetLangIdString(dst, 0)) || !strcmp(srcString, dstString))
		return 0;

	if (!text)
		return (char*)1;

	char *out = (char*) malloc(strlen(postPrefixTemplate) + strlen(srcString) + strlen(dstString) + strlen(text) + 1);
	sprintf(out, postPrefixTemplate, srcString, dstString, text);
	return out;
}

wchar_t *HttpWindow::GetTranslationPath(Language src, Language dst, const wchar_t *data)
{
	if (wcsstr(path, L"%hs"))
	{
		char *srcString, *dstString;
		if (src == dst || !(srcString = GetLangIdString(src, 1)) || !(dstString = GetLangIdString(dst, 0)))
			return 0;
		if (!data)
			return wcsdup(L"");
		wchar_t *out = (wchar_t*) malloc((wcslen(path) + strlen(srcString) + strlen(dstString) + wcslen(data) + 1) * sizeof(wchar_t));
		wsprintf(out, path, srcString, dstString, data);
		return out;
	}
	return wcsdup(path);
}

int HttpWindow::WndProc (LRESULT *output, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (uMsg == WMA_CLEANUP_REQUEST)
	{
		CleanupRequest();
		return 1;
	}
	else if (uMsg == WMA_COMPLETE_REQUEST)
	{
		CompleteRequest();
		return 1;
	}
	return 0;
}

void HttpWindow::CompleteRequest()
{
	if (hWnd)
	{
		if (!buffer || bufferSize < 4)
			SetWindowTextA(hWndEdit, "Connection Error.\n");
		else
		{
			wchar_t *html = (wchar_t*)buffer;
			int len = 0;
			// UTF16, cannabalize buffer.
			if (html[0] == 0xFFFE || html[0] == 0xFEFF)
			{
				len = bufferSize / 2;
				// BE, need to flip.
				if (html[0] == 0xFFFE)
					for (int i=1; i<len; i++)
						html[i-1] = ntohs(html[i]);
				// LE, just need to move data back one.  Makes cleanup more uniform.
				else
					for (int i=1; i<len; i++)
						html[i-1] = html[i];
				html[--len] = 0;
				bufferSize = 0;
				buffer = 0;
			}
			else
			{
				int p;
				buffer[bufferSize] = 0;
				int detectedCharset = 0;
				for (p=0; p < bufferSize - 40; p++)
				{
					if (!strnicmp(buffer+p, "<META", 5))
					{
						char *e = strchr(buffer+p, '>');
						if (!e) break;
						char *s = buffer+p;
						int flags = 0;
						detectedCharset = 0;
						while (s < e)
						{
							if (!strnicmp(s, ("content-type"), 12)) flags |=1;
							else if (!strnicmp(s, ("http-equiv"), 10)) flags |=2;
							else if (!strnicmp(s, "content", 7))
							{
								while (s < e && *s != '=') s++;
								while (s < e)
								{
									if (!strnicmp(s, "charset", 7))
									{
										s+=7;
										while (s < e && !isalnum((unsigned char)*s))
											s++;
										if (s<e)
										{
											for (int i=0; i<sizeof(contentCodings)/sizeof(contentCodings[0]); i++)
											{
												if (!strnicmp(s, contentCodings[i].string, strlen(contentCodings[i].string)))
												{
													detectedCharset = contentCodings[i].codePage;
													break;
												}
											}
											break;
										}
									}
									s++;
								}

							}
							s++;
						}
						if (flags == 3 && detectedCharset) break;
						detectedCharset = 0;
						p = (int) (e - buffer);
					}
				}
				if (!detectedCharset)
					detectedCharset = replyCodePage;

				len = 1+bufferSize;
				html = ToWideChar(buffer, detectedCharset, len);

				if (len)
					len--;
			}
			if (!html)
				SetWindowTextA(hWndEdit, "Invalid Data Received.\n");
			else
			{
				for (int i=len-1; i >= 0; i--)
					if (html[i] == 0) html[i] = ' ';
				wchar_t * data = FindTranslatedText(html);
				if (!data)
				{
					wchar_t *temp = (wchar_t*)malloc(sizeof(wchar_t)*(50 + len));
					wcscpy(temp, L"Unrecognized response received:\n\n");
					memcpy(wcschr(temp, 0), html, sizeof(wchar_t) * (wcslen(html)+1));
					SetWindowTextW(hWndEdit, temp);
					free(temp);
				}
				else
				{
					// Get rid of CRs, for simpler parsing.
					SpiffyUp(data);
					std::wstring text;
					StripResponse(data, &text);
					history.SetTranslation(in_progress_history_id_, transtion_history_id_, text);
					DisplayResponse(text.c_str());
				}
			}
			free(html);
		}
	}
	CleanupRequest();
}

wchar_t* HttpWindow::FindTranslatedText(wchar_t* html)
{
	return html;
}

void HttpWindow::StripResponse(wchar_t* html, std::wstring* text) const
{
	text->assign(html);
}

void HttpWindow::DisplayResponse(const wchar_t* text)
{
	SetWindowTextW(hWndEdit, text);
}

void CALLBACK HttpCallback(HINTERNET hInternet, DWORD_PTR dwContext, DWORD dwInternetStatus, LPVOID lpvStatusInformation, DWORD dwStatusInformationLength)
{
	HttpWindow *w = (HttpWindow *)dwContext;

	if (!w || hInternet != w->hRequest) return;

	if (dwInternetStatus == WINHTTP_CALLBACK_STATUS_SENDREQUEST_COMPLETE)
	{
		if (!WinHttpReceiveResponse(hInternet, 0))
			PostMessage(w->hWnd, WMA_CLEANUP_REQUEST, 0, 0);
	}
	else if (dwInternetStatus == WINHTTP_CALLBACK_STATUS_HEADERS_AVAILABLE)
	{
		if (!w->reading && !WinHttpQueryDataAvailable(hInternet, 0))
			PostMessage(w->hWnd, WMA_COMPLETE_REQUEST, 0, 0);
	}
	else if (dwInternetStatus == WINHTTP_CALLBACK_STATUS_DATA_AVAILABLE)
	{
		if (!w->reading)
		{
			int bytes = ((DWORD*)lpvStatusInformation)[0];
			if (bytes <= 0)
				PostMessage(w->hWnd, WMA_COMPLETE_REQUEST, 0, 0);
			else
			{
				w->buffer = (char*) realloc(w->buffer, w->bufferSize + bytes+4);
				w->reading = 1;
				if (!WinHttpReadData(hInternet, w->buffer+w->bufferSize, bytes, 0))
				{
					w->reading = 0;
					PostMessage(w->hWnd, WMA_COMPLETE_REQUEST, 0, 0);
				}
			}
		}
	}
	else if (dwInternetStatus == WINHTTP_CALLBACK_STATUS_READ_COMPLETE)
	{
		w->reading = 0;
		if (dwStatusInformationLength <= 0) dwStatusInformationLength = 0;
		w->bufferSize += dwStatusInformationLength;
		w->buffer[w->bufferSize] = 0;
		if (!dwStatusInformationLength || !WinHttpQueryDataAvailable(hInternet, 0))
			PostMessage(w->hWnd, WMA_COMPLETE_REQUEST, 0, 0);
	}
	else if (dwInternetStatus == WINHTTP_CALLBACK_STATUS_REQUEST_ERROR)
	{
		//WINHTTP_ASYNC_RESULT *statusInfo = (WINHTTP_ASYNC_RESULT*)lpvStatusInformation;
		PostMessage(w->hWnd, WMA_COMPLETE_REQUEST, 0, 0);
	}
}

void CALLBACK HttpCookieCallback(HINTERNET hInternet, DWORD_PTR dwContext, DWORD dwInternetStatus, LPVOID lpvStatusInformation, DWORD dwStatusInformationLength)
{
	HttpWindow *w = (HttpWindow *)dwContext;
	if (!w) return;

	if(dwInternetStatus == WINHTTP_CALLBACK_STATUS_SENDREQUEST_COMPLETE || dwInternetStatus == WINHTTP_CALLBACK_STATUS_HEADERS_AVAILABLE)
		SetEvent(w->m_hCookieHttpEvent);
}

wchar_t *HttpEscapeParamW(const wchar_t *src, int len)
{
	wchar_t *out = (wchar_t*) malloc(sizeof(wchar_t)*(3*len+1));
	wchar_t *dst = out;
	while (len)
	{
		len--;
		wchar_t c = *src;
#if 0
		if (c == '\n')
		{
				wcscpy(dst, L"\",\"");
				dst += 3;
				src++;
				continue;
		}
		if (c == '\"')
		{
				wcscpy(dst, L"\\\"");
				dst += 2;
				src++;
				continue;
		}
		if (c == '\\')
		{
			wcscpy(dst, L"\\\\");
			dst += 2;
			src++;
			continue;
		}
#endif
		if (c <= 0x26 || c == '+' || (0x3A <= c && c <= 0x40) || c == '\\' || (0x5B <= c && c <= 0x60) || (0x7B <= c && c <= 0x7E))
		{
				wsprintfW(dst, L"%%%02X", c);
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

char *HttpEscapeParamA(const char *src, int len)
{
	char *out = (char*) malloc(sizeof(char)*(3*strlen(src)+1));
	char *dst = out;
	while (len)
	{
		len--;
		char c = *src;
		if (c <= 0x26 || c == '+' || (0x3A <= c && c <= 0x40) || c == '\\' || (0x5B <= c && c <= 0x60) || (0x7B <= c && c <= 0x7E))
		{
				sprintf(dst, "%%%02X", (unsigned char) c);
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

#define HTTP_GET L"GET"
#define HTTP_POST L"POST"

void HttpWindow::TryMakeRequest()
{
	if (hRequest || hConnect) return;
	if (!queuedString)
	{
		Stopped();
		return;
	}

	if (!CanTranslate(config.langSrc, config.langDst))
	{
		ClearQueuedTask();
		CleanupRequest();
		SetWindowText(hWndEdit, L"");
		return;
	}

	std::wstring history_entry;
	history.GetTranslation(queued_string_history_id_, transtion_history_id_, &history_entry);
	if (!history_entry.empty())
	{
		ClearQueuedTask();
		CleanupRequest();
		DisplayResponse(history_entry.c_str());
		return;
	}

	if (queued_force_history_)
	{
		ClearQueuedTask();
		CleanupRequest();
		SetWindowText(hWndEdit, L"");
		return;
	}

	if (!MakeInternet(impersonateIE))
	{
		hConnect = 0;
		SetWindowTextA(hWndEdit, "Failed to create connection.\n");
		ClearQueuedTask();
		CleanupRequest();
		return;
	}

	in_progress_history_id_ = queued_string_history_id_;

	busy = 1;

	postData = 0;
	int postLen = 0;
	wchar_t *path;
	wchar_t *pType;

	if (!postPrefixTemplate)
	{
		// GET
		pType = HTTP_GET;
		wchar_t *string = dontEscapeRequest ? queuedString->string : HttpEscapeParamW(queuedString->string, queuedString->length);
		ClearQueuedTask();
		path = GetTranslationPath(config.langSrc, config.langDst, string);
		if (!dontEscapeRequest) free(string);
	}
	else
	{
		// POST
		pType = HTTP_POST;
		int len = -1;
		char *postDataTemp = ToMultiByte(queuedString->string, postCodePage, len);
		ClearQueuedTask();
		if (!len)
		{
			free(postDataTemp);
			CleanupRequest();
			return;
		}
		char *postDataEscaped;
		if (dontEscapeRequest)
			postDataEscaped = postDataTemp;
		else
		{
			postDataEscaped = HttpEscapeParamA(postDataTemp, len-1);
			free(postDataTemp);
		}

		postData = GetTranslationPrefix(config.langSrc, config.langDst, postDataEscaped);
		free(postDataEscaped);

		postLen = strlen(postData);

		path = GetTranslationPath(config.langSrc, config.langDst, L"");
	}

	hConnect = WinHttpConnect(hHttpSession[impersonateIE], host, port, 0);
	if(!hConnect)
	{
		SetWindowTextA(hWndEdit, "Failed to create connection.\n");
		ClearQueuedTask();
		CleanupRequest();
		return;
	}

	hRequest = WinHttpOpenRequest(hConnect, pType, path, 0, referrer, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_ESCAPE_DISABLE | (port == 443 ? WINHTTP_FLAG_SECURE : 0));
	free(path);


	if (secondRequest)
	{
		WinHttpSendRequest(hRequest, requestHeaders, wcslen(requestHeaders), postData, postLen, postLen, 0);
		WinHttpReceiveResponse(hRequest, NULL);
		WinHttpCloseHandle(hRequest);
		hRequest = WinHttpOpenRequest(hConnect, L"GET", secondRequest, 0, referrer, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_ESCAPE_DISABLE | (port == 443 ? WINHTTP_FLAG_SECURE : 0));
		free(postData);
		postData = NULL;
		postLen = 0;
	}

	if (!hRequest ||
		WINHTTP_INVALID_STATUS_CALLBACK == WinHttpSetStatusCallback(hRequest, HttpCallback, WINHTTP_CALLBACK_FLAG_ALL_NOTIFICATIONS | WINHTTP_CALLBACK_FLAG_DATA_AVAILABLE | WINHTTP_CALLBACK_FLAG_REQUEST_ERROR | WINHTTP_CALLBACK_FLAG_CLOSE_CONNECTION, 0) ||
		!WinHttpSendRequest(hRequest, requestHeaders, wcslen(requestHeaders), postData, postLen, postLen, (DWORD_PTR) this))
	{
			CleanupRequest();
			SetWindowTextA(hWndEdit, "Failed to create connection.\n");
	}
}

void HttpWindow::Translate(SharedString *string, int history_id, bool only_use_history)
{
	ClearQueuedTask();
	busy = 1;
	queuedString = string;
	queued_force_history_ = only_use_history;
	queued_string_history_id_ = history_id;
	string->AddRef();
	if (!hRequest)
	{
		TryMakeRequest();
		if (hRequest) SetWindowTextA(hWndEdit, "Requesting Data.\n");
	}
	else
		SetWindowTextA(hWndEdit, "Busy.  Translation Queued.\n");
}

void AddHttpWindow()
{
	numHttpWindows++;
}

HttpWindow::HttpWindow(wchar_t *type, wchar_t *srcUrl, unsigned int flags) :
	TranslationWindow(type, 1, srcUrl, flags),
	in_progress_history_id_(-1), referrer(NULL),
	m_cookie(L""),
	m_hCookieHttpEvent(nullptr)
{
	impersonateIE = 0;
	dontEscapeRequest = false;
	hRequest = 0;
	hConnect = 0;
	buffer = 0;
	bufferSize = 0;
	reading = 0;

	m_hCookieHttpEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

	secondRequest = NULL;

	postData = 0;
	postPrefixTemplate = 0;
	requestHeaders = L"Accept-Charset: ISO-8859-1,utf-8;q=0.7,*;q=0.7\r\nContent-Type: application/x-www-form-urlencoded";

	m_cookiePath = L"/";

	replyCodePage = CP_UTF8;
	postCodePage = CP_UTF8;

	port = 80;

	static int next_history_id = 0;
	transtion_history_id_ = next_history_id++;

	AddHttpWindow();
};

HttpWindow::~HttpWindow()
{
	ClearQueuedTask();
	CleanupRequest();
	numHttpWindows--;
	WinHttpCloseHandle(hConnect);

	if(m_hCookieHttpEvent)
		CloseHandle(m_hCookieHttpEvent);

	if (!numHttpWindows)
		for (int i=0; i<3; i++)
			if (hHttpSession[i])
			{
				WinHttpCloseHandle(hHttpSession[i]);
				hHttpSession[i] = 0;
			}
}

void HttpWindow::Halt()
{
	if (hRequest && hWndEdit)
		SetWindowText(hWndEdit, L"Cancelled");
	ClearQueuedTask();
	CleanupRequest();
}

void HttpWindow::GetCookie()
{
	extern HINTERNET hHttpSession[3];

	if(impersonateIE == 2) // Google stuff
	{
		WinHttpCloseHandle(hHttpSession[impersonateIE]);
		hHttpSession[impersonateIE] = nullptr;
		MakeInternet(impersonateIE);
	}

	m_cookie.clear();

	if(HINTERNET hConnect = WinHttpConnect(hHttpSession[impersonateIE], host, port, 0))
	{
		if(HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", m_cookiePath.c_str(), 0, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, port == 443 ? WINHTTP_FLAG_SECURE : 0))
		{
			ResetEvent(m_hCookieHttpEvent);
			WinHttpSetStatusCallback(hRequest, HttpCookieCallback, WINHTTP_CALLBACK_FLAG_ALL_NOTIFICATIONS | WINHTTP_CALLBACK_FLAG_DATA_AVAILABLE | WINHTTP_CALLBACK_FLAG_REQUEST_ERROR | WINHTTP_CALLBACK_FLAG_CLOSE_CONNECTION | WINHTTP_CALLBACK_STATUS_HEADERS_AVAILABLE, 0);
			if(WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, (DWORD_PTR) this))
			{
				WaitForSingleObject(m_hCookieHttpEvent, 5000); // Wait 5 sec. for the event to be set
				if(WinHttpReceiveResponse(hRequest, NULL))
				{
					ResetEvent(m_hCookieHttpEvent);
					WaitForSingleObject(m_hCookieHttpEvent, 5000); // Wait 5 sec. for the event to be set
					DWORD index = 0;
					{
						for(;;)
						{
							DWORD err = GetLastError();
							DWORD index2 = index;
							DWORD len = 0;
							WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_SET_COOKIE, WINHTTP_HEADER_NAME_BY_INDEX, NULL, &len, &index2);
							err = GetLastError();

							if(GetLastError() != ERROR_INSUFFICIENT_BUFFER)
								break;

							wchar_t *pBuffer = reinterpret_cast<wchar_t*>(calloc(len, sizeof(wchar_t)));
							if (!pBuffer)
								break;

							if(!WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_SET_COOKIE, WINHTTP_HEADER_NAME_BY_INDEX, pBuffer, &len, &index))
								break;

							m_cookie.append(pBuffer);
							m_cookie.append(L"; ");
						}
					}
				}
			}
			WinHttpCloseHandle(hRequest);
		}
		WinHttpCloseHandle(hConnect);
	}
}
