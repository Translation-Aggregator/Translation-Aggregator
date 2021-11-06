#pragma once
#include "../TranslationWindow.h"
#include <Shared/Thread.h>

class Thread;

class LECWindow : public TranslationWindow
{
public:

	LECWindow();
	~LECWindow();
	virtual void Translate(SharedString *text, int history_id, bool only_use_history) override;
	int WndProc(LRESULT *output, UINT uMsg, WPARAM wParam, LPARAM lParam);

	inline bool CanTranslate(Language src, Language dst)
	{
		return lecState >=0 && src == LANGUAGE_Japanese && dst == LANGUAGE_English;
	}

private:
	typedef int(__cdecl *eg_init_t)(const char *path);
	typedef int(__cdecl *eg_init2_t)(const char *path, int);
	typedef int(__cdecl *eg_end_t)();
	typedef int(__cdecl *eg_translate_multi_t)(int, const char *in, size_t out_size, char *out);
	//typedef int(__cdecl *eg_translate_one_t)(int, const char *in, const char *, size_t out_size, char *out, void*, void*);
	//typedef int(__cdecl *eg_setcallback_t)(int(__cdecl *callback)());
	HMODULE hLEC;
	eg_end_t eg_end;
	eg_translate_multi_t eg_translate_multi;
	int lecState; // 0 - not initialized, 1 - ready, -1 - not available;

	void SetUpLEC();
	bool LoadLECFromPath(char *path);

	void TryStartTask();

	Thread thread_;
	bool lecInitialized;

	friend class LECTask;
};
