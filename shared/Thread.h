#pragma once

#include "Shared/Lock.h"

// Very simple thread implementation.  Tasks cannot safely be posted
// during destruction.
class Thread
{
public:
	class Task
	{
	public:
		virtual void Run() {};
		virtual bool ShouldQuit() { return false; }
	};

	Thread(const char* name);
	~Thread();

	void PostTask(Task* task);

private:
	static DWORD __stdcall ThreadStartProc(void* param);
	void RunThread();

	std::list<Task*> tasks_;

	Lock lock_;
	HANDLE event_;
	HANDLE thread_handle_;
};
