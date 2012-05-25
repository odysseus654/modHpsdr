#pragma once

class Condition;

class Lock
{
public:
	inline Lock()			{ InitializeCriticalSection(&m_cs); }
	~Lock()					{ DeleteCriticalSection(&m_cs); }
	inline void lock()		{ EnterCriticalSection(&m_cs); }
	inline void unlock()	{ LeaveCriticalSection(&m_cs); }
	inline bool tryLock()	{ return !!TryEnterCriticalSection(&m_cs); }

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
	inline Condition()			{ InitializeConditionVariable(&m_cond); }
	~Condition()				{ }
	inline bool sleep(Locker& cs, DWORD milli = INFINITE)
								{ return cs.m_bLocked && cs.m_lock && !!SleepConditionVariableCS(&m_cond, &cs.m_lock->m_cs, milli); }
	inline void wake()			{ WakeConditionVariable(&m_cond); }
	inline void wakeAll()		{ WakeAllConditionVariable(&m_cond); }

private:
	CONDITION_VARIABLE m_cond;
};

class Semaphore
{
public:
	inline Semaphore():m_sem(NULL)				{ }
	inline Semaphore(unsigned count, unsigned maxCount) { open(count, maxCount); }
	~Semaphore()								{ close(); }
	inline bool sleep(DWORD milli = INFINITE)	{ return m_sem && WaitForSingleObject(m_sem, milli) == WAIT_OBJECT_0; }
	inline bool wake(unsigned count = 1)		{ return m_sem && ReleaseSemaphore(m_sem, count, NULL); }
	inline bool isOpen() const					{ return !!m_sem; }
//	void reset()								{ while(sleep(0)); }

	void open(unsigned count, unsigned maxCount)
	{
		close();
		m_sem = CreateSemaphore(NULL, count, maxCount, NULL);
	}

	void close()
	{
		if(m_sem)
		{
			CloseHandle(m_sem);
			m_sem = NULL;
		}
	}

private:
	HANDLE m_sem;
};
