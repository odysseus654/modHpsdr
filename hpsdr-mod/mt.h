#pragma once

class Condition;

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

friend class Condition;
};

class Locker
{
public:
	explicit Locker(Lock& l, bool bLocked = true):m_lock(&l),m_bLocked(false)
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

friend class Condition;
};

class Condition
{
public:
	Condition()					{ InitializeConditionVariable(&m_cond); }
	~Condition()				{ }
	bool sleep(Locker& cs, DWORD milli = INFINITE)
								{ return cs.m_bLocked && cs.m_lock && !!SleepConditionVariableCS(&m_cond, &cs.m_lock->m_cs, milli); }
	void wake()					{ WakeConditionVariable(&m_cond); }
	void wakeAll()				{ WakeAllConditionVariable(&m_cond); }

private:
	CONDITION_VARIABLE m_cond;
};