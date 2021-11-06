#include <Shared/Shrink.h>
#include "Filter.h"

int InfiniteRepeatFilter(wchar_t *s, int len)
{
	int out = 1;
	int in = 1;
	while (in < len)
	{
		if (s[in] != s[in-1])
			s[out++] = s[in];
		in++;
	}
	s[out] = 0;
	return out;
}

int ConstantRepeatFilter(wchar_t *s, int len, int repeat)
{
	if (repeat > 1)
	{
		int in = 0, out = 0;
		while (in < len)
		{
			s[out] = s[in];
			out++;
			in += repeat;
		}
		s[out] = 0;
		return out;
	}
	return len;
}

int GCD(int a, int b)
{
	while (b)
	{
		int b2 = a - b * (a / b);
		a = b;
		b = b2;
	}
	return a;
}

int AutoConstantRepeatFilter(wchar_t *s, int len)
{
	int count = 1;
	int repeat = 0;
	for (int i=1; i<len; i++)
	{
		if (s[i] == s[i-1])
			count++;
		else
		{
			repeat = GCD(count, repeat);
			count = 1;
		}
	}
	repeat = GCD(count, repeat);
	return ConstantRepeatFilter(s, len, repeat);
}

int AutoAdvancedRepeatFilter(wchar_t *s, int len)
{
	if (len <= 6)
		return AutoConstantRepeatFilter(s, len);
	int out = 0;
	int count = 0;
	for (int i=0; i<len; i++)
	{
		int best = 0;
		int bestScore = 2;
		int bestLen = 0;
		for (int rep = 2; rep < len && s[i] == s[i+rep-1]; rep++)
		{
			int score = 0;
			int j;
			for (j = 1; i+rep*(j+1) <= len; j++)
			{
				int match = 1;
				while (match < rep)
				{
					if (s[i+j*rep] != s[i+j*rep+match])
						break;
					match++;
				}
				if (match != rep)
					break;
				if (s[i+j*rep] != s[i+j*rep-1])
					score ++;
			}
			if (score > bestScore)
			{
				if (s[i+j*rep] == s[i+j*rep-1])
				{
					while (s[i+j*rep] == s[i+j*rep-1])
						j--;
				}
				bestScore = score;
				best = rep;
				bestLen = j;
			}
		}
		if (best)
		{
			for (int k=0; k < bestLen; k++)
				s[out++] = s[i+k*best];
			i += bestLen * best-1;
			continue;
		}
		s[out] = s[i];
		out++;
	}
	s[out] = 0;
	return out;
}

int CustomRepeatFilter(wchar_t *s, int len, unsigned int *repeats, int numRepeats)
{
	if (numRepeats <= 0)
		return InfiniteRepeatFilter(s, len);
	if (numRepeats == 1)
		return ConstantRepeatFilter(s, len, repeats[0]);
	int out = 0;
	int count = 0;
	for (int i=0; i<len; i++)
	{
		int bestLen = 0;
		int bestRep = 0;
		for (int r = 0; r<numRepeats; r++)
		{
			int rep = repeats[r];
			int j;
			for (j=0; i + j * rep <= len; j++)
			{
				int k;
				for (k=1; k<rep; k++)
				{
					if (s[i+j*rep+k] != s[i+j*rep+k-1])
						break;
				}
				if (k < rep)
					break;
			}
			if (j > 0)
			{
				int clen = j * rep;
				if (clen > bestLen || (clen == bestLen && rep > bestRep))
				{
					bestRep = rep;
					bestLen = rep * j;
					if (j > 1)
					{
						int end = bestLen + i;
						wchar_t c = s[end-1];
						if ((end < len && s[end] == c) || c == s[end-1-rep])
						{
							end -= rep;
							while (end > i && c == s[end-1])
								end -= rep;
							if (end != i)
								bestLen = end - i;
						}
					}
				}
			}
		}
		if (bestRep)
		{
			int j = 0;
			while (j < bestLen)
			{
				s[out++] = s[i];
				i += bestRep;
				j += bestRep;
			}
			i--;
			continue;
		}
		s[out] = s[i];
		out++;
	}
	s[out] = 0;
	return out;
}

int RepeatExtensionFilter(wchar_t *s, int len)
{
	int out = 0;
	for (int i=0; i<len; i++)
	{
		if (i < len - 4)
		{
			int lastLen = 1;
			int lastStart = i;
			while (1)
			{
				int currentStart = lastStart + lastLen;
				if (currentStart + lastLen + 1 > len)
					break;
				if (memcmp(s+lastStart, s+currentStart, lastLen * sizeof(wchar_t)))
					break;
				lastStart = currentStart;
				lastLen++;
			}
			if (lastLen >= 3)
			{
				memcpy(s+out, s+lastStart, sizeof(wchar_t) * lastLen);
				out += lastLen;
				i = lastStart + lastLen-1;
				continue;
			}
		}
		s[out] = s[i];
		out++;
	}
	s[out] = 0;
	return out;
}

int AggressiveExtensionFilter(wchar_t *s, int len)
{
	for (int i=len-1; i>=0; i--)
	{
		int max = len-i-1;
		if (max > i) max = i;
		for (int w = max; w >= 5; w--)
		{
			if (!wcsncmp(s + i, s+i-w, w))
			{
				int charCount = 1;
				for (int q = 1; q<w; q++)
				{
					if (s[q+i] != s[q+i-1])
						charCount ++;
				}
				if (charCount >= 3 || (charCount >= 2 && w >= 4))
				{
					int start = i-w;
					while (w > 0)
					{
						if (start >= w && !wcsncmp(s + start, s + start-w, w))
						{
							start -= w;
							continue;
						}
						w--;
					}
					wcscpy(s + start, s + i);
					len -= i - start;
					i = start;
				}
			}
		}
	}
	return len;
}


int PhraseRepeatFilter(wchar_t *s, int len, const int minDist, const int maxDist)
{
	int out = 0;
	int tempMinDist = minDist;
	int lastDist = 0;
	for (int i=0; i<len; i++)
	{
		int hitDist = 0;
		int hitCount = 0;
		// Older code.  Doesn't work for XXX very well.
		/*
		for (int d=minDist; d < maxDist && d <= out; d++)
		{
			int count = 0;
			int pos = i;
			while (!memcmp(s+out-d, s+pos, d * sizeof(wchar_t)))
			{
				pos += d;
				count ++;
				if (pos + d > len)
					break;
			}
			if (count && count*d >= hitDist*hitCount)
			{
				hitCount = count;
				hitDist = d;
			}
		}
		if (hitCount)
		{
			i += hitCount * hitDist-1;
			continue;
		}
		//*/
		for (int d=tempMinDist; d < maxDist && i+2*d <= len; d++)
		{
			int count = 1;
			int pos = i + d;
			while (!memcmp(s+i, s+pos, d * sizeof(wchar_t)))
			{
				pos += d;
				count ++;
				if (pos + d > len)
					break;
			}
			if (count > 1)
			{
				if (hitDist * hitCount < d * count)
				{
					int chars = 1;
					if (!hitCount)
					{
						//for (int j=1; j<d; j++) {
						//  chars += (s[i+j] != s[i+j-1]);
						//}
						// If want to require 3 distinct characters.
						//if (chars >= 3) {
						hitDist = d;
						hitCount = count;
						//}
					}
					else if (hitDist * hitCount > d * count)
					{
						hitDist = d;
						hitCount = count;
					}
				}
			}
		}
		if (hitDist)
		{
			lastDist = hitDist;
			tempMinDist = hitDist;
			i += hitDist * (hitCount-1) - 1;
			continue;
		}
		tempMinDist = minDist;

		if (lastDist)
		{
			while (lastDist > 1)
			{
				s[out++] = s[i++];
				lastDist--;
			}
		}
		s[out] = s[i];
		out++;
	}
	s[out] = 0;
	return out;
}

int LineBreakRemoveAll(wchar_t *s, int len)
{
	int out = 0;
	for (int i=0; i<len; i++)
	{
		if (s[i] == '\n')
			continue;
		s[out] = s[i];
		out++;
	}
	s[out] = 0;
	return out;
}

int LineBreakRemoveSome(wchar_t *s, int len, unsigned int first, unsigned int last)
{
	int out = 0;
	unsigned int found = 0;
	while (found < first && out < len)
	{
		if (s[out] == '\n')
			found ++;
		out ++;
	}
	if (out == len) return len;
	found = 0;
	int finalBr = len;

	while (found < last && finalBr > out)
	{
		finalBr--;
		if (s[finalBr] == '\n')
			found++;
	}
	if (finalBr <= out)
		return len;
	int i = out;
	for (;i < finalBr; i++)
	{
		if (s[i] == '\n')
			continue;
		s[out] = s[i];
		out++;
	}
	wcscpy(s+out, s+finalBr);
	return out + len - finalBr;
}

int AutoFilter(wchar_t *s, int len, const ContextSettings *settings)
{
	//FILE *out = fopen("out.txt", "ab");
	//fwrite(s, 2, len, out);
	//fwrite(L"\r\n\r\n", 2, 4, out);
	switch (settings->charRepeatMode)
	{
		case CHAR_REPEAT_AUTO_CONSTANT:
			len = AutoConstantRepeatFilter(s, len);
			break;
		case CHAR_REPEAT_INFINITE:
			len = InfiniteRepeatFilter(s, len);
			break;
		case CHAR_REPEAT_AUTO_ADVANCED:
			len = AutoAdvancedRepeatFilter(s, len);
			break;
		case CHAR_REPEAT_CUSTOM:
			{
				int numRepeats = 0;
				unsigned int repeats[sizeof(settings->customRepeats)/sizeof(settings->customRepeats[0])];
				while (numRepeats < sizeof(repeats)/sizeof(repeats[0]))
				{
					if (!settings->customRepeats[numRepeats])
						break;
					numRepeats++;
				}
				len = CustomRepeatFilter(s, len, repeats, numRepeats);
				if (!settings->customRepeats[1])
					len = InfiniteRepeatFilter(s, len);
				break;
			}
		case CHAR_REPEAT_NONE:
		default:
			break;
	}

	switch (settings->phaseExtensionMode)
	{
		case PHRASE_EXTENSION_BASIC:
			len = RepeatExtensionFilter(s, len);
			break;
		case PHRASE_EXTENSION_AGGRESSIVE:
			len = AggressiveExtensionFilter(s, len);
			break;
		case PHRASE_EXTENSION_NONE:
		default:
			break;
	}

	switch (settings->phaseRepeatMode)
	{
		case PHRASE_REPEAT_CUSTOM:
			if (settings->phraseMin > 0 && settings->phraseMax > settings->phraseMin)
				len = PhraseRepeatFilter(s, len, settings->phraseMin, settings->phraseMax);
			break;
		case PHRASE_REPEAT_AUTO:
			len = PhraseRepeatFilter(s, len);
			break;
		case PHRASE_REPEAT_NONE:
		default:
			break;
	}

	// Note:  Use len everywhere else here, but always null terminated as well.
	// Need to remove '\r' before line break stuff.
	SpiffyUp(s);
	len = wcslen(s);

	switch (settings->lineBreakMode)
	{
		case LINE_BREAK_REMOVE:
			len = LineBreakRemoveAll(s, len);
			break;
		case LINE_BREAK_REMOVE_SOME:
			len = LineBreakRemoveSome(s, len, settings->lineBreaksFirst, settings->lineBreaksLast);
			break;
		case LINE_BREAK_KEEP:
		default:
			break;
	}
	//fwrite(s, 2, len, out);
	//fwrite(L"\r\n\r\n\r\n\r\n", 2, 4, out);
	//fclose(out);

	return len;
}
