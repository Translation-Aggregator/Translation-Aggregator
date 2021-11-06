#include <Shared/Shrink.h>
#include "HookEval.h"
/*
ULONG EvalSub(Registers *regs, wchar_t *&s, int *error, int endChar);

// Handles immediates/registers and operators that apply to one argument.
// Note that bracketed expressions as a whole are included in this, though
// the body between the brackets are not included.
ULONG EvalSingle(Registers *regs, wchar_t *&s, int *error)
{
	ULONG temp;
	wchar_t *end;
	wchar_t code;
	switch(s[0])
	{
		case '!':
		case '~':
			code = s[0];
			s++;
			temp = EvalSingle(regs, s, error);
			if (*error) return -1;
			if (code == '!')
				return !temp;
			if (code == '~')
				return ~temp;
			// ????
			break;
		case '_':
			s++;
			temp = EvalSingle(regs, s, error);
			if (*error) return -1;
			if (IsBadReadPtr((ULONG*)(regs->esp + temp), 4))
			{
				*error = 1;
				return -1;
			}
			return *(ULONG*)(regs->esp + temp);
		case '-':
			if (s[1] < '0' || s[1] > '9')
			{
				s++;
				temp = EvalSingle(regs, s, error);
				if (!*error) temp = -(LONG)temp;
				return temp;
			}
			temp = wcstol(s, &end, 0);
			if (end == s)
			{
				*error = 1;
				return -1;
			}
			s = end;
			return temp;
		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
			temp = wcstoul(s, &end, 0);
			if (end == s)
			{
				*error = 1;
				return -1;
			}
			s = end;
			return temp;
		case 'E':
			s++;
			s+=2;
			{
				int reg = *(int*)(s-2);
				switch (reg)
				{
					case 'X\0A':
						return regs->eax;
					case 'X\0B':
						return regs->ebx;
					case 'X\0C':
						return regs->ecx;
					case 'X\0D':
						return regs->edx;
					case 'P\0S':
						return regs->esp;
					case 'P\0B':
						return regs->ebp;
					case 'I\0S':
						return regs->esi;
					case 'I\0D':
						return regs->edi;
				}
			}
			*error = 1;
			return -1;
		case '(':
			s++;
			return EvalSub(regs, s, error, ')');
		case '[':
			s++;
			return EvalSub(regs, s, error, ']');
		default:
			break;
	};
	*error = 0;
	return -1;
}

// Handles operators that apply to two arguments.
ULONG EvalSub(Registers *regs, wchar_t *&s, int *error, int endChar)
{
	ULONG arg1, arg2, argAdd;
	wchar_t op = '+', op2;
	argAdd = EvalSingle(regs, s, error);
	if (*error) return -1;
	arg1 = 0;

	while (s[0] != endChar)
	{
		if (!s[0])
		{
			*error = 1;
			return -1;
		}
		switch (s[0])
		{
			case '+':
			case '-':
				if (op == '+')
					arg1 += argAdd;
				else
					arg1 -= argAdd;
				op = s[0];
				s++;
				argAdd = EvalSingle(regs, s, error);
				if (*error) return -1;
				break;
			case '*':
			case '/':
			case '%':
			case '^':
			case '|':
			case '&':
				op2 = s[0];
				s++;
				arg2 = EvalSingle(regs, s, error);
				if (*error) return -1;
				if (op2 == '*')
					argAdd *= arg2;
				else if (op2 == '/')
					argAdd /= arg2;
				else if (op2 == '%')
					argAdd %= arg2;
				else if (op2 == '^')
					argAdd ^= arg2;
				else if (op2 == '|')
					argAdd |= arg2;
				else if (op2 == '&')
					argAdd &= arg2;
				break;
			case '>':
			case '<':
				if (s[1] != s[0])
				{
					*error = 1;
					return -1;
				}
				op2 = s[0];
				s+=2;
				arg2 = EvalSingle(regs, s, error);
				if (*error) return -1;
				if (op2 == '>')
					argAdd >>= arg2;
				else
					argAdd <<= arg2;
				break;
			default:
				*error = 0;
				return -1;
		}
	};
	s++;

	if (op == '+')
		arg1 += argAdd;
	else
		arg1 -= argAdd;
	if (endChar == ']')
	{
		if (IsBadReadPtr((ULONG*)arg1, 4))
		{
			*error = 1;
			return -1;
		}
		return *(ULONG*)arg1;
	}
	return arg1;
}

ULONG HookEval(Registers *regs, wchar_t *s, int *error)
{
	int e = 0;
	wchar_t *str = s;
	ULONG out = EvalSub(regs, str, &e, 0);
	if (e) *error = e;
	return out;
}




ULONG EvalSyntaxCheckSub(wchar_t *&s, int endChar);
// Handles immediates/registers and operators that apply to one argument.
// Note that bracketed expressions as a whole are included in this, though
// the body between the brackets are not included.
ULONG EvalSyntaxCheckSingle(wchar_t *&s)
{
	ULONG temp;
	wchar_t *end;
	switch(s[0])
	{
		case '_':
		case '!':
		case '~':
			s++;
			return EvalSyntaxCheckSingle(s);
		case '-':
			if (s[1] < '0' || s[1] > '9')
			{
				s++;
				return EvalSyntaxCheckSingle(s);
			}
			temp = wcstol(s, &end, 0);
			if (end == s)
				return 0;
			s = end;
			return 1;
		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
			temp = wcstoul(s, &end, 0);
			if (end == s)
				return 0;
			s = end;
			return 1;
		case 'E':
			s++;
			s+=2;
			{
				int reg = *(int*)(s-2);
				switch (reg)
				{
					case 'X\0A':
					case 'X\0B':
					case 'X\0C':
					case 'X\0D':
					case 'P\0S':
					case 'P\0B':
					case 'I\0S':
					case 'I\0D':
						return 1;
					default:
						return 0;
				}
			}
		case '(':
			s++;
			return EvalSyntaxCheckSub(s, ')');
		case '[':
			s++;
			return EvalSyntaxCheckSub(s, ']');
		default:
			return 0;
	};
}

// Handles operators that apply to two arguments.
ULONG EvalSyntaxCheckSub(wchar_t *&s, int endChar)
{
	if (!EvalSyntaxCheckSingle(s))
		return 0;

	while (s[0] != endChar)
	{
		if (!s[0])
			return 0;
		switch (s[0])
		{
			case '+':
			case '-':
				s++;
				if (!EvalSyntaxCheckSingle(s))
					return 0;
				break;
			case '*':
			case '/':
			case '%':
			case '^':
			case '|':
			case '&':
				s++;
				if (!EvalSyntaxCheckSingle(s))
					return 0;
				break;
			case '>':
			case '<':
				if (s[1] != s[0])
					return 0;
				if (!EvalSyntaxCheckSingle(s))
					return 0;
				break;
			default:
				return 0;
		}
	};
	s++;
	return 1;
}
//*/
struct OperatorInfo
{
	wchar_t *string;
	Operator op;
	Priority priority;
};

OperatorInfo prefixOps[] =
{
	{L"!", OP_NEGATE, PRIORITY_PREFIX},
	{L"~", OP_BITWISE_NEGATE, PRIORITY_PREFIX},
	{L"_", OP_ESP_DEREF, PRIORITY_PREFIX},
};

OperatorInfo binaryOps[] =
{
	{L"*", OP_MUL, PRIORITY_MUL},
	{L"/", OP_DIV, PRIORITY_MUL},
	{L"%", OP_MOD, PRIORITY_MUL},

	{L"^", OP_BITWISE_XOR, PRIORITY_MUL},
	{L"&", OP_BITWISE_AND, PRIORITY_MUL},
	{L"|", OP_BITWISE_OR,  PRIORITY_MUL},

	{L">>", OP_RSHIFT, PRIORITY_MUL},
	{L"<<", OP_LSHIFT, PRIORITY_MUL},

	{L"+", OP_ADD, PRIORITY_ADD},
	{L"-", OP_SUB, PRIORITY_ADD},
};

int HookEvalSyntaxCheck(wchar_t *s)
{
	if (!s) return 0;

	CompiledExpression *exp = CompileExpression(s);
	if (!exp)
		return 0;
	free(exp);
	return 1;

	//return EvalSyntaxCheckSub(s, 0);
}

void PushOp(Operation* &stack, Operator op, Priority priority)
{
	stack--;
	stack->op = op;
	stack->priority = priority;
	// Value is always 0 on stack, no need to set.
}

void AddOp(CompiledExpression *exp, const Operation& op)
{
	exp->ops[exp->numOps++] = op;
}

void AddOp(CompiledExpression *exp, Operator op, Priority priority, unsigned int value)
{
	Operation operation;
	operation.op = op;
	operation.priority = priority;
	operation.value = value;
	AddOp(exp, operation);
}

void PopOps(CompiledExpression *exp, Operation *&stack, Operation *&stackEnd, Priority maxPriority, int &depth)
{
	while (stack != stackEnd && stack->priority <= maxPriority)
	{
		AddOp(exp, *stack);
		if (stack->priority <= PRIORITY_MUL)
			depth--;
		stack++;
	}
}

CompiledExpression *CompileExpression(wchar_t *s)
{
	if (!s) return 0;
	int nextIsOperand = 1;
	int len = wcslen(s);
	// 1 large enough than really needed, but guarantees Operation is a constant in AddOp.
	int memSize = len * sizeof(Operation) + sizeof(CompiledExpression);
	CompiledExpression *exp = (CompiledExpression*) calloc(memSize, 1);
	Operation *stackEnd = (Operation*)(((char*)exp) + memSize);
	Operation *stack = stackEnd;
	int depth = 0;
	while (*s)
	{
		if (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n')
		{
			s++;
			continue;
		}
		if (nextIsOperand)
		{
			// Handle unary operators.
			int i=0;
			while (i<sizeof(prefixOps)/sizeof(prefixOps[0]))
			{
				if (!wcsncmp(s, prefixOps[i].string, wcslen(prefixOps[i].string)))
					break;
				i++;
			}
			if (i < sizeof(prefixOps)/sizeof(prefixOps[0]))
			{
				PushOp(stack, prefixOps[i].op, prefixOps[i].priority);
				s+=wcslen(prefixOps[i].string);
				continue;
			}

			// Brackets.
			if (s[0] == '(' || s[0] == '[')
			{
				Operator op = OP_PARENS;
				if (s[0] == '[') op = OP_BRACKETS;
				PushOp(stack, op, PRIORITY_BRACKET);
				s++;
				continue;
			}

			// Integers
			unsigned int value;
			wchar_t *end;
			value = wcstoul(s, &end, 0);
			if (end != s)
			{
				AddOp(exp, OP_PUSH_INT, PRIORITY_NONE, value);
				depth++;
				if (depth > exp->maxStack)
					exp->maxStack = depth;
				s = end;
				nextIsOperand = 0;
				continue;
			}

			// Registers:
			int regOffset = -1;
			// Not actually used - just want the offsets.
#define REGISTERS ((Registers*)0)
			if (!wcsnicmp(s, L"esp", 3))
				regOffset = &REGISTERS->esp - (int*)REGISTERS;
			else if (!wcsnicmp(s, L"ebp", 3))
				regOffset = &REGISTERS->ebp - (int*)REGISTERS;
			else if (!wcsnicmp(s, L"eax", 3))
				regOffset = &REGISTERS->eax - (int*)REGISTERS;
			else if (!wcsnicmp(s, L"ebx", 3))
				regOffset = &REGISTERS->ebx - (int*)REGISTERS;
			else if (!wcsnicmp(s, L"ecx", 3))
				regOffset = &REGISTERS->ecx - (int*)REGISTERS;
			else if (!wcsnicmp(s, L"edx", 3))
				regOffset = &REGISTERS->edx - (int*)REGISTERS;
			else if (!wcsnicmp(s, L"esi", 3))
				regOffset = &REGISTERS->esi - (int*)REGISTERS;
			else if (!wcsnicmp(s, L"edi", 3))
				regOffset = &REGISTERS->edi - (int*)REGISTERS;
			if (regOffset >= 0)
			{
				AddOp(exp, OP_PUSH_REG, PRIORITY_NONE, regOffset);
				s += 3;
				depth++;
				if (depth > exp->maxStack)
					exp->maxStack = depth;
				nextIsOperand = 0;
				continue;
			}
			// Fail.
			break;
		}
		else
		{
			// Brackets.
			if (s[0] == ')' || s[0] == ']')
			{
				PopOps(exp, stack, stackEnd, (Priority)(PRIORITY_BRACKET-1), depth);
				if (stack == stackEnd ||
					(s[0] == ')' && stack->op != OP_PARENS) ||
					(s[0] == ']' && stack->op != OP_BRACKETS))
						break;
				stack++;
				if (s[0] == ']')
					AddOp(exp, OP_DEREF, PRIORITY_BRACKET, 0);
				s++;
				continue;
			}
			// Handle binary operators.
			int i=0;
			while (i<sizeof(binaryOps)/sizeof(binaryOps[0]))
			{
				if (!wcsncmp(s, binaryOps[i].string, wcslen(binaryOps[i].string)))
					break;
				i++;
			}
			if (i<sizeof(binaryOps)/sizeof(binaryOps[0]))
			{
				PopOps(exp, stack, stackEnd, binaryOps[i].priority, depth);
				s += wcslen(binaryOps[i].string);
				PushOp(stack, binaryOps[i].op, binaryOps[i].priority);
				nextIsOperand = 1;
				continue;
			}
			// Fail;
			break;
		}
	}
	PopOps(exp, stack, stackEnd, (Priority)(PRIORITY_BRACKET-1), depth);

	// Either of the first two should imply the 3rd, but...
	// 100 stack depth should be more than enough.
	if (s[0] || depth != 1 || stack != stackEnd || exp->maxStack >= 100)
	{
		free(exp);
		return 0;
	}

	memSize = (exp->numOps-1) * sizeof(Operation) + sizeof(CompiledExpression);
	exp = (CompiledExpression*) realloc(exp, exp->CalcMemSize());
	return exp;
}

int CompiledExpression::CalcMemSize()
{
	return (numOps-1) * sizeof(Operation) + sizeof(CompiledExpression);
}

unsigned int CompiledExpression::Eval(Registers *regs, int *error)
{
	int stack[100];
	unsigned int stackTop = -1;
	int op = 0;
	while (op < numOps)
	{
		// Most ops do this.  Saves a lot of lines of code,
		// though have to undo it in a lot of common cases...
		stackTop--;
		switch (ops[op].op)
		{
			case OP_ESP_DEREF:
				stack[stackTop+1] += regs->esp;
			case OP_DEREF:
				stackTop++;
				if (IsBadReadPtr((ULONG*)(stack[stackTop]), 4))
				{
					*error = 1;
					return -1;
				}
				stack[stackTop] = *(ULONG*)(stack[stackTop]);
				break;
			case OP_NEGATE:
				stackTop++;
				stack[stackTop] = !stack[stackTop];
				break;
			case OP_BITWISE_NEGATE:
				stackTop++;
				stack[stackTop] = ~stack[stackTop];
				break;
			case OP_PUSH_INT:
				stackTop++;
				stack[++stackTop] = ops[op].value;
				break;
			case OP_PUSH_REG:
				stackTop++;
				stack[++stackTop] = regs->regs[ops[op].value];
				break;

			case OP_MUL:
				stack[stackTop] *= stack[stackTop+1];
				break;
			case OP_DIV:
				stack[stackTop] = (int)stack[stackTop] / (int) stack[stackTop+1];
				break;
			case OP_MOD:
				stack[stackTop] %= stack[stackTop+1];
				break;

			case OP_BITWISE_XOR:
				stack[stackTop] ^= stack[stackTop+1];
				break;
			case OP_BITWISE_AND:
				stack[stackTop] &= stack[stackTop+1];
				break;
			case OP_BITWISE_OR:
				stack[stackTop] |= stack[stackTop+1];
				break;

			case OP_RSHIFT:
				stack[stackTop] >>= stack[stackTop+1];
				break;
			case OP_LSHIFT:
				stack[stackTop] <<= stack[stackTop+1];
				break;

			case OP_ADD:
				stack[stackTop] += stack[stackTop+1];
				break;
			case OP_SUB:
				stack[stackTop] -= stack[stackTop+1];
				break;
			default:
				*error = 1;
				return -1;
		}
		op++;
	}
	return stack[0];
}
