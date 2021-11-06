#pragma once
#include "HttpWindow.h"

class JdicWindow : public HttpWindow
{
public:
	JdicWindow();
	~JdicWindow();
	void LoadConfig();

	wchar_t *FindTranslatedText(wchar_t* html);
	virtual void StripResponse(wchar_t* html, std::wstring* text) const override;
	void DisplayResponse(const wchar_t* text) override;
	int WndProc(LRESULT *output, UINT uMsg, WPARAM wParam, LPARAM lParam);

	friend INT_PTR CALLBACK JdicDialogProc(HWND hWndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
	wchar_t *GetTranslationPath(Language src, Language dst, const wchar_t *text);
};
