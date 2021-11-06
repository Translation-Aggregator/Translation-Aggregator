#include <Shared/Shrink.h>
#include "ConversionTable.h"
#include <Shared/StringUtil.h>

void ConversionTable::Cleanup()
{
	for (int i=0; i<256; i++)
	{
		if (entries[i].type & CONVERSION_SUBTABLE)
			entries[i].subtable.table->Cleanup();
	}
	if (reverseTable)
	{
		reverseTable->reverseTable = 0;
		reverseTable->Cleanup();
	}
	free(this);
}

int ConversionTable::DecodeCharacterInternal(const void *v_in, int *readLen, void *v_out, int *error)
{
	unsigned char *in = (unsigned char*)v_in;
	unsigned char *out = (unsigned char*)v_out;
	int len = 1;
	ConversionTable *table = this;
	while (1)
	{
		CTEntry *entry = table->entries + *in;
		if (entry->type & CONVERSION_NODE)
		{
			memcpy(out, entry->character.value, entry->character.length);
			*readLen = len;
			*error = 0;
			return entry->character.length;
		}
		else if (entry->type & CONVERSION_SUBTABLE)
		{
			len++;
			in++;
			table = entry->subtable.table;
		}
		else
			break;
	}
	// Error length handled at higher level, to distinguish
	// UTF16 and multi-byte.
	*error = 1;
	return 0;
}

int ConversionTable::DecodeCharacter(const char *in, int *readLen, wchar_t *out, int *error)
{
	// Works at byte level, so returns length in bytes.
	int len = DecodeCharacterInternal(in, readLen, out, error);
	if (*error)
	{
		*out = '?';
		*readLen = 1;
		return 1;
	}
	return len/2;
}

int ConversionTable::EncodeCharacter(const wchar_t *in, int *readLen, char *out, int *error)
{
	int len = reverseTable->DecodeCharacterInternal(in, readLen, out, error);
	if (*error)
	{
		*out = '?';
		*readLen = 1;
		return 1;
	}
	*readLen /= 2;
	return len;
}

int ConversionTable::DecodeString(const char *in, int inLen, wchar_t *out)
{
	int written = 0;
	int read = 0;

	int readLen;
	int outLen;
	int error;
	// Currently always returns value >= 1.
	while (outLen = DecodeCharacter(in + read, &readLen, out + written, &error))
	{
		written += outLen;
		read += readLen;
		if (read >= inLen)
			break;
	}
	return written;
}

int ConversionTable::EncodeString(const wchar_t *in, int inLen, char *out)
{
	int written = 0;
	int read = 0;

	int readLen;
	int outLen;
	int error;
	// Currently always returns value >= 1.
	while (outLen = EncodeCharacter(in + read, &readLen, out + written, &error))
	{
		written += outLen;
		read += readLen;
		if (read >= inLen)
			break;
	}
	return written;
}

int ConversionTable::AddEntry(unsigned char *s1, int len1, unsigned char *s2, int len2)
{
	if (len2 > 6) return 0;

	CTEntry *entry = entries+s1[0];
	len1--;
	s1++;
	while (len1 > 0)
	{
		if (entry->type & CONVERSION_NODE)
			return 0;
		else if (!(entry->type & CONVERSION_SUBTABLE))
		{
			entry->type = CONVERSION_SUBTABLE;
			entry->subtable.table = (ConversionTable*) calloc(1, sizeof(ConversionTable));
		}
		entry = entry->subtable.table->entries + s1[0];
		len1--;
		s1++;
	}
	if (entry->type & CONVERSION_SUBTABLE)
		return 0;
	// Allow multiple characters to map to single character in either direction.
	if (entry->type & CONVERSION_NODE)
		return 1;
	entry->type = CONVERSION_NODE;
	entry->character.length = len2;
	memcpy(entry->character.value, s2, len2);
	return 1;
}

ConversionTable *ConversionTable::LoadTable(wchar_t *path)
{
	unsigned char *data;
	int size;
	LoadFile(path, (wchar_t **) &data, &size, 1);
	if (!size)
		return 0;
	ConversionTable *out = (ConversionTable *)calloc(1, sizeof(ConversionTable));
	out->reverseTable = (ConversionTable *)calloc(1, sizeof(ConversionTable));

	unsigned char *pos = data;
	int count = 0;
	while (pos < data+size)
	{
		unsigned char len1 = pos[0] & 0xF;
		unsigned char len2 = pos[0] / 16;
		pos++;
		count ++;
		if (pos + len1 + len2 > data + size)
		{
			out->Cleanup();
			return 0;
		}
		unsigned char *s1 = pos;
		pos += len1;
		unsigned char *s2 = pos;
		pos += len2;
		if (!out->AddEntry(s1, len1, s2, len2) || !out->reverseTable->AddEntry(s2, len2, s1, len1))
		{
			out->Cleanup();
			return 0;
		}
	}

	// Not currently needed, but doesn't hurt.
	out->reverseTable->reverseTable = out;
	return out;
}

static ConversionTable *tables[1];

ConversionTable *GetEucJpTable()
{
	if (!tables[0])
	{
		tables[0] = ConversionTable::LoadTable(L"ConversionTables\\euc-jp.bin");
		if (!tables[0])
			tables[0] = (ConversionTable*)-1;
	}
	if (tables[0] == (ConversionTable*)-1)
		return 0;
	return tables[0];
}

void CleanupConversionTables()
{
	for (int i=0; i<sizeof(tables)/sizeof(tables[0]); i++)
	{
		if (tables[i] && tables[i] != (ConversionTable*)-1)
			tables[i]->Cleanup();
		tables[i] = 0;
	}
}

class LazyCTCleanup
{
public:
	~LazyCTCleanup()
	{
		CleanupConversionTables();
	}
};
