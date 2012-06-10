#pragma once
#include "error.h"
#include "../ext/FastDelegate.h"
#include <intrin.h>

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
		if(!m_sem) ThrowLastError(GetLastError());
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

template<class PARM1=fastdelegate::detail::DefaultVoid, class PARM2=fastdelegate::detail::DefaultVoid>
class AsyncDelegate
{
public:
	inline AsyncDelegate(fastdelegate::FastDelegate2<PARM1,PARM2> func):m_func(func),m_pending(0) {}
	inline bool isPending() const { return !!m_pending; }

	void fire(const PARM1& parm1, const PARM2& parm2)
	{
		ASSERT(!m_pending);
		m_parm1 = parm1;
		m_parm2 = parm2;
		if(!QueueUserWorkItem(staticCatch, this, WT_EXECUTEDEFAULT)) ThrowLastError(GetLastError());
		_InterlockedIncrement(&m_pending);
	}
private:
	fastdelegate::FastDelegate2<PARM1,PARM2> m_func;
	PARM1 m_parm1;
	PARM2 m_parm2;
	volatile long m_pending;

	static DWORD __stdcall staticCatch(void* parm)
	{
		AsyncDelegate<PARM1,PARM2>* caller = (AsyncDelegate<PARM1,PARM2>*)parm;
		if(!caller) { ASSERT(FALSE); return 1; }
		ASSERT(caller->m_pending && caller->m_func);
		_InterlockedDecrement(&caller->m_pending);
		try
		{
			caller->m_func(caller->m_parm1, caller->m_parm2);
		}
		catch(...)
		{
	#ifdef _DEBUG
			DebugBreak();
	#endif
			return 3;
		}
		return 0;
	}
};

template<class PARM>
class AsyncDelegate<PARM>
{
public:
	inline AsyncDelegate(fastdelegate::FastDelegate1<PARM> func):m_func(func),m_pending(0) {}
	inline bool isPending() const { return !!m_pending; }

	void fire(const PARM& parm)
	{
		ASSERT(!m_pending);
		m_parm = parm;
		if(!QueueUserWorkItem(staticCatch, this, WT_EXECUTEDEFAULT)) ThrowLastError(GetLastError());
		_InterlockedIncrement(&m_pending);
	}
private:
	fastdelegate::FastDelegate1<PARM> m_func;
	PARM m_parm;
	volatile long m_pending;

	static DWORD __stdcall staticCatch(void* parm)
	{
		AsyncDelegate<PARM>* caller = (AsyncDelegate<PARM>*)parm;
		if(!caller) { ASSERT(FALSE); return 1; }
		ASSERT(caller->m_pending && caller->m_func);
		_InterlockedDecrement(&caller->m_pending);
		try
		{
			caller->m_func(caller->m_parm);
		}
		catch(...)
		{
	#ifdef _DEBUG
			DebugBreak();
	#endif
			return 3;
		}
		return 0;
	}
};

template<>
class AsyncDelegate<>
{
public:
	inline AsyncDelegate(fastdelegate::FastDelegate0<> func):m_func(func),m_pending(0) {}
	inline bool isPending() const { return !!m_pending; }

	void fire()
	{
		ASSERT(!m_pending);
		if(!QueueUserWorkItem(staticCatch, this, WT_EXECUTEDEFAULT)) ThrowLastError(GetLastError());
		_InterlockedIncrement(&m_pending);
	}
private:
	fastdelegate::FastDelegate0<> m_func;
	volatile long m_pending;

	static DWORD __stdcall staticCatch(void* parm)
	{
		AsyncDelegate<>* caller = (AsyncDelegate<>*)parm;
		if(!caller) { ASSERT(FALSE); return 1; }
		ASSERT(caller->m_pending && caller->m_func);
		_InterlockedDecrement(&caller->m_pending);
		try
		{
			caller->m_func();
		}
		catch(...)
		{
	#ifdef _DEBUG
			DebugBreak();
	#endif
			return 3;
		}
		return 0;
	}
};

void SetThreadName(const char* threadName, DWORD dwThreadID = -1);
