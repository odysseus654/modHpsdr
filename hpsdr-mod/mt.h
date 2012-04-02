#pragma once
#include <Windows.h>

class Lock
{
public:
	Lock()			{ InitializeCriticalSection(&m_cs); }
	~Lock()			{ DeleteCriticalSection(&m_cs); }
	void lock()		{ EnterCriticalSection(&m_cs); }
	void unlock()	{ LeaveCriticalSection(&m_cs); }
	bool tryLock()	{ return !!TryEnterCriticalSection(&m_cs); }

private:
	CRITICAL_SECTION m_cs;
};

class Locker
{
public:
	Locker(Lock& l, bool bLocked = true):m_lock(&l),m_bLocked(false)
	{
		if(bLocked) lock();
	}

	~Locker()
	{
		unlock();
	}

	void set(Lock& lock)
	{
		unlock();
		m_lock = &lock;
	}

	void lock()
	{
		if(m_lock && !m_bLocked)
		{
			m_lock->lock();
			m_bLocked = true;
		}
	}

	bool tryLock()
	{
		if(m_lock && (m_bLocked || m_lock->tryLock()))
		{
			m_bLocked = true;
			return true;
		}
		else
		{
			return false;
		}
	}

	void unlock()
	{
		if(m_lock && m_bLocked)
		{
			m_lock->unlock();
			m_bLocked = false;
		}
	}

private:
	Lock* m_lock;
	bool  m_bLocked;
};