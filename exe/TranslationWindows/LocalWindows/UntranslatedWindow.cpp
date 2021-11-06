#include <Shared/Shrink.h>
#include "UntranslatedWindow.h"
#include "../../Config.h"

#include "../../resource.h"

UntranslatedWindow::UntranslatedWindow() : TranslationWindow(L"Original Text", 0)
{
	transHighlighted = GetPrivateProfileIntW(windowType, L"Trans Highlighted", 0, config.ini);
}

void UntranslatedWindow::SaveWindowTypeConfig()
{
	WritePrivateProfileInt(windowType, L"Trans Highlighted", transHighlighted, config.ini);
}

void UntranslatedWindow::Translate(SharedString *text, int history_id, bool only_use_history)
{
}

void UntranslatedWindow::AddClassButtons()
{
	TBBUTTON tbButtons[] =
	{
		{SEPARATOR_WIDTH, 0,   0, BTNS_SEP, {0}, 0, 0},
		//{0, ID_TRANSLATE, TBSTATE_ENABLED, BTNS_AUTOSIZE | BTNS_SHOWTEXT | TBSTYLE_BUTTON, {0}, 0, (INT_PTR)windowType},
		{8, ID_TRANS_HIGHLIGHTED, TBSTATE_ENABLED | (TBSTATE_CHECKED * (transHighlighted != 0)), BTNS_CHECK, {0}, 0, (INT_PTR)L"Auto Translate Highlighted Text"},
		//{7, ID_TOPMOST, TBSTATE_ENABLED | (TBSTATE_CHECKED * (config.topmost != 0)), BTNS_CHECK, {0}, 0, (INT_PTR)L"Always on Top"},
	};

	TBBUTTONINFO info =
	{
		sizeof(info), TBIF_COMMAND | TBIF_IMAGE,
		ID_TRANSLATE_ALL, 1, 0, 0, 0, 0, 0, 0
	};

	SendMessage(hWndToolbar, TB_SETBUTTONINFO, ID_TRANSLATE, (LPARAM)&info);
	SendMessage(hWndToolbar, TB_ADDBUTTONS, (WPARAM)sizeof(tbButtons)/sizeof(tbButtons[0]), (LPARAM)&tbButtons);
}

void SetChecks();

int UntranslatedWindow::WndProc (LRESULT *output, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
		case WM_LBUTTONUP:
			wParam=wParam;
			break;
		case WM_NOTIFY:
		{
			NMHDR *hdr = (NMHDR*)lParam;
			if (hdr->code == EN_MSGFILTER)
			{
				MSGFILTER *msg = (MSGFILTER*)hdr;
				if (transHighlighted)
				{
					if(msg->msg == WM_LBUTTONDOWN)
					{
						// Prevent redundant translation on focus, at the cost
						// of losing current selection on app focus.
		//				SendMessage(hWndEdit, EM_SETSEL, -1, -1);
					}
					else if (msg->msg == WM_LBUTTONUP)
						SendMessage(hWndSuperMaster, WMA_TRANSLATE_HIGHLIGHTED, 0, 0);
				}
			}
			break;
		}
		case WM_COMMAND:
		//if (lParam)
			if (HIWORD(wParam) == BN_CLICKED)
			{
				if (LOWORD(wParam) == ID_TRANSLATE_ALL)
				{
					SendMessage(hWndSuperMaster, WMA_TRANSLATE_ALL, 0, 0);
					return 0;
				}
				else if (LOWORD(wParam) == ID_TOPMOST)
				{
					PostMessage(hWndParent, WM_COMMAND, wParam, lParam);
					return 0;
				}
				else if (LOWORD(wParam) == ID_TRANS_HIGHLIGHTED)
				{
					int w = SendMessage(hWndToolbar, TB_GETSTATE, ID_TRANS_HIGHLIGHTED, 0);
					transHighlighted = ((w & TBSTATE_CHECKED) != 0);
					SaveGeneralConfig();
					return 0;
				}
			}
			break;
		default:
			break;
	}
	return 0;
}
