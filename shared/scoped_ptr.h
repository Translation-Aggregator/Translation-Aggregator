#pragma once

template <class T>
class scoped_ptr
{
public:
	scoped_ptr() : ptr_(NULL) {}
	explicit scoped_ptr(T* ptr) : ptr_(ptr) {}

	~scoped_ptr()
	{
		delete ptr_;
	}

	void reset(T* ptr = NULL)
	{
		delete ptr_;
		ptr_ = ptr;
	}

	T* get() const
	{
		return ptr_;
	}

	T* release()
	{
		T* ptr = ptr_;
		ptr_ = NULL;
		return ptr;
	}

	T& operator*() const
	{
		return *ptr_;
	}

	T* operator->() const
	{
		return ptr_;
	}

private:
	T* ptr_;
};
