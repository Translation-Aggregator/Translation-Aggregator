#pragma once

#include <Shared/StringUtil.h>
#include "Config.h"

struct ContextRecord
{
	// May add more later, like translation logs.
	// or may handle that elsewhere.
	SharedString *string;
	void Cleanup();
};

struct ContextProcessInfo
{
	wchar_t *name;
	unsigned int pid;

	// Used mostly for knowing when to cleanup.
	// Keep separate mostly for debugging.  May keep pointer
	// to contexts, too, later.
	int numContexts;
	int numSockets;

	unsigned int internalHookTime;
	unsigned int maxLogLen;

	time_t connectTime;

	// Unlike other objects on this page, these each use their own independently allocated memory,
	// so Cleanup calls free as well.
	void Cleanup();
	void ReleaseContext();
	void AddContext();
	void ReleaseSocket();
	void AddSocket();
};

struct Context
{
	ContextRecord *records;
	int numRecords;

	wchar_t *current;
	int currentLen;
	unsigned int lastTime;
	unsigned int logLen;

	ContextProcessInfo *process;
	wchar_t *name;

	wchar_t *loop;
	int loopLen;
	// Next time to check for a never ending loop.
	// Basically incremented by the idle time.
	unsigned int nextLoopTime;
	// Length when nextLoopTime was set.
	// For it to be loop, everything beyond must be
	// a repeat.
	unsigned int lastCheckLength;

	int characters;
	int filteredCharacters;

	ContextSettings settings;

	void Clear(int noRedraw=0);
	void Cleanup();
	void FlushCurrent();
};

class ContextList
{
public:
	// Need pointers to these, as reshuffle when remove them.
	ContextProcessInfo ** processes;
	int numProcesses;

	// Currently sorted by process, but may change later.
	Context **contexts;
	int numContexts;

	int needTextRefresh;

	ContextList();
	~ContextList();

	// Fails it settings is null and no process exists.
	ContextProcessInfo *FindProcess(wchar_t *name, unsigned int pid, InjectionSettings *settings);

	// Creates process if does not exist.
	void RemoveProcess(ContextProcessInfo *process);
	void AddText(ContextProcessInfo *process, wchar_t *contextName, wchar_t *string);

	// This will also load a process's injection settings, if it doesn't exist.
	void AddText(wchar_t *exeNamePlusFolder, int pid, wchar_t *contextName, wchar_t *string);

	int CheckDone();

	// returns 1 if conext is done, 0 otherwise.
	int CheckContext(Context *context, unsigned int time);

	void ClearProcess(ContextProcessInfo *process);

	void Configure(HWND hWnd);
};

extern ContextList Contexts;
