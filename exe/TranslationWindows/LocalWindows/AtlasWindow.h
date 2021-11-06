#include "../TranslationWindow.h"
#include "Shared/scoped_ptr.h"

class Thread;

class AtlasWindow : public TranslationWindow
{
public:
	int forceHalt;

	AtlasWindow();
	~AtlasWindow();
	int WndProc (LRESULT *output, UINT uMsg, WPARAM wParam, LPARAM lParam);
	virtual void Translate(SharedString *text, int history_id, bool only_use_history) override;
	void Halt();
	//void SaveWindowTypeConfig();
	int SetUpAtlas();

	inline bool CanTranslate(Language src, Language dst)
	{
		return (src == LANGUAGE_Japanese && dst == LANGUAGE_English) || (src == LANGUAGE_English && dst == LANGUAGE_Japanese);
	}

private:
	void TryStartTask();
	friend DWORD WINAPI AtlasThreadProc(void *taskInfo);
	void AddClassButtons();

	// Does all the work.
	// It'll pass a message to the hWnd of the Atlas window in the main thread when done.
	scoped_ptr<Thread> thread_;
};

void LaunchAtlasDictSearch();
int ConfigureAtlas(HWND hWnd, AtlasConfig &atlasConfig);
