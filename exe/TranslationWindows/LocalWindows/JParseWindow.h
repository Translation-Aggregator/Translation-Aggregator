#pragma once
#include "FuriganaWindow.h"
#include "Shared/Thread.h"

class JParseWindow : public FuriganaWindow
{
public:
	JParseWindow();
	~JParseWindow();
	virtual void Translate(SharedString *text, int history_id, bool only_use_history) override;
	void DisplayResponse(wchar_t *text);
	int WndProc(LRESULT *output, UINT uMsg, WPARAM wParam, LPARAM lParam);
	void SaveWindowTypeConfig();

	inline bool CanTranslate(Language src, Language dst)
	{
		return true;
		return src == LANGUAGE_Japanese;
	}

private:
	void TryStartTask();
	Thread thread_;
};
