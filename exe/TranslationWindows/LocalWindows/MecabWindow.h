#pragma once
#include "FuriganaWindow.h"
#include "Shared/Thread.h"

class MecabWindow : public FuriganaWindow
{
public:

	MecabWindow();
	~MecabWindow();
	virtual void Translate(SharedString *text, int history_id, bool only_use_history) override;
	int WndProc(LRESULT *output, UINT uMsg, WPARAM wParam, LPARAM lParam);

	inline bool CanTranslate(Language src, Language dst)
	{
		return src == LANGUAGE_Japanese && dst == LANGUAGE_English;
	}

private:
	void TryStartTask();

	Thread thread_;
};
