#include <Shared/Shrink.h>
#include "MecabWindow.h"
#include "../../util/Mecab.h"
#include <Shared/StringUtil.h>
#include <Shared/ProcessUtil.h>
#include "../../config.h"

#include "../../resource.h"

static MecabWindow *mecabWindow;

class MecabTask : public Thread::Task
{
public:
	MecabTask(HWND hWnd, wchar_t* orig_string) :
		hWnd(hWnd), orig_string_(orig_string)
	{
	}

	void Run()
	{
		wchar_t* error;
		wchar_t* text = MecabParseString(orig_string_.c_str(), orig_string_.length(), &error);
		if (!text)
		{
			PostMessage(hWnd, WMA_THREAD_DONE, (WPARAM)0, (LPARAM)error);
			return;
		}
		int i;
		Result* data = (Result*) calloc(1, sizeof(Result));
		int wc = 0;
		wchar_t *pos = text;

		// Mecab's line breaks.  Each means one word.
		while (pos = wcschr(pos, '\n'))
		{
			pos ++;
			wc++;
		}
		pos = text;

		// My line breaks.  Each separates 2 words on a single line.
		// +2 includes word break plus one word.  Other word is already counter.
		while (pos = wcschr(pos, '\r'))
		{
			pos ++;
			wc += 2;
		}
		pos = text;
		data->words = (FuriganaWord*) calloc(wc, sizeof(FuriganaWord));
		while (*pos)
		{
			while (*pos != '\r' && iswspace(*pos)) pos++;
			wchar_t *end = wcschr(pos, '\n');
			if (!end) break;
			FuriganaWord * w = &data->words[data->numWords++];
			*end = 0;
			if (wcsicmp(pos, L"EOS"))
			{
				wchar_t *tab = wcschr(pos, '\t');
				if (tab)
				{
					tab[0] = 0;
					wchar_t *cr;
					if (cr = wcschr(pos, '\r'))
					{
						while (1)
						{
							if (cr != pos)
							{
								w->word = pos;
								*cr = 0;
								pos = cr;
								w = &data->words[data->numWords++];
							}
							w->pos = POS_LINEBREAK;
							pos = cr + 1;
							cr = wcschr(pos, '\r');
							if (!cr) break;

							w = &data->words[data->numWords++];
						}
						if (!pos[0])
						{
							pos = end+1;
							continue;
						}
					}
					w->word = pos;
					wchar_t *sub[10];
					sub[0] = tab+1;
					for (i=1; i<10; i ++)
					{
						sub[i] = wcschr(sub[i-1], ',');
						if (!sub[i]) break;
						*sub[i] = 0;
						sub[i]++;
					}
					if (i >= 7)
					{
						for (int j=0; j<sizeof(FuriganaPartsOfSpeech)/sizeof(FuriganaPartsOfSpeech[0]); j++)
						{
							if (!wcsicmp(FuriganaPartsOfSpeech[j].string, sub[0]))
							{
								w->pos = j;
								break;
							}
						}
						if (wcscmp(sub[6], pos) && sub[6][0] != '*')
							w->srcWord = sub[6];
						if (i >= 9)
							if (wcsijcmp(w->word, sub[7]) && sub[7][0] != '*')
								w->pro = sub[7];
					}
				}
			}
			pos = end+1;
		}
		data->words = (FuriganaWord*) realloc(data->words, sizeof(FuriganaWord) * wc);
		int len = 0;
		for (i=0; i<data->numWords; i++)
		{
			for (int j=0; j<sizeof(data->words->strings) / sizeof(wchar_t*); j++)
				if (data->words[i].strings[j]) len += 1 + wcslen(data->words[i].strings[j]);
		}

		pos = data->string = (wchar_t*) malloc(sizeof(wchar_t)*len);
		for (i=0; i<data->numWords; i++)
		{
			for (int j=0; j<sizeof(data->words->strings) / sizeof(wchar_t*); j++)
			{
				if (data->words[i].strings[j])
				{
					wcscpy(pos, data->words[i].strings[j]);
					data->words[i].strings[j] = pos;
					pos += wcslen(pos)+1;
				}
			}
		}
		free(text);
		PostMessage(hWnd, WMA_THREAD_DONE, (WPARAM)data, (LPARAM)error);
	}

private:
	HWND hWnd;
	std::wstring orig_string_;
};

MecabWindow::MecabWindow() :
	FuriganaWindow(L"Mecab", 0, L"https://code.google.com/p/mecab/", TWF_SEPARATOR|TWF_CONFIG_BUTTON, HIRAGANA),
	thread_("MeCab")
{
	mecabWindow = this;
}

MecabWindow::~MecabWindow()
{
	ClearQueuedTask();
	mecabWindow = 0;
}


void MecabWindow::TryStartTask()
{
	if (!queuedString || busy)
		return;

	thread_.PostTask(new MecabTask(hWnd, queuedString->string));
	busy = 1;
	ClearQueuedTask();
	CleanupResult();
	Reformat();
}

void MecabWindow::Translate(SharedString *string, int history_id, bool only_use_history)
{
	ClearQueuedTask();
	if (!CanTranslate(config.langSrc, config.langDst))
	{
		CleanupResult();
		topWord = 0;
		Reformat();
		return;
	}
	string->AddRef();
	queuedString = string;
	TryStartTask();
}

INT_PTR CALLBACK MecabDialogProc(HWND hWndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	int i;
	wchar_t str[30];
	switch (uMsg)
	{
#ifdef SETSUMI_CHANGES
		case WM_SHOWWINDOW: //hack - fix keyboard input on freshly shown dialogs
			if (wParam) {
				HWND hwndOk = GetDlgItem(hWndDlg, IDOK);
				SetActiveWindow(hWndDlg);
				SetFocus(hwndOk);
			}
			break;
#endif
		case WM_INITDIALOG:
			{
				for (int i=0; i<3; i++)
				{
					wsprintfW(str, L"%06X", RGBFlip(mecabWindow->colors[i]));
					SetDlgItemText(hWndDlg, IDC_DEFAULT_COLOR + 2*i, str);
				}
				SetDlgItemInt(hWndDlg, IDC_NORMAL_FONT_SIZE, mecabWindow->normalFontSize, 1);
				SetDlgItemInt(hWndDlg, IDC_FURIGANA_FONT_SIZE, mecabWindow->furiganaFontSize, 1);
				CheckDlgButton(hWndDlg, IDC_HIRAGANA + mecabWindow->characterType, BST_CHECKED);
			}
			break;
		case WM_COMMAND:
			if (HIWORD(wParam) == BN_CLICKED)
			{
				i = LOWORD(wParam);
				if (i == IDOK || i == IDC_APPLY)
				{
					for (int j=0; j<3; j++)
					{
						if (GetDlgItemText(hWndDlg, IDC_DEFAULT_COLOR + 2*j, str, 30))
						{
							wchar_t *end = 0;
							unsigned int color = wcstoul(str, &end, 16);
							mecabWindow->colors[j] = RGBFlip(color);
						}
					}
					mecabWindow->normalFontSize = GetDlgItemInt(hWndDlg, IDC_NORMAL_FONT_SIZE, 0, 1);
					mecabWindow->furiganaFontSize = GetDlgItemInt(hWndDlg, IDC_FURIGANA_FONT_SIZE, 0, 1);
					mecabWindow->characterType = HIRAGANA;
					if (BST_CHECKED == IsDlgButtonChecked(hWndDlg, IDC_KATAKANA))
						mecabWindow->characterType = KATAKANA;
					else if (BST_CHECKED == IsDlgButtonChecked(hWndDlg, IDC_ROMAJI))
						mecabWindow->characterType = ROMAJI;
					mecabWindow->SetFont();
					mecabWindow->SaveWindowTypeConfig();
					if (i == IDOK)
						EndDialog(hWndDlg, 1);
				}
				else if (i == IDCANCEL)
					EndDialog(hWndDlg, 0);
				else if (i >= IDC_DEFAULT_COLOR && i < IDC_DEFAULT_COLOR+6 && ((i-IDC_DEFAULT_COLOR)&1))
				{
					int index = (i-IDC_DEFAULT_COLOR)/2;
					unsigned int color = 0;
					if (GetDlgItemText(hWndDlg, i-1, str, 30))
						color = RGBFlip(wcstoul(str, 0, 16));
					CHOOSECOLOR cc;
					cc.lStructSize = sizeof(cc);
					cc.hwndOwner = hWndDlg;
					cc.rgbResult = color;
					static COLORREF customColors[16] =
					{
						0xFFFFFF,0xFFFFFF,0xFFFFFF,0xFFFFFF,
						0xFFFFFF,0xFFFFFF,0xFFFFFF,0xFFFFFF,
						0xFFFFFF,0xFFFFFF,0xFFFFFF,0xFFFFFF,
						0xFFFFFF,0xFFFFFF,0xFFFFFF,0xFFFFFF,
					};
					cc.lpCustColors = customColors;
					cc.Flags = CC_ANYCOLOR | CC_RGBINIT | CC_FULLOPEN;
					if (ChooseColor(&cc))
					{
						wsprintfW(str, L"%06X", RGBFlip(cc.rgbResult));
						SetDlgItemText(hWndDlg, i-1, str);
					}
				}
			}
			break;
		default:
			break;
	}
	return 0;
}

int MecabWindow::WndProc(LRESULT *output, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (uMsg == WM_COMMAND)
	{
		if (HIWORD(wParam) == BN_CLICKED)
		{
			if (LOWORD(wParam) == ID_CONFIG)
			{
				DialogBoxParam(ghInst, MAKEINTRESOURCE(IDD_MECAB_CONFIG), hWnd, MecabDialogProc, 0);
				return 1;
			}
		}
	}
	else if (uMsg == WMA_THREAD_DONE)
	{
		Stopped();
		TryStartTask();
		CleanupResult();
		error = (wchar_t*) lParam;
		if (wParam)
			data = (Result*)wParam;
		Reformat();
	}
	return 0;
}
