#pragma once
// Defines base class for each subwindow (Even the untranslated one,
// despite class name).

// Currently needed for Language enum.
#include "../Config.h"
#include <Shared/StringUtil.h>

#define SEPARATOR_WIDTH 8

#define TWF_SEPARATOR    1
#define TWF_CONFIG_BUTTON  2
#define TWF_RICH_TEXT    4
#define TWF_NO_EDIT      8

LRESULT CALLBACK TranslationWindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

class TranslationWindow
{
private:
	inline virtual void AddClassButtons() {}

public:
	void PlaceWindows();
	// If mod is null, make any substitutions to orig and populate it as needed.
	void Translate(SharedString *orig, SharedString *&mod, int history_id, bool only_use_history);
	virtual void Translate(SharedString *text, int history_id, bool only_use_history) = 0;

	// Saved to config file.
	char autoClipboard;

	// set on creation and not changed.
	unsigned int flags;
	char remote;

	wchar_t *windowType;
	wchar_t *srcUrl;
	SharedString *queuedString;
	int queued_string_history_id_;

	HWND hWnd;
	HWND hWndEdit;
	HWND hWndToolbar;
	HWND hWndRebar;
	HWND hWndParent;
	// Set by main window.
	int id;
	int menuIndex;


	int busy;

	TranslationWindow(wchar_t *type, char remote, wchar_t *srcUrl = 0, unsigned int flags=0);
	virtual ~TranslationWindow();

	// returns 1 if it wants output to be returned, without running default proc.
	// Also don't need hWnd, since I already have it.
	virtual int WndProc (LRESULT *output, UINT uMsg, WPARAM wParam, LPARAM lParam);

	// Often have to do this early, for better shutdown.
	void ClearQueuedTask();

	// Keep one structure around per allowed window type.
	// So can have structures without windows.
	virtual int MakeWindow(int showToolbar, HWND hWndParent);
	// window-class specific button code, called whenever toolbar is created.

	void ShowToolbar(int show);
	// *Not* called on quit, as windows have already been destroyed.
	virtual void NukeWindow();
	void Animate();
	virtual void Halt() {}
	virtual void Stopped();
	void SetFont();
	virtual void SaveWindowTypeConfig();
	inline void SetWindowParent(HWND hWndParent)
	{
		if (hWnd)
		{
			this->hWndParent = hWndParent;
			SetParent(hWnd, hWndParent);
		}
	}

	inline virtual bool CanTranslate(Language src, Language dst) {return false;}
	int CanTranslateCurrent()
	{
		return CanTranslate(config.langSrc, config.langDst);
	}

	void UpdateEnabled();
};

// Only used by TranslationWindow currently, but handy to have around.
TranslationWindow *GetObjectFronHandle(HWND);

int RegisterTranslationWindowClass();
