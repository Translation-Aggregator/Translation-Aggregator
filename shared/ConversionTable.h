#define CONVERSION_NULL    0x0
#define CONVERSION_NODE    0x1
#define CONVERSION_SUBTABLE  0x2

struct ConversionTable;

struct CTCharacter
{
	unsigned char type;
	unsigned char length;
	// Keeps entries down to 8 bytes.
	// While some UTF8 characters use more, not used for UTF8.
	unsigned char value[6];
};

struct CTSubTable
{
	unsigned char type;
	ConversionTable *table;
};

struct CTEntry
{
	union
	{
		unsigned char type;
		CTCharacter character;
		CTSubTable subtable;
	};
};

struct ConversionTable
{
private:
	ConversionTable *reverseTable;

	// Internally, uses identical code to/from UTF16.  Note that waste a lot of memory
	// for UTF16, due to endian-ness (Well, about 128k).
	int DecodeCharacterInternal(const void *in, int *readLen, void *out, int *error);
public:
	CTEntry entries[256];

	// On error, writes '?'.
	int DecodeCharacter(const char *in, int *readLen, wchar_t *out, int *error);
	int EncodeCharacter(const wchar_t *in, int *readLen, char *out, int *error);

	// For simplicity, assume long enough.  Also, must be null terminated.
	int DecodeString(const char *in, int inLen, wchar_t *out);
	int EncodeString(const wchar_t *in, int inLen, char *out);

	void Cleanup();

	static ConversionTable *LoadTable(wchar_t *path);
	int AddEntry(unsigned char *s1, int len1, unsigned char *s2, int len2);
};

void CleanupConversionTables();

// If need to add more, may add more complicated management/loading code.
// For now, specific charset functions work fine.
ConversionTable *GetEucJpTable();
