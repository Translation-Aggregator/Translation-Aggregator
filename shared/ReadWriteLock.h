#pragma once

#include "Shared/Lock.h"

// Lock that allows a single writer or multiple readers.
class ReadWriteLock : public GenericLock
{
public:
	ReadWriteLock();
	~ReadWriteLock();

	// Get/reload read lock.
	virtual void Get();
	virtual void Release();

	// The write lock acts as a normal lock.
	Lock* GetWriteLock();

	// May fail to get a lock.
	bool TryGetWriteLock();
	// Must only be used after successful call to MaybeGetWriteLock.
	void ReleaseWriteLock();

private:
	// Temporarily obtained before modifying |reader_count_|.
	// Not actually held while reading.
	Lock read_lock_;

	// Main lock is held while writing or reading.  While reading, no
	// specific individual reader actually holds it.  Must be held
	// before incrementing reader_count_ to 1.
	Lock write_lock_;

	int reader_count_;
};
