#pragma once

class GenericLock
{
public:
	inline virtual ~GenericLock() {}

	virtual void Get() = 0;
	virtual void Release() = 0;
};

class Lock : public GenericLock
{
public:
	Lock();
	virtual ~Lock();

	virtual void Get();
	virtual void Release();
	bool TryGet();

private:
	CRITICAL_SECTION critical_section_;
};

class AutoLock
{
public:
	AutoLock(GenericLock& lock);
	~AutoLock();

	GenericLock* lock_;
};
