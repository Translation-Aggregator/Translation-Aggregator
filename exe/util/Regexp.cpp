#include <Shared/Shrink.h>
#include "Regexp.h"

struct RegMatch
{
	wchar_t *start;
	wchar_t *end;
	wchar_t *regexpStart;
};

int RegexpMatch(wchar_t *s, wchar_t *regexp, RegMatch **matches)
{
	return 0;
}

int RegexpReplace(wchar_t *s, int len, int memLen, wchar_t *regexp, wchar_t *replace)
{
	RegMatch *matches = 0;
	int res;
	if (regexp[0] == '^' && regexp[1] == '^')
		res = RegexpMatch(s, regexp+2, &matches);
	else if (regexp[0] == '^')
	{
		for (int i=0; s[i]; i++)
			if (!i || s[i-1] == '\n')
			{
				res = RegexpMatch(s, regexp+1, &matches);
				if (res)
					break;
			}
	}
	else
		res = RegexpMatch(s, regexp, &matches);
	return len;
}
