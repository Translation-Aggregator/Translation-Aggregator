#include <Shared/Shrink.h>
#include <Shared/StringUtil.h>


struct EscapeCode
{
	wchar_t code;
	char string[8];
};
wchar_t *UnescapeHtml(wchar_t *out, int eatBRs)
{
	static const EscapeCode escapeCodes[] =
	{
		{L' ', "nbsp"},
		{L'<', "lt"},
		{L'>', "gt"},
		{L'&', "amp"},
		{L'"', "quot"},
		{0x20AC, "euro"},
		{161, "iexcl"},
		{162, "cent"},
		{163, "pound"},
		{164, "curren"},
		{165, "yen"},
		{166, "brvbar"},
		{167, "sect"},
		{168, "uml"},
		{169, "copy"},
		{170, "ordf"},
		{172, "not"},
		{173, "shy"},
		{174, "reg"},
		{175, "macr"},
		{176, "deg"},
		{177, "plusmn"},
		{178, "sup2"},
		{179, "sup3"},
		{180, "acute"},
		{181, "micro"},
		{182, "para"},
		{183, "middot"},
		{184, "cedil"},
		{185, "sup1"},
		{186, "ordm"},
		{187, "raquo"},
		{188, "frac14"},
		{189, "frac12"},
		{190, "frac34"},
		{191, "iquest"},
		{192, "Agrave"},
		{193, "Aacute"},
		{195, "Atilde"},
		{196, "Auml"},
		{197, "Aring"},
		{198, "AElig"},
		{199, "Ccedil"},
		{200, "Egrave"},
		{201, "Eacute"},
		{202, "Ecirc"},
		{204, "Igrave"},
		{205, "Iacute"},
		{206, "Icirc"},
		{207, "Iuml"},
		{208, "ETH"},
		{209, "Ntilde"},
		{210, "Ograve"},
		{211, "Oacute"},
		{212, "Ocirc"},
		{213, "Otilde"},
		{214, "Ouml"},
		{215, "times"},
		{216, "Oslash"},
		{217, "Ugrave"},
		{218, "Uacute"},
		{219, "Ucirc"},
		{220, "Uuml"},
		{221, "Yacute"},
		{222, "THORN"},
		{223, "szlig"},
		{224, "agrave"},
		{225, "aacute"},
		{226, "acirc"},
		{227, "atilde"},
		{228, "auml"},
		{229, "aring"},
		{230, "aelig"},
		{231, "ccedil"},
		{232, "egrave"},
		{233, "eacute"},
		{234, "ecirc"},
		{235, "euml"},
		{236, "igrave"},
		{237, "iacute"},
		{238, "icirc"},
		{239, "iuml"},
		{240, "eth"},
		{241, "ntilde"},
		{242, "ograve"},
		{243, "oacute"},
		{244, "ocirc"},
		{245, "otilde"},
		{246, "ouml"},
		{247, "divide"},
		{248, "oslash"},
		{249, "ugrave"},
		{250, "uacute"},
		{251, "ucirc"},
		{252, "uuml"},
		{253, "yacute"},
		{254, "thorn"},
	};

	int src = 0;
	int dst = 0;
	while (out[src])
	{
		if (out[src] == '\t' || out[src] == ' ' || (!eatBRs && (out[src] == '\r' || out[src] == '\n')))
		{
			if ((out[src] == '\r' || out[src] == '\n') /*&& dst && out[dst-1] != '\n'*/) out[dst++] = '\n'; else
			if (dst && out[dst-1] != ' ' && out[dst-1] != '\n') out[dst++] = ' ';
			src++;
			continue;
		}
		else if (out[src] == '<')
		{
			int p = ++src;
			// Not going to write 80 lines to recognize quoted >'s
			// Since XML's specs on escape characters are so bad.
			while (out[p] && out[p-1] != '>') p++;
			if (!wcsnicmp(out+src, L"ul", 2) && ((MyIsWspace(out[2+src]) || out[2+src] == '>')))
				out[dst++] = '\n';
			else if ((!wcsnicmp(out+src, L"/ul", 3) || !wcsnicmp(out+src, L"/li", 3)) && out[3+src] == '>')
				out[dst++] = '\n';
			else if (!eatBRs && (!wcsnicmp(out+src, L"br", 2) && (out[2+src] == '>' || iswspace(out[2+src]) || out[2+src] == '/')))
				out[dst++] = '\n';
			src = p;
			continue;
		}
		else if (out[src] == '&')
		{
			int i = 1;
			int accum = 0;
			int num = 0;
			int hex = 0;
			if (out[src+1] == '#')
			{
				if (out[src+2] == 'x' || out[src+2] == 'X')
				{
					i = 3;
					hex = 1;
				}
				else
				{
					i = 2;
					num = 1;
				}
			}
			while (out[src+i] != ';' && i < 10)
			{
				if (num)
					if (out[src+i] >= '0' && out[src+i] <= '9')
						accum = accum*10 + out[src+i] - '0';
					else
						num = 0;
				else if (hex)
					if (out[src+i] >= '0' && out[src+i] <= '9')
						accum = accum*16 + out[src+i] - '0';
					else if (out[src+i] >= 'a' && out[src+i] <= 'f')
						accum = accum*16 + out[src+i] - 'a' + 10;
					else if (out[src+i] >= 'A' && out[src+i] <= 'F')
						accum = accum*16 + out[src+i] - 'A' + 10;
					else
						hex = 0;
				i++;
			}
			if (i < 10 && i > 1)
			{
				int happy = 0;
				if (accum && (num || hex) && accum <= 0xFFFF)
				{
					out[dst++] = (wchar_t)accum;
					happy = 1;
				}
				else
				{
					for (int j=0; j<sizeof(escapeCodes)/sizeof(escapeCodes[0]); j++)
					{
						const char *v = escapeCodes[j].string;
						int p = 0;
						while (v[p] && (wchar_t)v[p] == out[src+1+p]) p++;
						// second case is for strange responce escaping of SysTrans
						if (!v[p] && (out[src+1+p] == ';' || out[src+1+p] == '\\' && out[src+2+p] == ';'))
						{
							happy = 1;
							out[dst++] = escapeCodes[j].code;
							break;
						}
					}
				}
				if (happy)
				{
					src += i+1;
					continue;
				}
			}
		}
		out[dst++] = out[src++];
	}
	out[dst] = 0;
	return out;
}
