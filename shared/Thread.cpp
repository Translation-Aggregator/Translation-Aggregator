#include <Shared/Shrink.h>

#include "Shared/scoped_ptr.h"
#include "Shared/Thread.h"

namespace
{

class QuitTask : public Thread::Task
{
public:
	virtual bool ShouldQuit() override { return true; }
};

// MS code to name a thread.
// const DWORD MS_VC_EXCEPTION = 0x406D1388;

// #pragma pack(push,8)
// typedef struct tagTHREADNAME_INFO
// {
// 	DWORD dwType; // Must be 0x1000.
// 	LPCSTR szName; // Pointer to name (in user addr space).
// 	DWORD dwThreadID; // Thread ID (-1=caller thread).
// 	DWORD dwFlags; // Reserved for future use, must be zero.
// } THREADNAME_INFO;
// #pragma pack(pop)

//TODO: fix for MinGW
// void SetThreadName(DWORD dwThreadID, const char* threadName)
// {
// 	THREADNAME_INFO info;
// 	info.dwType = 0x1000;
// 	info.szName = threadName;
// 	info.dwThreadID = dwThreadID;
// 	info.dwFlags = 0;

// 	try
// 	{
// 		RaiseException( MS_VC_EXCEPTION, 0, sizeof(info)/sizeof(ULONG_PTR), (ULONG_PTR*)&info );
// 	}
// 	catch(EXCEPTION_EXECUTE_HANDLER)
// 	{
// 	}
// 	HRESULT r;
//     r = SetThreadDescription(
//         GetCurrentThread(),
//         threadName
//     );
// }

}  // namespace

Thread::Thread(const char* name)
{
	event_ = CreateEvent(NULL, FALSE, FALSE, NULL);
	DWORD id;
	thread_handle_ = CreateThread(NULL, 0, Thread::ThreadStartProc, this, 0, &id);
	// SetThreadName(id, name);
}

Thread::~Thread()
{
	PostTask(new QuitTask());
	WaitForSingleObject(thread_handle_, INFINITE);
	CloseHandle(thread_handle_);
	CloseHandle(event_);
}

void Thread::PostTask(Task* task)
{
	AutoLock lock(lock_);
	tasks_.push_back(task);
	SetEvent(event_);
}

//static
DWORD Thread::ThreadStartProc(void* param)
{
	((Thread*)param)->RunThread();
	return 0;
}

void Thread::RunThread()
{
	while (1)
	{
		DWORD result = WaitForSingleObject(event_, INFINITE);
		// Shouldn't happen.
		if (result != WAIT_OBJECT_0)
			break;
		scoped_ptr<Task> task;
		{
			AutoLock lock(lock_);
			if (tasks_.empty())
				continue;
			task.reset(tasks_.front());
			tasks_.pop_front();
		}
		task->Run();
		if (task->ShouldQuit())
			break;
	}

	// The lock shouldn't be needed.
	AutoLock lock(lock_);
	while (!tasks_.empty())
	{
		delete tasks_.front();
		tasks_.pop_front();
	}
}
