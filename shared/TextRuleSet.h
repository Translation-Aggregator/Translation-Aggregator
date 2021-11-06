#pragma once

enum OpCode
{
	OP_END_SENTENCE,
	OP_START_SENTENCE,
	OP_OUTPUT,
	OP_OUTPUT_STRING,
};

struct Op
{
	OpCode op;
	wchar_t *data;
};

struct TextRule
{
	wchar_t *match;
	int matchLen;
	Op *ops;
	int numOps;
};

class TextRuleSet
{
protected:

public:

	inline TextRuleSet() {}
	// Use this instead of constructor so can fail.

	unsigned int flags;

	// Each individual rule is stored as a single memory block containing
	// string to match and all instructions.  Save a bit of memory that way.
	TextRule **rules;
	int numRules;

	wchar_t *ParseText(wchar_t *text, int *len, int *count);

	~TextRuleSet();
};

TextRuleSet *LoadRuleSet(wchar_t *path, wchar_t *fileName, unsigned int flags);

