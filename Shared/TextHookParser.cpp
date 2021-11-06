#include <Shared/Shrink.h>
#include "TextHookParser.h"
#include "HookEval.h"

void Capitalize(wchar_t *s)
{
	while (*s)
	{
		*s = towupper(*s);
		s++;
	}
}

TextHookInfo *CreateTextHookInfo(wchar_t *hookText, wchar_t *alias, void* address, unsigned int type, wchar_t *value, wchar_t *context, wchar_t *length, wchar_t *codePage, int defaultFilter)
{
	if (!address || !value)
		return 0;
	if (!context)
		context = L"";
	if (length && !length[0])
		length = 0;
	if (codePage && !codePage[0])
		codePage = 0;

	int numContexts = 1;
	wchar_t *temp = context;
	while (temp = wcschr(temp, ';'))
	{
		numContexts++;
		temp++;
	}

	TextHookInfo *info = (TextHookInfo*) calloc(sizeof(TextHookInfo) + sizeof(CompiledExpression*) * (numContexts+1), 1);
	info->defaultFilter = defaultFilter;
	if (type & TEXT_FLIP)
	{
		info->flip = 1;
		type &= ~TEXT_FLIP;
	}
	info->contexts = (CompiledExpression**)(info+1);
	int error = 0;

	CompiledExpression **currentContext = info->contexts;
	wchar_t *tempContext = wcsdup(context);
	context = tempContext;
	while (1)
	{
		wchar_t *contextEnd = context;
		while (contextEnd[0] && contextEnd[0] != ';')
			contextEnd++;
		if (contextEnd == context)
		{
			currentContext[0] = CompileExpression(L"_0");
			if (!currentContext[0])
				error = 1;
		}
		// Ignore context if just "0".
		else if (contextEnd != context+1 || context[0] != '0')
		{
			wchar_t c = contextEnd[0];
			contextEnd[0] = 0;
			currentContext[0] = CompileExpression(context);
			contextEnd[0] = c;
			if (!currentContext[0])
				error = 1;
		}
		if (currentContext[0])
			currentContext++;
		if (!contextEnd[0]) break;
		context = contextEnd+1;
	}
	free(tempContext);

	info->address = address;
	info->type = type;

	info->hookText = wcsdup(hookText);
	if (!alias)
		info->alias = wcsdup(info->hookText);
	else
	{
		info->alias = (wchar_t*) malloc((wcslen(alias)+3)*sizeof(wchar_t));
		wsprintf(info->alias, L"[%s]", alias);
	}

	info->value = CompileExpression(value);
	if (!info->value)
		error = 1;

	if (length)
	{
		info->length = CompileExpression(length);
		if (!info->length)
			error = 1;
	}

	if (codePage)
	{
		info->codePage = CompileExpression(codePage);
		if (!info->codePage)
			error = 1;
	}
	if (error)
	{
		CleanupTextHookInfo(info);
		return 0;
	}
	return info;
}

void *ResolveAddress(wchar_t *str, int testOnly)
{
	wchar_t *end;
	ULONG_PTR offset = wcstoul(str, &end, 16);
	// ???
	if (!end)
		return 0;
	if (end[0] == 0 || !wcscmp(end, L":") || !wcscmp(end, L"::"))
		return (void*)offset;
	if (end[0] != ':')
	{
		offset = 0;
		end = str;
	}
	else
		end++;
	wchar_t *module = end;
	wchar_t *fxn = wcschr(end, ':');
	if (fxn)
	{
		*fxn = 0;
		fxn ++;
	}
	// These are allowed in function names.
	if (wcschr(module, '@'))
		return 0;
	HMODULE hMod = 0;
	if (!testOnly)
	{
		hMod = GetModuleHandle(module);
		if (!hMod)
			hMod = LoadLibrary(module);
		if (!hMod)
			return 0;
	}
	void *base = hMod;
	if (fxn)
	{
		int i;
		fxn[-1] = 0;
		if (*fxn == '#')
		{
			ULONG_PTR ordinal = wcstoul(fxn+1, &end, 0);
			base = 0;
			if (*end && ordinal < (1<<16))
			{
				if (testOnly) return (void*)1;
				base = reinterpret_cast<void*>(GetProcAddress(hMod, (char*)ordinal));
			}
			if (!base) return 0;
		}
		else if (*fxn)
		{
			char *temp = (char*)malloc(wcslen(fxn)+1);
			for (i=0; fxn[i]; i++)
				temp[i] = (char)fxn[i];
			temp[i] = 0;
			if (testOnly)
			{
				free(temp);
				return (void*)1;
			}
			base = reinterpret_cast<void*>(GetProcAddress(hMod, temp));
			free(temp);
			if (!base) return 0;
		}
	}
	if (testOnly) return (void*)1;
	return (char*)base + offset;
}

struct TypeInfo
{
	wchar_t *str;
	unsigned int flags;
	unsigned int canFlip;
	unsigned int charSet;
};

TextHookInfo *ParseTextHookString(wchar_t *string, int testOnly)
{
	if (!string) return 0;
	TextHookInfo *out = 0;
	int defaultFilter = 0;
	wchar_t *hook = wcsdup(string);
	const static TypeInfo types[] =
	{
		{L"char",    TEXT_CHAR , 1, 0},
		{L"wchar",   TEXT_WCHAR, 1, 0},
		{L"UTF8",    TEXT_CHAR , 0, CP_UTF8},
		{L"UTF16",   TEXT_WCHAR, 1, 0},
		{L"SJIS",    TEXT_CHAR, 1, 932},
	};

	wchar_t *alias = 0;
	wchar_t *commandStart = hook;
	if (commandStart[0] == '[')
	{
		wchar_t *aliasEnd = wcschr(commandStart, ']');
		if (aliasEnd)
		{
			*aliasEnd = 0;
			alias = commandStart+1;
			commandStart = aliasEnd+1;
		}
	}
	wchar_t *addrString = wcschr(commandStart, '@');
	if (addrString)
	{
		void *addr;
		if (addr = ResolveAddress(addrString+1, testOnly))
		{
			if (!wcsnicmp(commandStart, L"filter::", 8))
			{
				commandStart += 8;
				defaultFilter = 1;
			}
			*addrString = 0;
			wchar_t *type = wcstok(commandStart, L":");
			if (type)
			{
				int i;
				wchar_t *extType = 0;
				for (i=0; i<sizeof(types)/sizeof(types[0]); i++)
				{
					if (!wcsnicmp(types[i].str, type, wcslen(types[i].str)))
					{
						extType = type + wcslen(types[i].str);
						break;
					}
				}
				if (extType)
				{
					wchar_t temp[20];
					unsigned int flags = types[i].flags;
					if (!wcsnicmp(extType, L"be", 2))
					{
						extType += 2;
						if (types[i].canFlip)
							flags |= TEXT_FLIP;
					}
					if (*extType == '*')
					{
						flags |= TEXT_PTR;
						extType++;
					}
					wchar_t *args[4] = {0,0,0,0};
					if (types[i].charSet)
					{
						args[3] = temp;
						wsprintf(temp, L"%i", types[i].charSet);
					}
					if (!*extType)
					{
						unsigned int count = 0;
						wchar_t *param;
						while (param = wcstok(0, L":"))
						{
							if (count >= 4) break;
							if (param[0])
							{
								if (args[count])
								{
									// Really an error.  Current just override.
								}
								args[count] = param;
							}
							else if (args[0])
							{
								// :(
								count = 0;
								break;
							}
							count++;
						}
						if (count > 0 && count < 4)
							out = CreateTextHookInfo(string, alias, addr, flags, args[0], args[1], args[2], args[3], defaultFilter);
					}
				}
			}
		}
	}
	free(hook);
	return out;
}

void CleanupTextHookInfo(TextHookInfo *info)
{
	free(info->value);
	while (info->contexts[0])
		free(info->contexts++[0]);
	free(info->length);
	free(info->codePage);
	free(info->alias);
	free(info->hookText);
	free(info);
}
