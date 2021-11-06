#include <Shared/Shrink.h>

#include "Shared/Lock.h"

Lock::Lock()
{
	InitializeCriticalSection(&critical_section_);
}

Lock::~Lock()
{
	DeleteCriticalSection(&critical_section_);
}

void Lock::Get()
{
	EnterCriticalSection(&critical_section_);
}

void Lock::Release()
{
	LeaveCriticalSection(&critical_section_);
}

bool Lock::TryGet()
{
	return 0 != TryEnterCriticalSection(&critical_section_);
}

AutoLock::AutoLock(GenericLock& lock) : lock_(&lock)
{
	lock_->Get();
}

AutoLock::~AutoLock()
{
	lock_->Release();
}
