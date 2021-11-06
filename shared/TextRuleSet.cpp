#include <Shared/Shrink.h>
#include "TextRuleSet.h"
#include <Shared/StringUtil.h>

wchar_t *GetFileText(wchar_t *path, wchar_t *fileName)
{
	if (!path || !path[0] || !fileName || wcslen(path) + wcslen(fileName) + 2 > 2*MAX_PATH) return 0;
	wchar_t name[2*MAX_PATH];
	wcscpy(name, path);
	wchar_t *e = wcschr(name, 0);
	if (e[-1] != '\\')
	{
		*e = '\\';
		e++;
	}
	wcscpy(e, fileName);
	HANDLE hFile = CreateFileW(name, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
	if (hFile == INVALID_HANDLE_VALUE) return 0;
	__int64 size;
	wchar_t *out = 0;
	if (GetFileSizeEx(hFile, (PLARGE_INTEGER)&size) && size > 2 && size < 1024*1024*100)
	{
		unsigned char *temp = (unsigned char*) malloc((DWORD)size+2);
		if (temp)
		{
			DWORD read = 0;
			if (ReadFile(hFile, temp, (DWORD)size, &read, 0) && read == size)
			{
				temp[size] = 0;
				temp[size+1] = 0;
				int len = 0;
				if (temp[0] == 0xEF &&
					temp[1] == 0xBB &&
					temp[2] == 0xBF)
					{
						// +1 is for the terminating null.
						int len = ((int)size-3)+1;
						out = ToWideChar((char*)temp+3, CP_UTF8, len);
				}
				else
				{
					char *temp2 = (char*)temp;
					int len = strchr(temp2, 0)-temp2;
					if (*(wchar_t*)temp2 == 0xFFFE || (len != size && !(len & 1)))
						for (int i=0; i<size/2; i++)
							((unsigned short*)temp2)[i] = (((unsigned short*)temp2)[i]<<8) + (((unsigned short*)temp2)[i]>>8);
					if (*(wchar_t*)temp2 == 0xFEFF)
					{
						temp2 += 2;
						len = 0;
						size -= 2;
					}
					if (len != size)
						out = wcsdup((wchar_t*)temp2);
					else
					{
						len++;
						out = ToWideChar(temp2, 932, len);
					}
				}
			}
			free(temp);
		}
	}
	CloseHandle(hFile);
	return out;
}

void FlushRule(TextRule &rule, TextRule **&rules, int &numRules)
{
	if (!rule.match || !rule.match[0]) return;
	rule.matchLen = wcslen(rule.match);
	int memSize = sizeof(TextRule) + (1+wcslen(rule.match)) * sizeof(wchar_t) + sizeof(Op) * rule.numOps;
	int i;
	for (i=0; i<rule.numOps; i++)
		if (rule.ops[i].data)
			memSize += (1+wcslen(rule.ops[i].data)) * sizeof(wchar_t);
	TextRule *newRule = (TextRule*) malloc(memSize);
	*newRule = rule;
	newRule->ops = (Op*)(newRule+1);
	wchar_t *pos = (wchar_t*) (newRule->ops + rule.numOps);
	newRule->match = pos;
	wcscpy(pos, rule.match);
	pos += wcslen(rule.match)+1;
	if (!rule.numOps)
		newRule->ops = 0;
	else
		for (i=0; i<rule.numOps; i++)
		{
			newRule->ops[i] = rule.ops[i];
			if (rule.ops[i].data)
			{
				newRule->ops[i].data = pos;
				wcscpy(pos, rule.ops[i].data);
				pos += wcslen(rule.ops[i].data)+1;
			}
		}
	rule.match = 0;
	rule.numOps = 0;
	if (numRules % 32 == 0)
		rules = (TextRule**) malloc(sizeof(TextRule*) * (numRules+32));
	rules[numRules] = newRule;
	numRules++;
}

void CleanupLine(wchar_t *&line)
{
	while (*line && MyIsWspace(*line))
		line++;
	wchar_t *e = wcschr(line, 0);
	if (e > line+1 && e[-1] == '"' && line[0] == '"')
	{
		line++;
		e[-1] = 0;
	}
}

int LoadRuleSetRecursive(TextRule **&rules, int &numRules, wchar_t *path, wchar_t *fileName)
{
	wchar_t *text = GetFileText(path, fileName);
	if (!text) return 0;
	SpiffyUp(text);
	wchar_t *pos = text;
	Op *ops = (Op*) malloc(2000 * sizeof(Op));
	TextRule rule = {0,0,ops,0};
	while (*pos)
	{
		if (MyIsWspace(*pos))
		{
			pos++;
			continue;
		}
		int lineLen = wcscspn(pos, L"\n");
		wchar_t *line = pos;
		pos += lineLen;
		wchar_t *e = pos;
		if (*pos)
			pos++[0] = 0;
		while (e > line && MyIsWspace(e[-1]))
		{
			e--;
			*e = 0;
		}
		if (!wcsnicmp(line, L"//", 2)) continue;
		int type = 0;
		if (!wcsnicmp(line, L"include ", 8))
		{
			line += 8;
			CleanupLine(line);
			LoadRuleSetRecursive(rules, numRules, path, line);
			continue;
		}
		int bonusType = 0;
		wchar_t *origLine = line;
		if (!wcsnicmp(line, L"line ", 5))
		{
			line += 5;
			bonusType |= 8;
		}
		if (!wcsnicmp(line, L"break before ", 13))
		{
			type = 1 | bonusType;
			line += 13;
		}
		else if (!wcsnicmp(line, L"break after ", 12))
		{
			type = 4 | bonusType;
			line += 12;
		}
		else if (!wcsnicmp(line, L"break start before ", 18))
		{
			type = 3 | bonusType;
			line += 18;
		}
		wchar_t *temp;
		if (type)
		{
			CleanupLine(line);
			FlushRule(rule, rules, numRules);
			int numOps = 0;
			memset(ops, 0, sizeof(Op) * 5);
			if (type & 1)
			{
				ops[numOps++].op = OP_END_SENTENCE;
				if (type & 8)
				{
					ops[numOps].op = OP_OUTPUT_STRING;
					ops[numOps].data = L"\n";
					numOps++;
				}
			}
			if (type &= 2)
				ops[numOps++].op = OP_START_SENTENCE;
			//ops[numOps].data = line;
			ops[numOps++].op = OP_OUTPUT;
			if (type & 4)
			{
				ops[numOps++].op = OP_END_SENTENCE;
				if (type & 8)
				{
					ops[numOps].op = OP_OUTPUT_STRING;
					ops[numOps].data = L"\n";
					numOps++;
				}
			}
			rule.numOps = numOps;
			rule.match = line;
			FlushRule(rule, rules, numRules);
		}
		else if (!wcsnicmp(line, L"replace ", 8) && (temp = wcsstr(line, L" with ")))
		{
			line += 8;
			temp[0] = 0;
			temp += 6;
			CleanupLine(line);
			CleanupLine(temp);
			FlushRule(rule, rules, numRules);
			memset(ops, 0, sizeof(Op));
			ops[0].op = OP_OUTPUT_STRING;
			ops[0].data = temp;
			rule.numOps = 1;
			rule.match = line;
			FlushRule(rule, rules, numRules);
		}
		else
			line = origLine;
		/*
		else if (!wcsnicmp(line, L"break\n", 6))
		{
		}
		else if (!wcsnicmp(line, L"start\n", 6))
		{
		}
		else if (!wcsnicmp(line, L"break", 5) && MyIsWspace(line[5]))
		{
		}
		else if (!wcsnicmp(line, L"output", 6) && MyIsWspace(line[6]))
		{
		}//*/
	}
	FlushRule(rule, rules, numRules);
	free(ops);
	free(text);
	return 1;
}

TextRuleSet *LoadRuleSet(wchar_t *path, wchar_t *fileName, unsigned int flags)
{
	TextRuleSet *out = new TextRuleSet();
	out->flags = flags;
	out->rules = 0;
	out->numRules = 0;
	LoadRuleSetRecursive(out->rules, out->numRules, path, fileName);
	return out;
}

TextRuleSet::~TextRuleSet()
{
	if (numRules)
	{
		for (int i=0; i<numRules; i++)
			free(rules[i]);
		free(rules);
	}
}


wchar_t *TextRuleSet::ParseText(wchar_t *text, int *len, int *count)
{
	count[0] = 1;
	wchar_t *start = text;
	int maxOutLen = *len+10;
	int outLen = 0;
	wchar_t *out = (wchar_t*) malloc(sizeof(wchar_t) * maxOutLen);
	int transText = 0;
	while (*text)
	{
		if (maxOutLen < outLen + 6)
		{
			maxOutLen = 10+2 * maxOutLen;
			out = (wchar_t*) realloc(out, maxOutLen * sizeof(wchar_t));
		}
		int i;
		for (i=0; i<numRules; i++)
		{
			wchar_t *cmp = rules[i]->match;
			int len = rules[i]->matchLen;
			if (*cmp == '^')
			{
				cmp++;
				len--;
				if (!len) continue;
				if (text != start && text[-1] != '\n') continue;
			}
			if (!wcsnicmp(cmp, text, len))
			{
				if (maxOutLen < outLen + 6 + rules[i]->numOps)
				{
					maxOutLen = 10+2 * maxOutLen+rules[i]->numOps;
					out = (wchar_t*) realloc(out, maxOutLen * sizeof(wchar_t));
				}
				for (int j=0;j<rules[i]->numOps; j++)
				{
					OpCode op = rules[i]->ops[j].op;
					if (op == OP_END_SENTENCE)
					{
						if (transText)
						{
							out[outLen++] = 0;
							transText = 0;
							count[0]++;
						}
					}
					else if (op == OP_START_SENTENCE)
					{
						if (!transText)
						{
							out[outLen++] = 0;
							transText = 1;
						}
					}
					else if (op == OP_OUTPUT)
					{
						if (maxOutLen < outLen + 6 + len + rules[i]->numOps)
						{
							maxOutLen = 10+2 * maxOutLen+len+rules[i]->numOps;
							out = (wchar_t*) realloc(out, maxOutLen * sizeof(wchar_t));
						}
						memcpy(out+outLen, text, len*sizeof(wchar_t));
						outLen += len;
					}
					else if (op == OP_OUTPUT_STRING)
					{
						wchar_t *s = rules[i]->ops[j].data;
						int slen = wcslen(s);
						if (maxOutLen < outLen + 6 + slen + rules[i]->numOps)
						{
							maxOutLen = 10+2 * maxOutLen+slen+rules[i]->numOps;
							out = (wchar_t*) realloc(out, maxOutLen * sizeof(wchar_t));
						}
						memcpy(out+outLen, s, slen*sizeof(wchar_t));
						outLen += slen;
					}
				}
				text += len;
				break;
			}
		}
		if (i != numRules) continue;

		wchar_t c = *text;
		if (transText && (IsMajorPunctuation(c, flags) || (c == '\n' && text[1] == '\n')))
		{
			int pos = outLen;
			while (pos && IsPunctuation(out[pos-1]))
				pos--;
			if (pos != outLen || (c != L'。' && c != L'？' && c != L'！'))
			{
				// Simplest way yo make sure punctuation is converted.
				text -= outLen-pos;
				out[pos] = 0;
				outLen = pos+1;
				transText = 0;
				count[0]++;
				continue;
			}
			else
			{
				out[outLen]  = *text;
				out[outLen+1]  = 0;
			}
			outLen += 2;
			transText = 0;
			count[0]++;
			if (out[outLen-1] == L'…')
			{
				wcscpy(&out[outLen-1], L"...");
				outLen += 2;
			}
			else if (out[outLen-1] == L'‥')
			{
				wcscpy(&out[outLen-1], L"..");
				outLen ++;
			}
		}
		else if (!transText && IsPunctuation(c))
		{
			if (c == L'…')
			{
				wcscpy(&out[outLen], L"...");
				outLen += 3;
			}
			else if (c == L'‥')
			{
				wcscpy(&out[outLen], L"..");
				outLen +=2;
			}
			else
			{
				if (c == L'。')
					c = '.';
				else if (c == L'？')
					c = '?';
				else if (c == L'！')
					c = '!';
				else if (c == L'　')
					c = ' ';
				else if (c == L'「')
					c = '"';
				else if (c == L'」')
					c = '"';
				else if (c == L'（')
					c = '(';
				else if (c == L'）')
					c = ')';
				else if (c == L'、')
					c = ',';
				out[outLen++] = c;
			}
		}
		else
		{
			if (!transText && !MyIsWspace(c))
			{
				transText = 1;
				out[outLen++] = 0;
			}
			out[outLen++] = c;
		}
		text++;
	}
	out = (wchar_t*) realloc(out, sizeof(wchar_t) * (outLen+3));
	memset(out + outLen, 0, 3*sizeof(wchar_t));
	*len = outLen;

	return out;
}
