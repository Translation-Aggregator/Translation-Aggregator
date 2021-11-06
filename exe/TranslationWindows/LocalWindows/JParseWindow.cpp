#include <Shared/Shrink.h>
#include "JParseWindow.h"
#include "../../util/Dictionary.h"

#include "../../Config.h"

#include "../../resource.h"

/*
// Decided this wasn't worth the effort.
int FindBestMatch(int *kanji, int *hiragana, EntryData *entry, wchar_t *string, Match *match)
{
	*kanji = *hiragana = -1;
	int bestJap = -1;
	for (int jap = entry->numJap-1; jap>=0; jap--)
	{
		if (entry->jap[jap]->verbType == match->conj[0].verbType && !wcsnijcmp(entry->jap[jap]->jstring, string, match->srcLen) && !entry->jap[jap]->jstring[match->srcLen])
		{
			bestJap = jap;
			if (!wcsnicmp(entry->jap[jap]->jstring, string, match->srcLen)) break;
		}
	}
	if (bestJap == -1) return 0;
	if (!(entry->jap[entry->numJap-1]->flags & JAP_WORD_PRONOUNCE))
	{
		*hiragana = bestJap;
		return 1;
	}
	if (entry->jap[bestJap] & JAP_WORD_PRONOUCE)
	{
		*hiragana = bestJap;
		int jap = bestJap - 1;
		while (jap >= 0 && (entry->jap[jap] & JAP_WORD_PRONOUCE)) jap--;
		bestJap = -1;
		for (; jap>=0; jap--)
			entry->jap[jap] & JAP_WORD_PRONOUCE) continue;
		*kanji = bestJap;
		return 1;
	}
}//*/

namespace
{

HWND hWndJParser;

void JparserCallback(JParserState state)
{
	PostMessage(hWndJParser, WMA_JPARSER_STATE, state, 0);
}

}  // namespace

class FindMatchesTask : public Thread::Task
{
public:
	FindMatchesTask(HWND hWnd, wchar_t* orig_string) :
		hWnd(hWnd), orig_string_(orig_string)
	{
	}

	void Run()
	{
		if (!hWnd)
			return;
		hWndJParser = hWnd;
		if (!DictsLoaded())
		{
			PostMessage(hWnd, WMA_THREAD_DONE, (WPARAM)0, (LPARAM)L"No dictionaries loaded");
			return;
		}
	wchar_t *t = (wchar_t*)orig_string_.c_str();
	for (int i = orig_string_.length(); --i >= 0;)
		//if (t[i] - '0' <= 9u)
		//	t[i] += L'０' - '0';
		AutoLock lock(dictionary_read_write_lock);
		Match *matches;
		int numMatches;
		FindBestMatches(orig_string_.c_str(), orig_string_.length(), matches, numMatches, config.jParserFlags & JPARSER_USE_MECAB, JparserCallback);

		if (numMatches) SortMatches(matches, numMatches);

		FuriganaWord *firstWord = 0;
		FuriganaWord *word = 0;
		int maxWords = 50;

		// All substrings concatenated and null terminated.
		// As rellocate it periodically, fix pointers in final pass.
		wchar_t *furiganaString = 0;
		wchar_t *end = 0;
		int size = 5000;
		int pos = -1;
		int endPos = 0;
		for (int i=0; i<=numMatches; i++)
		{
			if (i < numMatches && pos == matches[i].start)
			{
				EntryData entry;
				if (GetDictEntry(matches[i], &entry))
				{
					POSData pos;
					GetPartsOfSpeech(entry.english, &pos);
					if (pos.pos[POS_PART])
						word[-1].pos = POS_PART;
				}
				continue;
			}
			if (end-furiganaString >= size - 5000)
			{
				int pos = (int)(end-furiganaString);
				furiganaString = (wchar_t*) realloc(furiganaString, sizeof(wchar_t) * (size*=2));
				end = furiganaString+pos;
			}
			if (word-firstWord >= maxWords - 50)
			{
				int wordIndex = (int)(word-firstWord);
				firstWord = (FuriganaWord*) realloc(firstWord, sizeof(FuriganaWord) * (maxWords*=2));
				word = firstWord+wordIndex;
				memset(word, 0, sizeof(FuriganaWord)*(maxWords - wordIndex));
			}
			FuriganaWord *cword = 0;
			int loopEnd = orig_string_.length();
			if (i < numMatches) loopEnd = matches[i].start;
			while (endPos < loopEnd)
			{
				// Just in case...
				if (end-furiganaString >= size - 5000)
				{
					int pos = (int)(end-furiganaString);
					furiganaString = (wchar_t*) realloc(furiganaString, sizeof(wchar_t) * (size*=2));
					end = furiganaString+pos;
				}
				if (orig_string_[endPos] == '\n' || orig_string_[endPos] == ' ' || orig_string_[endPos] == '\t')
				{
					if (cword)
					{
						end++[0] = 0;
						cword = 0;
					}
					if (orig_string_[endPos] == '\n') word->pos = POS_LINEBREAK;
					word ++;
					if (word-firstWord >= maxWords - 50)
					{
						int wordIndex = (int)(word-firstWord);
						firstWord = (FuriganaWord*) realloc(firstWord, sizeof(FuriganaWord) * (maxWords*=2));
						word = firstWord+wordIndex;
						memset(word, 0, sizeof(FuriganaWord)*(maxWords - wordIndex));
					}
				}
				else
				{
					if (!cword)
					{
						word->word = (wchar_t*)1;
						cword = word++;
					}
					end++[0] = orig_string_[endPos];
				}
				endPos++;
			}
			if (cword) end++[0] = 0;
			if (i == numMatches) break;
			pos = matches[i].start;
			endPos = matches[i].start + matches[i].len;
			word->word = (wchar_t*)1;
			word->flags = WORD_HAS_TRANS;

			memcpy(end, orig_string_.c_str()+pos, sizeof(wchar_t) * (endPos-pos));
			end[endPos-pos] = 0;
			end+=endPos-pos+1;

			EntryData entry;
			// Don't label middle dot.
			if ((matches[i].len != 1 || orig_string_[matches[i].start] != 0x30FB) && GetDictEntry(matches[i], &entry))
			{
				int j;
				//int kanji, hiragana;
				//FindBestMatch(&kanji, &hiragana, &entry, string->string+matches[i].start, matches+i);
				int found = 0;
				for (j=0; j<entry.numJap; j++)
				{
					if (entry.jap[j]->flags & JAP_WORD_PRONOUNCE)
					{
						if (found)
						{
							if (matches[i].conj[0].verbType)
							{
								if (matches[i].conj[0].verbType == entry.jap[j]->verbType)
								{
									word->pro = (wchar_t*)1;
									wcscpy(end, entry.jap[j]->jstring);
									end = wcschr(end, 0);
									memcpy(end, orig_string_.c_str() + matches[i].srcLen + matches[i].start, sizeof(wchar_t)*(matches[i].len - matches[i].srcLen));
									end += matches[i].len - matches[i].srcLen;
									end++[0] = 0;
									break;
								}
								else if (entry.kuruHack[0] && entry.jap[j]->verbType)
								{
									word->pro = (wchar_t*)1;
									wcscpy(end, entry.jap[j]->jstring);
									wcscat(end, entry.kuruHack);
									end = wcschr(end, 0);
									memcpy(end, orig_string_.c_str() + matches[i].srcLen + matches[i].start + 1, sizeof(wchar_t)*(matches[i].len - matches[i].srcLen-1));
									end += matches[i].len - matches[i].srcLen-1;
									end++[0] = 0;
									break;
								}
								continue;
							}
							word->pro = entry.jap[j]->jstring;
						}
						break;
					}
					if (!found && !wcsnijcmp(entry.jap[j]->jstring, orig_string_.c_str()+matches[i].start, matches[i].srcLen))
						found = 1;
				}
			}

			word++;
			// lazy hack.
			i--;
		}

		Result* data = (Result*)malloc(sizeof(Result));
		data->numWords = word-firstWord;
		data->words = firstWord = (FuriganaWord*) realloc(firstWord, sizeof(FuriganaWord) * data->numWords);
		data->string = furiganaString = (wchar_t*) realloc(furiganaString, sizeof(wchar_t) * (end-furiganaString));
		for (int i=0; i<data->numWords; i++)
		{
			word = data->words + i;
			if (1 == (size_t) word->word)
			{
				word->word = furiganaString;
				furiganaString = 1+wcschr(furiganaString, 0);
			}
			if (1 == (size_t) word->pro)
			{
				word->pro = furiganaString;
				furiganaString = 1+wcschr(furiganaString, 0);
			}
		}

		free(matches);
		PostMessage(hWnd, WMA_THREAD_DONE, (WPARAM)data, 0);
	}

private:
	HWND hWnd;
	std::wstring orig_string_;
};

JParseWindow::JParseWindow() :
	FuriganaWindow(L"JParser", 0, L"http://www.edrdg.org/jmdict/edict_doc.html", TWF_SEPARATOR|TWF_CONFIG_BUTTON, NO_FURIGANA),
	thread_("JParser")
{
	// Tries to load dictionaries.
	thread_.PostTask(new FindMatchesTask(hWnd, L""));
}

JParseWindow::~JParseWindow()
{
}

void JParseWindow::Translate(SharedString *string, int history_id, bool only_use_history)
{
	ClearQueuedTask();
	if (!CanTranslate(config.langSrc, config.langDst))
	{
		CleanupResult();
		Reformat();
		return;
	}
	string->AddRef();
	queuedString = string;
	TryStartTask();
}

void JParseWindow::TryStartTask()
{
	if (!queuedString || busy)
		return;

	thread_.PostTask(new FindMatchesTask(hWnd, queuedString->string));
	busy = 1;
	ClearQueuedTask();
	CleanupResult();
	Reformat();
}

static JParseWindow *jParseWindow = 0;

INT_PTR CALLBACK JParseDialogProc(HWND hWndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
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
			if (config.jParserDefinitionLines) config.jParserFlags |= JPARSER_JAPANESE_OWN_LINE;
			for (int j=IDC_DISPLAY_VERB_CONJUGATIONS; j<=IDC_USE_MECAB; j++)
			{
				int state = BST_UNCHECKED;
				if (config.jParserFlags & (1<<(j-IDC_DISPLAY_VERB_CONJUGATIONS)))
					state = BST_CHECKED;
				CheckDlgButton(hWndDlg, j, state);
			}
			for (int i=0; i<4; i++)
			{
				wsprintfW(str, L"%06X", RGBFlip(jParseWindow->colors[i]));
				SetDlgItemText(hWndDlg, IDC_DEFAULT_COLOR + 2*i, str);
			}
			for (int i=0; i<4; i++)
			{
				wsprintfW(str, L"%06X", RGBFlip(config.colors[i]));
				SetDlgItemText(hWndDlg, IDC_KANJI_COLOR + 2*i, str);
			}
			SetDlgItemInt(hWndDlg, IDC_NORMAL_FONT_SIZE, jParseWindow->normalFontSize, 1);
			SetDlgItemInt(hWndDlg, IDC_FURIGANA_FONT_SIZE, jParseWindow->furiganaFontSize, 1);
			CheckDlgButton(hWndDlg, IDC_HIRAGANA + jParseWindow->characterType, BST_CHECKED);
			if (config.jParserHideCrossRefs) CheckDlgButton(hWndDlg, IDC_HIDE_CROSS_REFS, BST_CHECKED);
			if (config.jParserHideUsage) CheckDlgButton(hWndDlg, IDC_HIDE_USAGE, BST_CHECKED);
			if (config.jParserHidePos) CheckDlgButton(hWndDlg, IDC_HIDE_POS, BST_CHECKED);

			if (config.jParserDefinitionLines) CheckDlgButton(hWndDlg, IDC_DEFINITION_LINES, BST_CHECKED);
			if (config.jParserReformatNumbers) CheckDlgButton(hWndDlg, IDC_NUMBER_REFORMAT, BST_CHECKED);
			if (config.jParserNoKanaBrackets) CheckDlgButton(hWndDlg, IDC_NO_KANA_BRACKETS, BST_CHECKED);
			// Despite being disabled in the dialog box template, have to do this because
			// changing the text of the controls a few line back actually sent
			// an EN_CHANGE event.  Grr...
			EnableWindow(GetDlgItem(hWndDlg, IDC_APPLY), 0);
			break;
		}
		case WM_COMMAND:
			if (HIWORD(wParam) == EN_CHANGE)
				EnableWindow(GetDlgItem(hWndDlg, IDC_APPLY), 1);
			else if (HIWORD(wParam) == BN_CLICKED)
			{
				i = LOWORD(wParam);
				if (i >= 1000 && i <= 2000)
				{
					EnableWindow(GetDlgItem(hWndDlg, IDC_APPLY), 1);
					if (i == IDC_DEFINITION_LINES)
					{
						if (BST_CHECKED == IsDlgButtonChecked(hWndDlg, IDC_DEFINITION_LINES))
							CheckDlgButton(hWndDlg, IDC_JAPANESE_OWN_LINE, BST_CHECKED);
					}
					else if (i == IDC_JAPANESE_OWN_LINE)
					{
						if (BST_CHECKED != IsDlgButtonChecked(hWndDlg, IDC_JAPANESE_OWN_LINE))
							CheckDlgButton(hWndDlg, IDC_DEFINITION_LINES, BST_UNCHECKED);
					}
				}
				if (i == IDOK || i == IDC_APPLY)
				{
					if (i == IDC_APPLY) EnableWindow(GetDlgItem(hWndDlg, IDC_APPLY), 0);
					for (int j=IDC_DISPLAY_VERB_CONJUGATIONS; j<=IDC_USE_MECAB; j++)
					{
						int bit = (1<<(j-IDC_DISPLAY_VERB_CONJUGATIONS));
						config.jParserFlags &= ~bit;
						if (IsDlgButtonChecked(hWndDlg, j) == BST_CHECKED) config.jParserFlags |= bit;
					}
					for (int j=0; j<4; j++)
					{
						if (GetDlgItemText(hWndDlg, IDC_DEFAULT_COLOR + 2*j, str, 30))
						{
							wchar_t *end = 0;
							unsigned int color = wcstoul(str, &end, 16);
							jParseWindow->colors[j] = RGBFlip(color);
						}
					}
					for (int j=0; j<4; j++)
					{
						if (GetDlgItemText(hWndDlg, IDC_KANJI_COLOR + 2*j, str, 30))
						{
							wchar_t *end = 0;
							unsigned int color = wcstoul(str, &end, 16);
							config.colors[j] = RGBFlip(color);
						}
					}
					jParseWindow->normalFontSize = GetDlgItemInt(hWndDlg, IDC_NORMAL_FONT_SIZE, 0, 1);
					jParseWindow->furiganaFontSize = GetDlgItemInt(hWndDlg, IDC_FURIGANA_FONT_SIZE, 0, 1);
					jParseWindow->characterType = NO_FURIGANA;

					config.jParserHideCrossRefs = (BST_CHECKED == IsDlgButtonChecked(hWndDlg, IDC_HIDE_CROSS_REFS));
					config.jParserHideUsage = (BST_CHECKED == IsDlgButtonChecked(hWndDlg, IDC_HIDE_USAGE));
					config.jParserHidePos = (BST_CHECKED == IsDlgButtonChecked(hWndDlg, IDC_HIDE_POS));

					config.jParserDefinitionLines = (BST_CHECKED == IsDlgButtonChecked(hWndDlg, IDC_DEFINITION_LINES));
					config.jParserReformatNumbers = (BST_CHECKED == IsDlgButtonChecked(hWndDlg, IDC_NUMBER_REFORMAT));
					config.jParserNoKanaBrackets = (BST_CHECKED == IsDlgButtonChecked(hWndDlg, IDC_NO_KANA_BRACKETS));

					if (BST_CHECKED == IsDlgButtonChecked(hWndDlg, IDC_KATAKANA))
						jParseWindow->characterType = KATAKANA;
					else if (BST_CHECKED == IsDlgButtonChecked(hWndDlg, IDC_ROMAJI))
						jParseWindow->characterType = ROMAJI;
					else if (BST_CHECKED == IsDlgButtonChecked(hWndDlg, IDC_HIRAGANA))
						jParseWindow->characterType = HIRAGANA;
					jParseWindow->SetFont();
					// Since use global config object to share options with tooltip class,
					// have to save global config.
					SaveConfig();
					// jParseWindow->SaveWindowTypeConfig();
					if (i == IDOK)
						EndDialog(hWndDlg, 1);
				}
				else if (i == IDCANCEL)
					EndDialog(hWndDlg, 0);
				else if (i >= IDC_DEFAULT_COLOR && i < IDC_DEFAULT_COLOR+16 && ((i-IDC_DEFAULT_COLOR)&1))
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
						EnableWindow(GetDlgItem(hWndDlg, IDC_APPLY), 1);
					}
				}
			}
			break;
		default:
			break;
	}
	return 0;
}

int JParseWindow::WndProc(LRESULT *output, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (uMsg == WM_COMMAND)
	{
		if (HIWORD(wParam) == BN_CLICKED)
		{
			if (LOWORD(wParam) == ID_CONFIG)
			{
				jParseWindow = this;
				DialogBoxParam(ghInst, MAKEINTRESOURCE(IDD_JPARSER_CONFIG), hWnd, JParseDialogProc, 0);
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
	else if (uMsg == WMA_JPARSER_STATE)
	{
		if (wParam == SEARCHING_DICTIONARY)
			error = L"Searching dictionary...";
		else if (wParam == RUNNING_MECAB)
			error = L"Running MeCab...";
		else
			error = L"Finding best words...";
		Reformat();
		return 1;
	}
	return 0;
}

void JParseWindow::SaveWindowTypeConfig()
{
	FuriganaWindow::SaveWindowTypeConfig();
	WritePrivateProfileInt(windowType, L"Flags", flags, config.ini);
}
