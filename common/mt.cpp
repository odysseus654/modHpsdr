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
#include "stdafx.h"
#include "mt.h"

#include <process.h>

void Semaphore::open(unsigned count, unsigned maxCount)
{
	close();
	m_sem = CreateSemaphore(NULL, count, maxCount, NULL);
	if(!m_sem) ThrowLastError(GetLastError());
}

void Semaphore::close()
{
	if(m_sem)
	{
		HANDLE sem = m_sem;
		m_sem = NULL;
		ReleaseSemaphore(sem, 1, NULL);
		ReleaseSemaphore(sem, 1, NULL);
		ReleaseSemaphore(sem, 1, NULL);
		CloseHandle(m_sem);
	}
}

// ------------------------------------------------------------------------------------------------ ThreadBase

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
