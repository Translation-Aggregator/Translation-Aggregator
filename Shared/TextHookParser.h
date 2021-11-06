#define TEXT_WCHAR     0x0000
#define TEXT_WCHAR_PTR 0x0002
#define TEXT_CHAR      0x0001
#define TEXT_CHAR_PTR  0x0003

// Flags
#define TEXT_PTR  0x0002
// Only used on creation, cleared afterwards.
#define TEXT_FLIP 0x0004

struct CompiledExpression;

struct TextHookInfo
{
	void* address;
	unsigned short type;
	unsigned char flip;
	unsigned char defaultFilter;
	CompiledExpression *value, **contexts, *length, *codePage;
	wchar_t *alias, *hookText;
};

void CleanupTextHookInfo(TextHookInfo *info);

TextHookInfo *CreateTextHookInfo(wchar_t *hookText, wchar_t *alias, void* address, unsigned int type, wchar_t *value, wchar_t *context, wchar_t *length, wchar_t *codePage, int defaultFilter = 1);
TextHookInfo *ParseTextHookString(wchar_t *hook, int testOnly);
