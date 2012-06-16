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

template<class ThreadOper, class PARM1=fastdelegate::detail::DefaultVoid, class PARM2=fastdelegate::detail::DefaultVoid>
class ThreadDelegate
{
protected:
	typedef fastdelegate::FastDelegate2<PARM1,PARM2> delegate_type;
public:
	inline ThreadDelegate(const delegate_type& func, const ThreadOper& oper = ThreadOper())
		:m_func(func),m_pending(0),m_oper(oper) {}
	inline bool isPending() const { return !!m_pending; }

	void fire(const PARM1& parm1, const PARM2& parm2)
	{
		ASSERT(!m_pending);
		m_parm1 = parm1;
		m_parm2 = parm2;
		_InterlockedIncrement(&m_pending);
		m_oper(staticCatch, this);
	}
private:
	delegate_type m_func;
	PARM1 m_parm1;
	PARM2 m_parm2;
	ThreadOper m_oper;
	volatile long m_pending;

	static DWORD __stdcall staticCatch(void* parm)
	{
		ThreadDelegate<ThreadOper,PARM1,PARM2>* caller = (ThreadDelegate<ThreadOper,PARM1,PARM2>*)parm;
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

template<class ThreadOper, class PARM>
class ThreadDelegate<ThreadOper, PARM>
{
protected:
	typedef fastdelegate::FastDelegate1<PARM> delegate_type;
public:
	inline ThreadDelegate(const delegate_type& func, const ThreadOper& oper = ThreadOper())
		:m_func(func),m_pending(0),m_oper(oper) {}
	inline bool isPending() const { return !!m_pending; }

	void fire(const PARM& parm)
	{
		ASSERT(!m_pending);
		m_parm = parm;
		_InterlockedIncrement(&m_pending);
		m_oper(staticCatch, this);
	}
private:
	delegate_type m_func;
	PARM m_parm;
	ThreadOper m_oper;
	volatile long m_pending;

	static DWORD __stdcall staticCatch(void* parm)
	{
		ThreadDelegate<ThreadOper,PARM>* caller = (ThreadDelegate<ThreadOper,PARM>*)parm;
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

template<class ThreadOper>
class ThreadDelegate<ThreadOper>
{
public:
	typedef fastdelegate::FastDelegate0<> delegate_type;
	inline ThreadDelegate(const delegate_type& func, const ThreadOper& oper = ThreadOper())
		:m_func(func),m_pending(0),m_oper(oper) {}
	inline bool isPending() const { return !!m_pending; }

	void fire()
	{
		ASSERT(!m_pending);
		_InterlockedIncrement(&m_pending);
		m_oper(staticCatch, this);
	}
private:
	delegate_type m_func;
	ThreadOper m_oper;
	volatile long m_pending;

	static unsigned __stdcall staticCatch(void* parm)
	{
		ThreadDelegate<ThreadOper>* caller = (ThreadDelegate<ThreadOper>*)parm;
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

struct AsyncDelegateOper
{
	inline void operator()(LPTHREAD_START_ROUTINE start, void* param)
	{
		if(!QueueUserWorkItem(start, param, WT_EXECUTEDEFAULT)) ThrowLastError(GetLastError());
	}
};

template<class PARM1=fastdelegate::detail::DefaultVoid, class PARM2=fastdelegate::detail::DefaultVoid>
class AsyncDelegate : public ThreadDelegate<AsyncDelegateOper, PARM1, PARM2>
{
private:
	typedef ThreadDelegate<AsyncDelegateOper, PARM1, PARM2> base_type;
public:
	inline AsyncDelegate(const delegate_type& func):base_type(func) {}
};

class ThreadBase
{
public:
	inline ThreadBase():m_hdl(INVALID_HANDLE_VALUE) {}
	static void SetThreadName(const char* threadName, DWORD dwThreadID = -1);

	void close();

	inline bool running() const
	{
		return m_hdl != INVALID_HANDLE_VALUE && WaitForSingleObject(const_cast<HANDLE>(m_hdl), 0) == WAIT_TIMEOUT;
	}

	inline void suspend()
	{
		if(m_hdl != INVALID_HANDLE_VALUE && SuspendThread(m_hdl) == -1L) ThrowLastError(GetLastError());
	}

	inline void resume()
	{
		if(m_hdl != INVALID_HANDLE_VALUE && ResumeThread(m_hdl) == -1L) ThrowLastError(GetLastError());
	}
protected:
	struct ThreadLaunchOper
	{
		inline ThreadLaunchOper(ThreadBase* parent):m_parent(parent){}
		inline void operator()(unsigned ( __stdcall *start )( void * ), void* param) { m_parent->doThreadLaunch(start, param); }
		ThreadBase* m_parent;
	};

	void doThreadLaunch(unsigned ( __stdcall *start )( void * ), void* param);

	int m_reqPriority;
	bool m_reqSuspended;
	HANDLE m_hdl;
};

#pragma warning(push)
#pragma warning(disable: 4355)

template<class PARM1=fastdelegate::detail::DefaultVoid, class PARM2=fastdelegate::detail::DefaultVoid>
class Thread : public ThreadBase, protected ThreadDelegate<ThreadBase::ThreadLaunchOper, PARM1, PARM2>
{
private:
	typedef ThreadDelegate<ThreadBase::ThreadLaunchOper, PARM1, PARM2> base_type;
public:
	typedef typename base_type::delegate_type delegate_type;
	inline Thread(const delegate_type& func):base_type(func, ThreadLaunchOper(this)) {}

	void launch(const PARM1& parm1, const PARM2& parm2, int priority = THREAD_PRIORITY_NORMAL, bool suspended = false)
	{
		m_reqPriority = priority;
		m_reqSuspended = suspended;
		fire(parm1, parm2);
	}
};

template<class PARM>
class Thread<PARM> : public ThreadBase, protected ThreadDelegate<ThreadBase::ThreadLaunchOper, PARM>
{
private:
	typedef ThreadDelegate<ThreadBase::ThreadLaunchOper, PARM> base_type;
public:
	typedef typename base_type::delegate_type delegate_type;
	inline Thread(const delegate_type& func):base_type(func, ThreadLaunchOper(this)) {}

	void launch(const PARM& parm1, int priority = THREAD_PRIORITY_NORMAL, bool suspended = false)
	{
		m_reqPriority = priority;
		m_reqSuspended = suspended;
		fire(parm1);
	}
};

template<>
class Thread<> : public ThreadBase, protected ThreadDelegate<ThreadBase::ThreadLaunchOper>
{
private:
	typedef ThreadDelegate<ThreadBase::ThreadLaunchOper> base_type;
public:
	typedef base_type::delegate_type delegate_type;
	inline Thread(const delegate_type& func):base_type(func, ThreadLaunchOper(this)) {}

	void launch(int priority = THREAD_PRIORITY_NORMAL, bool suspended = false)
	{
		m_reqPriority = priority;
		m_reqSuspended = suspended;
		fire();
	}
};

#pragma warning(pop)