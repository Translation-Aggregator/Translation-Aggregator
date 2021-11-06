#pragma once

struct Registers
{
	union
	{
		struct
		{
			int edi, esi, ebp, esp, ebx, edx, ecx, eax;
		};
		int regs[8];
	};
};


enum Operator
{
	// Brackets - not actually operators, don't appear in compiled stack.
	OP_PARENS,
	OP_BRACKETS,

	// Unary Operations - modify top stack value.
	OP_DEREF, // Used with [].
	OP_ESP_DEREF, // _ operator.
	OP_NEGATE,
	OP_BITWISE_NEGATE,

	// Add value to stack.
	OP_PUSH_INT,
	OP_PUSH_REG,

	// Replace top two stack values with a single value.

	OP_MUL,
	OP_DIV,
	OP_MOD,

	OP_BITWISE_XOR,
	OP_BITWISE_AND,
	OP_BITWISE_OR,

	OP_RSHIFT,
	OP_LSHIFT,

	OP_ADD,
	OP_SUB,
};

enum Priority
{
	// Priority only matters for things that are put on the stack before being added to
	// the epression.
	PRIORITY_NONE = -1,
	PRIORITY_ADD = 0,
	PRIORITY_MUL = 1,
	PRIORITY_PREFIX = 2,
	PRIORITY_BRACKET = 3,
};

struct Operation
{
	Operator op:16;
	Priority priority:16;
	// only used for registers and immediates.
	unsigned int value;
};

struct CompiledExpression
{
	int maxStack;
	int numOps;
	Operation ops[1];
	unsigned int Eval(Registers *regs, int *error);
	int CalcMemSize();
};

//ULONG HookEval(Registers *regs, wchar_t *s, int *error);
int HookEvalSyntaxCheck(wchar_t *s);

CompiledExpression *CompileExpression(wchar_t *s);
