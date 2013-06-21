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

#include "HPSDRDevice.h"

namespace hpsdr {

// ------------------------------------------------------------------ generic attribute handlers

template<signals::EType ET>
class COptionedAttribute : public CRWAttribute<ET>
{
public:
	inline COptionedAttribute(const char* name, const char* descr, const store_type& deflt,
		unsigned numOpt, const store_type* valList, const char** optList)
		:CRWAttribute<ET>(name, descr, deflt), m_numOpts(optList ? numOpt : 0),m_valList(valList),m_optList(optList)
	{
	}

	virtual ~COptionedAttribute() { }

	virtual unsigned options(const void* vals, const char** opts, unsigned availElem)
	{
		if((vals||opts) && availElem)
		{
			unsigned numCopy = min(availElem, m_numOpts);
			for(unsigned idx=0; idx < numCopy; idx++)
			{
				if(vals) ((store_type*)vals)[idx] = m_valList[idx];
				if(opts) opts[idx] = m_optList[idx];
			}
		}
		return m_numOpts;
	}

private:
	unsigned m_numOpts;
	const store_type* m_valList;
	const char** m_optList;
};

template<signals::EType ET>
class CNumOptionedAttribute : public CRWAttribute<ET>
{
public:
	inline CNumOptionedAttribute(const char* name, const char* descr, const store_type& deflt, unsigned numOpt, const char** optList)
		:CRWAttribute<ET>(name, descr, deflt), m_numOpts(optList ? numOpt : 0),m_optList(optList)
	{
	}

	virtual ~CNumOptionedAttribute() { }

	virtual unsigned options(const void* vals, const char** opts, unsigned availElem)
	{
		if((vals||opts) && availElem)
		{
			unsigned numCopy = min(availElem, m_numOpts);
			for(unsigned idx=0; idx < numCopy; idx++)
			{
				if(vals) ((store_type*)vals)[idx] = (store_type)idx;
				if(opts) opts[idx] = m_optList[idx];
			}
		}
		return m_numOpts;
	}

	virtual bool isValidValue(const store_type& newVal) const
	{
		return newVal >= 0 && (!m_numOpts || newVal < m_numOpts);
	}

private:
	unsigned m_numOpts;
	const char** m_optList;
};

class CAttr_outBit : public CRWAttribute<signals::etypBoolean>
{
private:
	typedef CRWAttribute<signals::etypBoolean> base;
public:
	inline CAttr_outBit(CHpsdrDevice& parent, const char* name, const char* descr, bool deflt, byte addr, byte offset, byte mask)
		:base(name, descr, deflt ? 1 : 0),m_parent(parent),m_addr(addr),m_offset(offset),m_mask(mask)
	{
		if(deflt) m_parent.setCCbits(m_addr, m_offset, m_mask, m_mask);
	}

	virtual ~CAttr_outBit() { }

protected:
	virtual void onSetValue(const store_type& newVal)
	{
		m_parent.setCCbits(m_addr, m_offset, m_mask, !!newVal ? m_mask : 0);
		base::onSetValue(newVal);
	}

private:
	CHpsdrDevice& m_parent;
	const byte m_addr;
	const byte m_offset;
	const byte m_mask;
};

class CAttr_inBit : public CROAttribute<signals::etypBoolean>, public CHpsdrDevice::CAttr_inMonitor
{
private:
	typedef CROAttribute<signals::etypBoolean> base;
public:
	inline CAttr_inBit(const char* name, const char* descr, bool deflt, byte addr, byte offset, byte mask)
		:base(name, descr, deflt ? 1 : 0), CAttr_inMonitor(addr), m_offset(offset), m_mask(mask)
	{
	}

	virtual ~CAttr_inBit() { }

	virtual void evaluate(byte* inVal)
	{
		if(m_offset >= 4) {ASSERT(FALSE);return;}
		privateSetValue((inVal[m_offset] & m_mask) ? 1 : 0);
	}

private:
	const byte m_offset;
	const byte m_mask;
};

class CAttr_outBits : public CNumOptionedAttribute<signals::etypByte>
{
private:
	typedef CNumOptionedAttribute<signals::etypByte> base;
public:
	inline CAttr_outBits(CHpsdrDevice& parent, const char* name, const char* descr, byte deflt,
		byte addr, byte offset, byte mask, byte shift, unsigned numOpt, const char** optList)
		:base(name, descr, deflt, numOpt, optList),m_parent(parent),m_addr(addr),m_offset(offset),
		 m_mask(mask),m_shift(shift)
	{
		if(deflt) setValue(deflt);
	}

	virtual ~CAttr_outBits() { }

protected:
	virtual void onSetValue(const store_type& newVal)
	{
		setValue(newVal);
		base::onSetValue(newVal);
	}

protected:
	CHpsdrDevice& m_parent;
private:
	const byte m_addr;
	const byte m_offset;
	const byte m_mask;
	const byte m_shift;

	inline void setValue(const store_type& newVal)
	{
		m_parent.setCCbits(m_addr, m_offset, m_mask, (newVal << m_shift) & m_mask);
	}
};

class CAttr_inBits : public CROAttribute<signals::etypByte>, public CHpsdrDevice::CAttr_inMonitor
{
private:
	typedef CROAttribute<signals::etypByte> base;
public:
	inline CAttr_inBits(const char* name, const char* descr, byte deflt, byte addr, byte offset, byte mask, byte shift)
		:base(name, descr, deflt), CAttr_inMonitor(addr), m_offset(offset), m_mask(mask), m_shift(shift)
	{
	}

	virtual ~CAttr_inBits() { }

	virtual void evaluate(byte* inVal)
	{
		if(m_offset >= 4) {ASSERT(FALSE);return;}
		privateSetValue((inVal[m_offset] & m_mask) >> m_shift);
	}

private:
	const byte m_offset;
	const byte m_mask;
	const byte m_shift;
};

class CAttr_outLong : public CRWAttribute<signals::etypLong>
{
private:
	typedef CRWAttribute<signals::etypLong> base;
public:
	inline CAttr_outLong(CHpsdrDevice& parent, const char* name, const char* descr, long deflt, byte addr)
		:base(name, descr, deflt), m_parent(parent), m_addr(addr)
	{
		if(deflt) setValue(deflt);
	}

//	virtual ~CAttr_outLong() { }

protected:
	virtual void onSetValue(const store_type& newVal)
	{
		setValue(newVal);
		base::onSetValue(newVal);
	}

private:
	CHpsdrDevice& m_parent;
	byte m_addr;

	inline void setValue(const store_type& newVal)
	{
		m_parent.setCCint(m_addr, newVal);
	}
};

class CAttr_inShort : public CROAttribute<signals::etypShort>, public CHpsdrDevice::CAttr_inMonitor
{
private:
	typedef CROAttribute<signals::etypShort> base;
public:
	inline CAttr_inShort(const char* name, const char* descr, short deflt, byte addr, byte offset)
		:base(name, descr, deflt), CAttr_inMonitor(addr), m_offset(offset)
	{
	}

	virtual ~CAttr_inShort() { }

	virtual void evaluate(byte* inVal)
	{
		if(m_offset >= 3) {ASSERT(FALSE);return;}
		privateSetValue((inVal[m_offset] << 8) | inVal[m_offset+1]);
	}

private:
	const byte m_offset;
};

// ------------------------------------------------------------------ attribute proxies

template<signals::EType ET>
class CAttr_outProxy : public CRWAttribute<ET>, public CHpsdrDevice::Receiver::IAttrProxy<CRWAttribute<ET> >
{
private:
	typedef CRWAttribute<ET> base_type;
	Lock m_proxyLock;

public:
	inline CAttr_outProxy(const char* pName, const char* pDescr, base_type& ref)
		:base_type(pName, pDescr, ref.nativeGetValue()), refObject(ref), proxyObject(NULL) { }

	virtual void setProxy(base_type& target)
	{
		Locker lock(m_proxyLock);
		proxyObject = &target;
		if(proxyObject) proxyObject->nativeSetValue(nativeGetValue());
	}

	virtual bool isValidValue(const store_type& newVal) const { return refObject.isValidValue(newVal); }

protected:
	virtual void onSetValue(const store_type& value)
	{
		Locker lock(m_proxyLock);
		if(proxyObject) proxyObject->nativeSetValue(value);
	}

	base_type& refObject;
	base_type* proxyObject;
};

class CAttr_inProxy : public CAttributeBase, public CHpsdrDevice::Receiver::IAttrProxy<signals::IAttribute>
{
public:
	inline CAttr_inProxy(const char* pName, const char* pDescr, signals::IAttribute& ref)
		:CAttributeBase(pName, pDescr), refObject(ref), proxyObject(NULL) { }
	virtual ~CAttr_inProxy()			{ }
	virtual unsigned options(const void* vals, const char** opts, unsigned availElem)
		{ return refObject.options(vals,opts,availElem); }
	virtual signals::EType Type()		{ return refObject.Type(); }
	virtual BOOL isReadOnly()			{ return true; }
	virtual BOOL setValue(const void* newVal) { UNUSED_ALWAYS(newVal); return false; }
	virtual const void* getValue()		{ Locker lock(m_proxyLock); return proxyObject ? proxyObject->getValue() : NULL; }
	virtual void setProxy(signals::IAttribute& target);

private:
	CAttr_inProxy(const CAttr_inProxy& other);
	CAttr_inProxy& operator=(const CAttr_inProxy& other);
private:
	Lock m_proxyLock;
	signals::IAttribute& refObject;
	signals::IAttribute* proxyObject;
};

// ------------------------------------------------------------------ special attribute handlers

class CAttr_out_alex_recv_ant : public CNumOptionedAttribute<signals::etypByte>
{
private:
	typedef CNumOptionedAttribute<signals::etypByte> base;
public:
	inline CAttr_out_alex_recv_ant(CHpsdrDevice& parent, const char* name, const char* descr, byte deflt,
		unsigned numOpt, const char** optList)
		:base(name, descr, deflt, numOpt, optList),m_parent(parent)
	{
		if(deflt) setValue(deflt);
	}

	virtual ~CAttr_out_alex_recv_ant() { }

protected:
	virtual void onSetValue(const store_type& newVal)
	{
		setValue(newVal);
		base::onSetValue(newVal);
	}

private:
	CHpsdrDevice& m_parent;

	inline void setValue(const store_type& newVal)
	{
		m_parent.setCCbits(0, 2, 0xe0, newVal ? ((newVal << 5) & 0x60) | 0x80 : 0);
	}
};

class CAttr_out_mic_src : public CNumOptionedAttribute<signals::etypByte>
{
private:
	typedef CNumOptionedAttribute<signals::etypByte> base;
public:
	inline CAttr_out_mic_src(CHpsdrDevice& parent, const char* name, const char* descr, byte deflt,
		unsigned numOpt, const char** optList)
		:base(name, descr, deflt, numOpt, optList),m_parent(parent)
	{
		if(deflt) setValue(deflt);
	}

	virtual ~CAttr_out_mic_src() { }

protected:
	virtual void onSetValue(const store_type& newVal)
	{
		setValue(newVal);
		base::onSetValue(newVal);
	}

private:
	CHpsdrDevice& m_parent;

	inline void setValue(const store_type& newVal)
	{
		m_parent.setCCbits(0, 0, 0x80, newVal == 0 ? 0 : 0x80);
		m_parent.setCCbits(9, 1, 0x02, newVal <= 1 ? 0 : 0x02);
	}
};

class CAttr_out_recv_speed : public COptionedAttribute<signals::etypLong>
{
private:
	typedef COptionedAttribute<signals::etypLong> base;
	static const long recv_speed_options[3];
	bool setValue(const store_type& newVal);
public:
	inline CAttr_out_recv_speed(CHpsdrDevice& parent, const char* name, const char* descr, long deflt,
		byte addr, byte offset, byte mask, byte shift)
		:base(name, descr, deflt, _countof(recv_speed_options), recv_speed_options, NULL),
		 m_rawValue(parent, name, descr, 0, addr, offset, mask, shift, 0, NULL),
		 m_parent(parent)
	{
		VERIFY(setValue(deflt));
	}

	virtual bool isValidValue(const store_type& newVal) const;

protected:
	CAttr_outBits m_rawValue;
	CHpsdrDevice& m_parent;

protected:
	virtual void onSetValue(const store_type& newVal)
	{
		if(setValue(newVal))
		{
			base::onSetValue(((newVal + 500) / 1000) * 1000);
		}
	}
};

}