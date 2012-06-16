#include "stdafx.h"
#include "mt.h"

#include <process.h>

void ThreadBase::SetThreadName(const char* threadName, DWORD dwThreadID /* = -1 */)
{
	#pragma pack(push,8)
	typedef struct tagTHREADNAME_INFO
	{
		DWORD dwType; // Must be 0x1000.
		LPCSTR szName; // Pointer to name (in user addr space).
		DWORD dwThreadID; // Thread ID (-1=caller thread).
		DWORD dwFlags; // Reserved for future use, must be zero.
	} THREADNAME_INFO;
	#pragma pack(pop)
	enum { MS_VC_EXCEPTION = 0x406D1388 };

	THREADNAME_INFO info;
	info.dwType = 0x1000;
	info.szName = threadName;
	info.dwThreadID = dwThreadID;
	info.dwFlags = 0;

	__try
	{
		RaiseException( MS_VC_EXCEPTION, 0, sizeof(info)/sizeof(ULONG_PTR), (ULONG_PTR*)&info );
	}
	__except(EXCEPTION_EXECUTE_HANDLER)
	{
	}
}

void ThreadBase::doThreadLaunch(unsigned ( __stdcall *start )( void * ), void* param)
{
	close();
	unsigned threadId;
	m_hdl = (HANDLE)_beginthreadex(NULL, 0, start, param, CREATE_SUSPENDED, &threadId);
	if(m_hdl == INVALID_HANDLE_VALUE) ThrowErrnoError(errno);
	if(m_reqPriority != THREAD_PRIORITY_NORMAL && !SetThreadPriority(m_hdl, THREAD_PRIORITY_TIME_CRITICAL)) ThrowLastError(GetLastError());
	if(!m_reqSuspended) resume();
}

void ThreadBase::close()
{
	if(m_hdl != INVALID_HANDLE_VALUE)
	{
		WaitForSingleObject(m_hdl, INFINITE);
		CloseHandle(m_hdl);
		m_hdl = INVALID_HANDLE_VALUE;
	}
}
