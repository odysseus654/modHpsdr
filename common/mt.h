/*
	Copyright 2012 Erik Anderson

	Licensed under the Apache License, Version 2.0 (the "License");
	you may not use this file except in compliance with the License.
	You may obtain a copy of the License at

		http://www.apache.org/licenses/LICENSE-2.0

	Unless required by applicable law or agreed to in writing, software
	distributed under the License is distributed on an "AS IS" BASIS,
	WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
	See the License for the specific language governing permissions and
	limitations under the License.
*/
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

	Lock(const Lock& other);
	Lock& operator=(const Lock& other);

	friend class Condition;
};

class Locker
{
public:
	explicit inline Locker(Lock& l, bool bLocked = true):m_lock(&l),m_bLocked(false)
	{
		if(bLocked) lock();
	}

	~Locker()
	{
		unlock();
	}

	inline void set(Lock& lock)
	{
		unlock();
		m_lock = &lock;
	}

	inline void lock()
	{
		if(m_lock && !m_bLocked)
		{
			m_lock->lock();
			m_bLocked = true;
		}
	}

	inline bool tryLock()
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

	inline void unlock()
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

	Locker(const Locker& other);
	Locker& operator=(const Locker& other);

	friend class Condition;
};

class RWLock
{
public:
	inline RWLock()				{ InitializeSRWLock(&m_lock); }
	~RWLock()					{ }
	inline void lockRead()		{ AcquireSRWLockShared(&m_lock); }
	inline void unlockRead()	{ ReleaseSRWLockShared(&m_lock); }
	inline bool tryLockRead()	{ return !!TryAcquireSRWLockShared(&m_lock); }
	inline void lockWrite()		{ AcquireSRWLockExclusive(&m_lock); }
	inline void unlockWrite()	{ ReleaseSRWLockExclusive(&m_lock); }
	inline bool tryLockWrite()	{ return !!TryAcquireSRWLockExclusive(&m_lock); }

private:
	SRWLOCK m_lock;

	RWLock(const RWLock& other);
	RWLock& operator=(const RWLock& other);

	friend class Condition;
};

class ReadLocker
{
public:
	explicit inline ReadLocker(RWLock& l, bool bLocked = true):m_lock(&l),m_bLocked(false)
	{
		if(bLocked) lock();
	}

	~ReadLocker()
	{
		unlock();
	}

	inline void set(RWLock& lock)
	{
		unlock();
		m_lock = &lock;
	}

	inline void lock()
	{
		if(m_lock && !m_bLocked)
		{
			m_lock->lockRead();
			m_bLocked = true;
		}
	}

	inline bool tryLock()
	{
		if(m_lock && (m_bLocked || m_lock->tryLockRead()))
		{
			m_bLocked = true;
			return true;
		}
		else
		{
			return false;
		}
	}

	inline void unlock()
	{
		if(m_lock && m_bLocked)
		{
			m_lock->unlockRead();
			m_bLocked = false;
		}
	}

private:
	RWLock* m_lock;
	bool    m_bLocked;

	ReadLocker(const ReadLocker& other);
	ReadLocker& operator=(const ReadLocker& other);

	friend class Condition;
};

class WriteLocker
{
public:
	explicit inline WriteLocker(RWLock& l, bool bLocked = true):m_lock(&l),m_bLocked(false)
	{
		if(bLocked) lock();
	}

	~WriteLocker()
	{
		unlock();
	}

	inline void set(RWLock& lock)
	{
		unlock();
		m_lock = &lock;
	}

	inline void lock()
	{
		if(m_lock && !m_bLocked)
		{
			m_lock->lockWrite();
			m_bLocked = true;
		}
	}

	inline bool tryLock()
	{
		if(m_lock && (m_bLocked || m_lock->tryLockWrite()))
		{
			m_bLocked = true;
			return true;
		}
		else
		{
			return false;
		}
	}

	inline void unlock()
	{
		if(m_lock && m_bLocked)
		{
			m_lock->unlockWrite();
			m_bLocked = false;
		}
	}

private:
	RWLock* m_lock;
	bool    m_bLocked;

	WriteLocker(const WriteLocker& other);
	WriteLocker& operator=(const WriteLocker& other);

	friend class Condition;
};

class Condition
{
public:
	inline Condition()			{ InitializeConditionVariable(&m_cond); }
	~Condition()				{ }
	inline bool sleep(Locker& cs, DWORD milli = INFINITE)
								{ return cs.m_bLocked && cs.m_lock && !!SleepConditionVariableCS(&m_cond, &cs.m_lock->m_cs, milli); }
	inline bool sleep(ReadLocker& cs, DWORD milli = INFINITE)
								{ return cs.m_bLocked && cs.m_lock && !!SleepConditionVariableSRW(&m_cond, &cs.m_lock->m_lock, milli, CONDITION_VARIABLE_LOCKMODE_SHARED); }
	inline bool sleep(WriteLocker& cs, DWORD milli = INFINITE)
								{ return cs.m_bLocked && cs.m_lock && !!SleepConditionVariableSRW(&m_cond, &cs.m_lock->m_lock, milli, 0); }
	inline void wake()			{ WakeConditionVariable(&m_cond); }
	inline void wakeAll()		{ WakeAllConditionVariable(&m_cond); }

private:
	CONDITION_VARIABLE m_cond;

	Condition(const Condition& other);
	Condition& operator=(const Condition& other);
};

class Semaphore
{
public:
	inline Semaphore():m_sem(NULL)				{ }
	inline Semaphore(unsigned count, unsigned maxCount) { open(count, maxCount); }
	~Semaphore()								{ close(); }
	inline bool sleep(DWORD milli = INFINITE)	{ return m_sem && WaitForSingleObject(m_sem, milli) == WAIT_OBJECT_0 && m_sem; }
	inline bool wake(unsigned count = 1)		{ return m_sem && ReleaseSemaphore(m_sem, count, NULL); }
	inline bool isOpen() const					{ return !!m_sem; }
//	void reset()								{ while(sleep(0)); }

	void open(unsigned count, unsigned maxCount);
	void close();

private:
	HANDLE m_sem;

	Semaphore(const Semaphore& other);
	Semaphore& operator=(const Semaphore& other);
};

template<class ThreadOper, class StaticRetVal, class PARM1=fastdelegate::detail::DefaultVoid, class PARM2=fastdelegate::detail::DefaultVoid>
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

	static StaticRetVal __stdcall staticCatch(void* parm)
	{
		ThreadDelegate<ThreadOper,StaticRetVal,PARM1,PARM2>* caller = (ThreadDelegate<ThreadOper,StaticRetVal,PARM1,PARM2>*)parm;
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

template<class ThreadOper, class StaticRetVal, class PARM>
class ThreadDelegate<ThreadOper, StaticRetVal, PARM>
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

	static StaticRetVal __stdcall staticCatch(void* parm)
	{
		ThreadDelegate<ThreadOper,StaticRetVal,PARM>* caller = (ThreadDelegate<ThreadOper,StaticRetVal,PARM>*)parm;
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

template<class ThreadOper, class StaticRetVal>
class ThreadDelegate<ThreadOper, StaticRetVal>
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

	static StaticRetVal __stdcall staticCatch(void* parm)
	{
		ThreadDelegate<ThreadOper,StaticRetVal>* caller = (ThreadDelegate<ThreadOper,StaticRetVal>*)parm;
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
class AsyncDelegate : public ThreadDelegate<AsyncDelegateOper, DWORD, PARM1, PARM2>
{
private:
	typedef ThreadDelegate<AsyncDelegateOper, DWORD, PARM1, PARM2> base_type;
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
class Thread : public ThreadBase, protected ThreadDelegate<ThreadBase::ThreadLaunchOper, unsigned, PARM1, PARM2>
{
private:
	typedef ThreadDelegate<ThreadBase::ThreadLaunchOper, unsigned, PARM1, PARM2> base_type;
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
class Thread<PARM> : public ThreadBase, protected ThreadDelegate<ThreadBase::ThreadLaunchOper, unsigned, PARM>
{
private:
	typedef ThreadDelegate<ThreadBase::ThreadLaunchOper, unsigned, PARM> base_type;
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
class Thread<> : public ThreadBase, protected ThreadDelegate<ThreadBase::ThreadLaunchOper, unsigned>
{
private:
	typedef ThreadDelegate<ThreadBase::ThreadLaunchOper, unsigned> base_type;
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