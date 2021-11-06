#include <Shared/Shrink.h>
#include "LECWindow.h"

#if 1
// Jp->En engine
#define CODEPAGE 932
#define PATH "Nova\\JaEn\\EngineDll_je.dll"
#else
// Ko->En engine
#define CODEPAGE 949
#define PATH "Nova\\KoEn\\keeglib.dll"
#endif

static LECWindow *lecWindow;

class LECTask : public Thread::Task
{
public:
	LECTask(LECWindow* window, wchar_t* orig_string) :
		window_(window), src(wcsdup(orig_string))
	{
	}

	~LECTask()
	{
		free(src);
	}

	virtual void Run() override
	{
		size_t src_size = wcslen(src) + 1, dst_size;
		// direct engine call doesn't handle those characters well
		// better handling would be manual sentence splitting with
		//  brackets content replacement with signle symbol which is
		//  replaced back after independent translation of content
		for (size_t i = 0; i < src_size; i++)
			switch (src[i])
			{
				case L'『':// src[i] = L'{'; break;
				case L'｢':
				case L'「': src[i] = L'['; break;
				case L'』':// src[i] = L'}'; break;
				case L'｣':
				case L'」': src[i] = L']'; break;
				case L'≪':
				case L'（': src[i] = L'('; break;
				case L'≫':
				case L'）': src[i] = L')'; break;
				case L'…': src[i] = L' '; break;
				case L'：': src[i] = L'￤'; break;
			}
		char *src_buf = (char*)malloc(src_size*2);
		WideCharToMultiByte(CODEPAGE, 0, src, src_size, src_buf, src_size*2, "_", NULL);
		free(src);
		src = NULL;

		// we have no idea how much buffer we actually need here
		// src_size*3 looks like a good guess, but let's play it a bit more safe
		char *dst_buf = NULL;
		for (size_t size = src_size*4 + 0x100;;)
		{
			char *d = (char*)realloc(dst_buf, size);
			if (!d)
				break;
			dst_buf = d;
			//window_->eg_translate_one(0, src_buf, NULL, size, dst_buf, NULL, NULL);
			window_->eg_translate_multi(0, src_buf, size, dst_buf);
			dst_size = strlen(dst_buf) + 1;
			if (dst_size < size)
				break;
			size *= 2;
		}
		free(src_buf);

		wchar_t *dst = (wchar_t*)malloc(dst_size*sizeof(wchar_t));
		MultiByteToWideChar(CODEPAGE, 0, dst_buf, dst_size, dst, dst_size);
		free(dst_buf);

		if (window_->hWndEdit)
			SetWindowTextW(window_->hWndEdit, dst);
		free(dst);

		if (window_->hWnd)
			PostMessage(window_->hWnd, WMA_THREAD_DONE, 0, 0);
	}

private:
	LECWindow* window_;
	wchar_t *src;
};

LECWindow::LECWindow() :
	TranslationWindow(L"LEC", 0, L"http://www.lec.com/power-translator-software.asp"),
	thread_("LEC"),
	lecState(0),
	hLEC(NULL)
{
	lecWindow = this;
}

LECWindow::~LECWindow()
{
	ClearQueuedTask();
	lecWindow = NULL;

	if (hLEC)
	{
		eg_end();
		FreeLibrary(hLEC);
	}
}

void LECWindow::TryStartTask()
{
	if (!queuedString || busy)
		return;

	if (!lecState)
	{
		SetUpLEC();
		if (lecState < 0)
		{
			SetWindowTextW(hWndEdit, L"Failed to initialize LEC Nova engine");
			EnableWindow(hWndEdit, FALSE);
		}
	}
	if (lecState < 0)
		return;

	busy = 1;
	thread_.PostTask(new LECTask(this, queuedString->string));
	ClearQueuedTask();
}

void LECWindow::Translate(SharedString *string, int history_id, bool only_use_history)
{
	ClearQueuedTask();
	if (!CanTranslate(config.langSrc, config.langDst))
	{
		SetWindowText(hWndEdit, L"");
		return;
	}
	string->AddRef();
	queuedString = string;
	TryStartTask();
}

void LECWindow::SetUpLEC()
{
	char path[MAX_PATH + 7 + sizeof(PATH)];
	path[GetModuleFileNameA(NULL, path, MAX_PATH)] = 0;
	if (char *p = strrchr(path, '\\'))
		p[1] = 0;
	strcat(path, "Plugins\\");
	if (LoadLECFromPath(path))
		return;

	lecState = -1;

	HKEY key;
	if (!RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\LogoMedia\\LEC Power Translator 15\\Configuration", 0, KEY_QUERY_VALUE, &key))
	{
		DWORD size = MAX_PATH;
		if (!RegQueryValueExA(key, "ApplicationPath", NULL, NULL, (LPBYTE)path, &size))
		{
			if (size && !path[size - 1])
				size--;
			if (size && path[size - 1] == '\\')
				size--;
			for (;size && path[size - 1] != '\\'; size--);
			if (size)
			{
				path[size] = 0;
				LoadLECFromPath(path);
			}
		}
		RegCloseKey(key);
	}
}

bool LECWindow::LoadLECFromPath(char *path)
{
	strcat(path, PATH);
	if (hLEC = LoadLibraryA(path))
	{
		eg_init_t eg_init;
		eg_init2_t eg_init2 = (eg_init2_t)GetProcAddress(hLEC, "eg_init2");
		if (!eg_init2)
			eg_init = (eg_init_t)GetProcAddress(hLEC, "eg_init");
		eg_end = (eg_end_t)GetProcAddress(hLEC, "eg_end");
		eg_translate_multi = (eg_translate_multi_t)GetProcAddress(hLEC, "eg_translate_multi");
		if ((eg_init2 || eg_init) && eg_end && eg_translate_multi)
		{
			path[strlen(path) - sizeof(PATH) + 10] = 0;
			if (eg_init2 ? !eg_init2(path, 0) : !eg_init(path))
			{
				lecState = 1;
				return true;
			}
		}
		FreeLibrary(hLEC);
		hLEC = NULL;
	}
	return false;
}

int LECWindow::WndProc(LRESULT *output, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (uMsg == WMA_THREAD_DONE)
	{
		Stopped();
		TryStartTask();
		return 1;
	}
	return 0;
}
