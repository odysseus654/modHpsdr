/*
	buffer.h - a statically-allocated buffer intended for passing data between threads

	Copyright 2012-2013 Erik Anderson

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

#include "mt.h"
#include <vector>

template<class Elem>
class Buffer
{
protected:
	typedef std::vector<Elem> vector_type;
public:
	typedef typename vector_type::size_type size_type;
	typedef typename vector_type::difference_type difference_type;
	typedef typename vector_type::pointer pointer;
	typedef typename vector_type::const_pointer const_pointer;
	typedef typename vector_type::reference reference;
	typedef typename vector_type::const_reference const_reference;
	typedef typename vector_type::value_type value_type;

	explicit Buffer(size_type size)
		:m_buffer(size+1),m_back(0),m_front(0)
	{
	}

#ifdef _DEBUG
	~Buffer()
	{
	}
#endif

	size_type size() const
	{
		Locker lock(m_lock);
		return (m_back > m_front ? m_back : m_back+m_buffer.size()) - m_front;
	}

	size_type capacity() const
	{
		Locker lock(m_lock);
		return m_buffer.size() - 1;
	}

	bool empty() const
	{
		Locker lock(m_lock);
		return m_back == m_front;
	}

	bool pop_front(reference val, DWORD milli = INFINITE)
	{
		Locker lock(m_lock);
		while(m_back == m_front)
		{
			if(!m_notEmpty.sleep(lock, milli)) return false;
		}
		val = m_buffer[m_front];
		m_front = (m_front + 1) % m_buffer.size();
		m_notFull.wakeAll();
		return true;
	}

	unsigned pop_front_vector(pointer val, unsigned numAvail, DWORD milli = INFINITE)
	{
		Locker lock(m_lock);
		return pop_front_vector_locked(lock, val, numAvail, milli);
	}

protected:
	unsigned pop_front_vector_locked(Locker& lock, pointer val, unsigned numAvail, DWORD milli = INFINITE)
	{
		while(m_back == m_front)
		{
			if(!m_notEmpty.sleep(lock, milli)) return 0;
		}
		size_type bufSize = m_buffer.size();
		unsigned numRead = min((m_back > m_front ? m_back : bufSize) - m_front, numAvail);
		memcpy(val, &m_buffer[m_front], sizeof(Elem) * numRead);
		m_front = (m_front + numRead) % bufSize;
		if(numAvail > numRead && m_back != m_front)
		{
			return numRead + pop_front_vector_locked(lock, val + numRead, numAvail - numRead, INFINITE);
		} else {
			m_notFull.wakeAll();
			return numRead;
		}
	}

public:
	bool pop_front_noblock(reference val)
	{
		Locker lock(m_lock);
		if(m_back == m_front) return false;
		val = m_buffer[m_front];
		m_front = (m_front + 1) % m_buffer.size();
		m_notFull.wakeAll();
		return true;
	}

	bool push_back(const_reference val, DWORD milli = INFINITE)
	{
		Locker lock(m_lock);
		size_type bufSize = m_buffer.size();
		while((m_back+1)%bufSize == m_front)
		{
			if(!m_notFull.sleep(lock, milli)) return false;
		}
		m_buffer[m_back] = val;
		m_back = (m_back + 1) % bufSize;
		m_notEmpty.wakeAll();
		return true;
	}

	unsigned push_back_vector(pointer val, unsigned numAvail, DWORD milli = INFINITE)
	{
		Locker lock(m_lock);
		return push_back_vector_locked(lock, val, numAvail, milli);
	}

protected:
	unsigned push_back_vector_locked(Locker& lock, pointer val, unsigned numAvail, DWORD milli = INFINITE)
	{
		size_type bufSize = m_buffer.size();
		while((m_back+1)%bufSize == m_front)
		{
			if(!m_notFull.sleep(lock, milli)) return 0;
		}
		unsigned numWrite = min((m_front > m_back ? m_front-1 : bufSize) - m_back, numAvail);
		memcpy(&m_buffer[m_back], val, sizeof(Elem) * numWrite);
		m_back = (m_back + numWrite) % bufSize;
		if(numAvail > numWrite && (m_back+1)%bufSize != m_front)
		{
			return numWrite + push_back_vector_locked(lock, val + numWrite, numAvail - numWrite, INFINITE);
		} else {
			m_notEmpty.wakeAll();
			return numWrite;
		}
	}

public:
	bool push_back_noblock(const_reference val)
	{
		Locker lock(m_lock);
		size_type bufSize = m_buffer.size()
		if((m_back+1)%bufSize == m_front) return false;
		m_buffer[m_back] = val;
		m_back = (m_back + 1) % bufSize;
		m_notEmpty.wakeAll();
		return true;
	}

protected:
	vector_type	m_buffer;
	size_type	m_back;
	size_type	m_front;
	mutable Lock	m_lock;
	Condition	m_notEmpty, m_notFull;

private:
	typedef Buffer<Elem> my_type;
	Buffer(const my_type& other);
	my_type& operator=(const my_type& other);
};