#include "Shared/Shrink.h"

#include "Shared/ReadWriteLock.h"

ReadWriteLock::ReadWriteLock() : reader_count_(0)
{
}

ReadWriteLock::~ReadWriteLock()
{
}

void ReadWriteLock::Get()
{
	AutoLock lock(read_lock_);
	if (reader_count_ == 0)
		write_lock_.Get();
	++reader_count_;
}

void ReadWriteLock::Release()
{
	AutoLock lock(read_lock_);
	--reader_count_;
	if (reader_count_ == 0)
		write_lock_.Release();
}

Lock* ReadWriteLock::GetWriteLock()
{
	return &write_lock_;
}

bool ReadWriteLock::TryGetWriteLock()
{
	return write_lock_.TryGet();
}

void ReadWriteLock::ReleaseWriteLock()
{
	write_lock_.Release();
}

