#include <Shared/Shrink.h>
#include "Dictionary.h"
#include "DictionaryUtil.h"
#include "../Config.h"
#include "../Dialogs/MyToolTip.h"

int PopupDef(wchar_t *word, RECT &clientRect, RECT &globalRect, HWND hWnd)
{
	if (!DictsLoaded()) return 0;

	AutoLock lock(dictionary_read_write_lock);
	Match *matches;
	int numMatches;
	FindExactMatches(word, wcslen(word), matches, numMatches);

	if (!numMatches) return 0;
	// Puts like matches next to each other.  May do better eventually.
	SortMatches(matches, numMatches);

	EntryData entry;

	int size = 20000;
	wchar_t *out = (wchar_t *) malloc(sizeof(wchar_t) * size);
	wchar_t *end = out;
	*end = 0;
	for (int i=0; i<numMatches; i++)
	{
		if (end-out > size - 5000)
		{
			int pos = (int)(end-out);
			out = (wchar_t*) realloc(out, sizeof(wchar_t) * (size*=2));
			end = out+pos;
		}
		if (GetDictEntry(matches[i].dictIndex, matches[i].firstJString, &entry))
		{
			if (i)
				end++[0] = '\n';
			do
			{
				if (config.jParserFlags & JPARSER_DISPLAY_VERB_CONJUGATIONS)
				{
					wchar_t temp[1000];
					int cLen = GetConjString(temp+1, matches+i);
					if (cLen)
					{
						temp[0] = 1;
						wcscpy(end, temp);
						end += cLen+1;
						wcscat(end, L"\n");
						end += wcslen(end);
					}
				}
			}
			while (i < numMatches-1 && matches[i].firstJString == matches[i+1].firstJString && matches[i].dictIndex == matches[i+1].dictIndex && ++i);
			//end++[0] = ' ';
			//end++[0] = ' ';
			int bracket = 0;
			for (int j=0; j<entry.numJap; j++)
			{
				if (entry.jap[j]->verbType) continue;
				if (!bracket && (entry.jap[j]->flags&JAP_WORD_PRONOUNCE))
				{
					end++[0] = ' ';
					end++[0] = 0x3010;
					bracket = 1;
				}
				else
					if (j)
					{
						end++[0] = ';';
						end++[0] = ' ';
					}
				wcscpy(end, entry.jap[j]->jstring);
				end += wcslen(entry.jap[j]->jstring);
			}
			if (bracket) end++[0] = 0x3011;
			wchar_t *endJap = end;
			end++[0] = ' ';

			if (matches[i].matchFlags & MATCH_IS_NAME)
			{
				wcscpy(end, L"(proper name) ");
				end = wcschr(end, 0);
			}

			int mem = size - (end-out);

			int len = MultiByteToWideChar(CP_UTF8, 0, entry.english, -1, end, mem);
			if (len >= mem) continue;
			end+=len-1;
			if (((config.jParserFlags & JPARSER_JAPANESE_OWN_LINE) || config.jParserDefinitionLines) && !(matches[i].matchFlags & MATCH_IS_NAME))
			{
				/*if (endJap[1] == '(' && endJap[2] >= '9' && wcsnicmp(endJap+2, L"See", 3)) {
					wchar_t *q = endJap;
					while (*q && q[-1] != ')') q++;
					if (q[-1] == ')' && q[0] == ' ')
						endJap = q;
				}//*/
				endJap[0] = '\r';
			}
		}
	}
	out = (wchar_t*) realloc(out, sizeof(wchar_t)*(end+1-out));
	free(matches);
	ShowToolTip(out, hWnd, &globalRect);
	return 1;
}
