#pragma once
#include "../TranslationWindow.h"
// Needed for language enum.

class HttpWindow : public TranslationWindow
{
public:
	int WndProc (LRESULT *output, UINT uMsg, WPARAM wParam, LPARAM lParam);
	friend void CALLBACK HttpCallback(HINTERNET hInternet2, DWORD_PTR dwContext, DWORD dwInternetStatus, LPVOID lpvStatusInformation, DWORD dwStatusInformationLength);
	friend void CALLBACK HttpCookieCallback(HINTERNET hInternet2, DWORD_PTR dwContext, DWORD dwInternetStatus, LPVOID lpvStatusInformation, DWORD dwStatusInformationLength);
	HttpWindow(wchar_t *type, wchar_t *srcUrl, unsigned int flags=0);
	virtual ~HttpWindow();

	void Halt();

	virtual void Translate(SharedString *text, int history_id, bool only_use_history) override;
	// void Translate(wchar_t *text, char len);
	void TryMakeRequest();

protected:
	bool queued_force_history_;
	// Both UTF8 by default.
	int postCodePage;
	int replyCodePage;
	int impersonateIE;

	wchar_t *host;
	wchar_t *path;
	wchar_t *referrer;
	unsigned short port;
	bool dontEscapeRequest;

	wchar_t *secondRequest;

	// Cached posted strings, as per WebHttp specs.
	char *postData;
	char *postPrefixTemplate;

	wchar_t *requestHeaders;

	// Finds the text to be translated from a response and null terminates it.
	// Returns null on failure.  Doesn't remove HTML formatting.
	virtual wchar_t *FindTranslatedText(wchar_t* html);
	virtual void StripResponse(wchar_t* html, std::wstring* text) const;
	virtual void DisplayResponse(const wchar_t* text);

	// ID used in the history.
	int transtion_history_id_;

	// ID of the translation in progress, used for saving to the history.
	int in_progress_history_id_;

	char *buffer;
	int bufferSize;
	int reading;
	HINTERNET hRequest;
	HINTERNET hConnect;
	void CleanupRequest();
	void CompleteRequest();

	virtual char * GetLangIdString(Language lang, int src) {return 0;}

	// If can't translate, GetTranslationPath() returns null or postPrefixTemplate
	// is non-null but GetTranslationPrefix() returns null.

	// Note that this must be in whatever format the server wants posted,
	// and posting using wide characters is not yet supported, as no
	// site needs it.
	virtual char *GetTranslationPrefix(Language src, Language dst, const char *text);
	virtual wchar_t *GetTranslationPath(Language src, Language dst, const wchar_t *text);

	// Lets me use same code to come up with necessary request as
	// checks if languages are supported.
	inline bool CanTranslate(Language src, Language dst)
	{
		wchar_t *path = GetTranslationPath(src, dst, NULL);
		if (!path) return false;
		free(path);
		if (postPrefixTemplate)
		{
			char *prefix = GetTranslationPrefix(src, dst, NULL);
			if (!prefix) return false;
			return true;
		}
		return true;
	}

	void GetCookie();
	HANDLE m_hCookieHttpEvent;
	std::wstring m_cookie;
	std::wstring m_cookiePath;
};
