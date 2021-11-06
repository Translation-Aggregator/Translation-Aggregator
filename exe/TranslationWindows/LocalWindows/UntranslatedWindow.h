#include "../TranslationWindow.h"

class UntranslatedWindow : public TranslationWindow
{
	int transHighlighted;
public:
	UntranslatedWindow();
	int WndProc (LRESULT *output, UINT uMsg, WPARAM wParam, LPARAM lParam);
	void Translate(SharedString *text, int history_id, bool only_use_history) override;
	void SaveWindowTypeConfig();
	void AddClassButtons();
	inline bool CanTranslate(Language src, Language dst)
	{
		return true;
	}
};
