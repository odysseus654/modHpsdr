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
#include "block.h"

#include "buffer.h"
#include <complex>
#include <intrin.h>
#include <list>
#include <map>
#include <set>
#include <string>

class CRefcountObject
{
protected:
	inline CRefcountObject():m_refCount(0) {}
public:
	virtual ~CRefcountObject() {}
	inline unsigned AddRef() { return _InterlockedIncrement(&m_refCount); }
	inline unsigned Release();

private:
	CRefcountObject(const CRefcountObject& other);
	CRefcountObject& operator=(const CRefcountObject& other);
private:
	volatile long m_refCount;
};

class CInEndpointBase : public signals::IInEndpoint
{
protected:
	inline CInEndpointBase():m_connRecv(NULL) {}
public:
	virtual ~CInEndpointBase() { Disconnect(); }
	unsigned Read(signals::EType type, void* buffer, unsigned numAvail, BOOL bFillAll, unsigned msTimeout);

	virtual BOOL Connect(signals::IEPRecvFrom* recv);
	virtual BOOL isConnected() { return !!m_connRecv; }
	virtual BOOL Disconnect()  { return Connect(NULL); }
//	virtual const char* EPName();
//	virtual unsigned AddRef() = 0;
//	virtual unsigned Release() = 0;
//	virtual signals::EType Type() = 0;
//	virtual signals::IAttributes* Attributes() = 0;
//	virtual signals::IEPBuffer* CreateBuffer() = 0;

	void MergeRemoteAttrs(std::map<std::string,signals::IAttribute*>& attrs, unsigned flags);
	signals::IAttribute* RemoteGetByName(const char* name);

protected:
	virtual void OnConnection(signals::IEPRecvFrom*) { }
	inline signals::IEPRecvFrom* CurrentEndpoint() const { return m_connRecv; }
private:
	CInEndpointBase(const CInEndpointBase& other);
	CInEndpointBase& operator=(const CInEndpointBase& other);
private:
	RWLock m_connRecvLock;
	Condition m_connRecvConnected;
	signals::IEPRecvFrom* m_connRecv;
};

class COutEndpointBase : public signals::IOutEndpoint
{
protected:
	inline COutEndpointBase():m_connSend(NULL) {}
public:
	virtual ~COutEndpointBase() { Disconnect(); }
	unsigned Write(signals::EType type, void* buffer, unsigned numElem, unsigned msTimeout);

	virtual BOOL Connect(signals::IEPSendTo* send);
	virtual BOOL isConnected() { return !!m_connSend; }
	virtual BOOL Disconnect()  { return Connect(NULL); }
//	virtual const char* EPName();
//	virtual signals::EType Type() = 0;
//	virtual signals::IAttributes* Attributes() = 0;
//	virtual signals::IEPBuffer* CreateBuffer() = 0;
protected:
	virtual void OnConnection(signals::IEPSendTo*) { }
	inline signals::IEPSendTo* CurrentEndpoint() const { return m_connSend; }
private:
	COutEndpointBase(const COutEndpointBase& other);
	COutEndpointBase& operator=(const COutEndpointBase& other);
private:
	RWLock m_connSendLock;
	Condition m_connSendConnected;
	signals::IEPSendTo* m_connSend;
};

class CAttributeBase : public signals::IAttribute
{
protected:
	inline CAttributeBase(const char* pName, const char* pDescr):m_name(pName),m_descr(pDescr) { }
public:
	virtual ~CAttributeBase()
	{
		WriteLocker obslock(m_observersLock);
		for(TObserverList::const_iterator trans=m_observers.begin(); trans != m_observers.end(); trans++)
		{
			(*trans)->OnDetached(this);
		}
	}
	virtual const char* Name()			{ return m_name; }
	virtual const char* Description()	{ return m_descr; }
	virtual void Observe(signals::IAttributeObserver* obs);
	virtual void Unobserve(signals::IAttributeObserver* obs);
	virtual unsigned options(const void* /* vals */, const char** /* opts */, unsigned /* availElem */) { return 0; }
//	virtual signals::EType Type() = 0;
//	virtual BOOL isReadOnly();
//	virtual const void* getValue() = 0;
//	virtual BOOL setValue(const void* newVal) = 0;
private:
	CAttributeBase(const CAttributeBase& other);
	CAttributeBase& operator=(const CAttributeBase& other);
protected:
	typedef std::set<signals::IAttributeObserver*> TObserverList;
	TObserverList m_observers;
	RWLock        m_observersLock;
private:
	const char* m_name;
	const char* m_descr;
};

class CAttributesBase : public signals::IAttributes
{
protected:
	inline CAttributesBase() {}
public:
	virtual ~CAttributesBase();

	virtual unsigned Itemize(signals::IAttribute** attrs, unsigned availElem, unsigned flags);
	virtual signals::IAttribute* GetByName(const char* name);
	virtual void Observe(signals::IAttributeObserver* obs);
	virtual void Unobserve(signals::IAttributeObserver* obs);

private:
	CAttributesBase(const CAttributesBase& other);
	CAttributesBase& operator=(const CAttributesBase& other);

protected: // using a "fake generic" for typesafety
	template<class ATTR> ATTR* addRemoteAttr(const char* pName, ATTR* attr);
	template<class ATTR> ATTR* addLocalAttr(bool bVisible, ATTR* attr);
//	CAttributeBase* buildAttr(const char* name, signals::EType type, const char* descr, bool bReadOnly, bool bVisible);

protected:
	typedef std::map<const void*,CAttributeBase*> TVoidMapToAttr;
	typedef std::map<std::string,const void*> TStringMapToVoid;
	typedef std::set<CAttributeBase*> TAttrSet;
	TVoidMapToAttr   m_attributes;
	TStringMapToVoid m_attrNames;
	TAttrSet         m_ownedAttrs;
	TAttrSet         m_visibleAttrs;
};

class CCascadedAttributesBase : public CAttributesBase
{
protected:
	inline CCascadedAttributesBase(CInEndpointBase& src):m_src(src) {}
public:
	virtual unsigned Itemize(signals::IAttribute** attrs, unsigned availElem, unsigned flags);
	virtual signals::IAttribute* GetByName(const char* name);

private:
	CCascadedAttributesBase(const CCascadedAttributesBase& other);
	CCascadedAttributesBase& operator=(const CCascadedAttributesBase& other);

private:
	CInEndpointBase& m_src;
};

template<signals::EType ET, int DEFAULT_BUFSIZE = 4096>
class CSimpleIncomingChild : public CInEndpointBase, public CAttributesBase
{	// This class is assumed to be a static (non-dynamic) member of its parent
protected:
	inline CSimpleIncomingChild(signals::IBlock* parent):m_parent(parent) { }
public:
	virtual ~CSimpleIncomingChild() {}

protected:
	signals::IBlock* m_parent;

private:
	CSimpleIncomingChild(const CSimpleIncomingChild& other);
	CSimpleIncomingChild& operator=(const CSimpleIncomingChild& other);

public: // CInEndpointBase interface
//	virtual const char* EPName()				{ return EP_NAME; }
//	virtual const char* EPDescr()				{ return EP_DESCR; }
	virtual unsigned AddRef()					{ return m_parent->AddRef(); }
	virtual unsigned Release()					{ return m_parent->Release(); }
	virtual signals::EType Type()				{ return ET; }
	virtual signals::IAttributes* Attributes()	{ return this; }

	virtual signals::IEPBuffer* CreateBuffer()
	{
		signals::IEPBuffer* buff = new CEPBuffer<ET>(DEFAULT_BUFSIZE);
		buff->AddRef(NULL);
		return buff;
	}
};

template<signals::EType ET, int DEFAULT_BUFSIZE = 4096>
class CSimpleOutgoingChild : public COutEndpointBase, public CAttributesBase
{	// This class is assumed to be a static (non-dynamic) member of its parent
protected:
	inline CSimpleOutgoingChild() { }
public:
	virtual ~CSimpleOutgoingChild() {}

private:
	CSimpleOutgoingChild(const CSimpleOutgoingChild& other);
	CSimpleOutgoingChild& operator=(const CSimpleOutgoingChild& other);

public: // COutEndpointBase interface
	virtual signals::EType Type()				{ return ET; }
//	virtual const char* EPName()				{ return EP_NAME; }
//	virtual const char* EPDescr()				{ return EP_DESCR; }
	virtual signals::IAttributes* Attributes()	{ return this; }

	virtual signals::IEPBuffer* CreateBuffer()
	{
		signals::IEPBuffer* buffer = new CEPBuffer<ET>(DEFAULT_BUFSIZE);
		buffer->AddRef(NULL);
		return buffer;
	}
};

template<signals::EType ET, int DEFAULT_BUFSIZE = 4096>
class CSimpleCascadeOutgoingChild : public COutEndpointBase, public CCascadedAttributesBase
{	// This class is assumed to be a static (non-dynamic) member of its parent
protected:
	inline CSimpleCascadeOutgoingChild(CInEndpointBase& src):CCascadedAttributesBase(src) { }
public:
	virtual ~CSimpleCascadeOutgoingChild() {}

private:
	CSimpleCascadeOutgoingChild(const CSimpleCascadeOutgoingChild& other);
	CSimpleCascadeOutgoingChild& operator=(const CSimpleCascadeOutgoingChild& other);

public: // COutEndpointBase interface
	virtual signals::EType Type()				{ return ET; }
//	virtual const char* EPName()				{ return EP_NAME; }
//	virtual const char* EPDescr()				{ return EP_DESCR; }
	virtual signals::IAttributes* Attributes()	{ return this; }

	virtual signals::IEPBuffer* CreateBuffer()
	{
		signals::IEPBuffer* buffer = new CEPBuffer<ET>(DEFAULT_BUFSIZE);
		buffer->AddRef(NULL);
		return buffer;
	}
};

class CBlockBase : public signals::IBlock, public CAttributesBase, protected CRefcountObject
{
protected:
	inline CBlockBase(signals::IBlockDriver* driver):m_driver(driver) {};
public:
	virtual ~CBlockBase() {}

private:
	CBlockBase(const CBlockBase& other);
	CBlockBase& operator=(const CBlockBase& other);

public: // IBlock implementation
	virtual unsigned AddRef()				{ return CRefcountObject::AddRef(); }
	virtual unsigned Release()				{ return CRefcountObject::Release(); }
//	virtual const char* Name()				{ return NAME; }
	virtual unsigned NodeId(char* /* buff */ , unsigned /* availChar */) { return 0; }
	virtual signals::IBlockDriver* Driver()	{ return m_driver; }
	virtual signals::IBlock* Parent()		{ return NULL; }
	virtual signals::IAttributes* Attributes() { return this; }
	virtual unsigned Children(signals::IBlock** /* blocks */, unsigned /* availBlocks */) { return 0; }
	virtual unsigned Incoming(signals::IInEndpoint** /* ep */, unsigned /* availEP */) { return 0; }
	virtual unsigned Outgoing(signals::IOutEndpoint** /* ep */, unsigned /* availEP */) { return 0; }
	virtual void Start()					{}
	virtual void Stop()						{}

protected:
	signals::IBlockDriver* m_driver;

	static unsigned singleIncoming(signals::IInEndpoint* ep, signals::IInEndpoint** pEp, unsigned pAvailEP);
	static unsigned singleOutgoing(signals::IOutEndpoint* ep, signals::IOutEndpoint** pEp, unsigned pAvailEP);
};

class CThreadBlockBase : public CBlockBase
{
protected:
	inline CThreadBlockBase(signals::IBlockDriver* driver):CBlockBase(driver),m_bThreadEnabled(false),
		 m_thread(Thread<CThreadBlockBase*>::delegate_type(&process_thread))
	{};

public:
	virtual ~CThreadBlockBase() { stopThread(); }

private:
	CThreadBlockBase(const CThreadBlockBase& other);
	CThreadBlockBase& operator=(const CThreadBlockBase& other);

protected:
	virtual void thread_run() = 0;
	void startThread(int priority = THREAD_PRIORITY_NORMAL);
	void stopThread();
	inline bool threadRunning() const { return m_bThreadEnabled; }

private:
	Thread<CThreadBlockBase*> m_thread;
	bool m_bThreadEnabled;
	static void process_thread(CThreadBlockBase* owner);
};

template<signals::EType ET> struct StoreType;
template<> struct StoreType<signals::etypEvent>
{
	typedef void type;
};

template<> struct StoreType<signals::etypBoolean>
{
	typedef unsigned char type;
	typedef Buffer<type> buffer_type;
};

template<> struct StoreType<signals::etypWinHdl>
{
	typedef HANDLE type;
	typedef Buffer<type> buffer_type;
};

template<> struct StoreType<signals::etypByte>
{
	typedef unsigned char type;
	typedef Buffer<type> buffer_type;
};

template<> struct StoreType<signals::etypShort>
{
	typedef short type;
	typedef Buffer<type> buffer_type;
};

template<> struct StoreType<signals::etypLong>
{
	typedef long type;
	typedef Buffer<type> buffer_type;
};

template<> struct StoreType<signals::etypInt64>
{
	typedef __int64 type;
	typedef Buffer<type> buffer_type;
};

template<> struct StoreType<signals::etypSingle>
{
	typedef float type;
	typedef Buffer<type> buffer_type;
};

template<> struct StoreType<signals::etypDouble>
{
	typedef double type;
	typedef Buffer<type> buffer_type;
};

template<> struct StoreType<signals::etypComplex>
{
	typedef std::complex<float> type;
	typedef Buffer<type> buffer_type;
};

template<> struct StoreType<signals::etypCmplDbl>
{
	typedef std::complex<double> type;
	typedef Buffer<type> buffer_type;
};

template<> struct StoreType<signals::etypString>
{
	typedef std::string type;
};

template<> struct StoreType<signals::etypLRSingle>
{
	typedef std::complex<float> type;
	typedef Buffer<type> buffer_type;
};

template<> struct StoreType<signals::etypVecBoolean>
{
	typedef unsigned char type;
	typedef VectorBuffer<type> buffer_type;
};

template<> struct StoreType<signals::etypVecByte>
{
	typedef unsigned char type;
	typedef VectorBuffer<type> buffer_type;
};

template<> struct StoreType<signals::etypVecShort>
{
	typedef short type;
	typedef VectorBuffer<type> buffer_type;
};

template<> struct StoreType<signals::etypVecLong>
{
	typedef long type;
	typedef VectorBuffer<type> buffer_type;
};

template<> struct StoreType<signals::etypVecInt64>
{
	typedef __int64 type;
	typedef VectorBuffer<type> buffer_type;
};
	
template<> struct StoreType<signals::etypVecSingle>
{
	typedef float type;
	typedef VectorBuffer<type> buffer_type;
};

template<> struct StoreType<signals::etypVecDouble>
{
	typedef double type;
	typedef VectorBuffer<type> buffer_type;
};

template<> struct StoreType<signals::etypVecComplex>
{
	typedef std::complex<float> type;
	typedef VectorBuffer<type> buffer_type;
};

template<> struct StoreType<signals::etypVecCmplDbl>
{
	typedef std::complex<double> type;
	typedef VectorBuffer<type> buffer_type;
};

template<> struct StoreType<signals::etypVecLRSingle>
{
	typedef std::complex<float> type;
	typedef VectorBuffer<type> buffer_type;
};

#pragma warning(push)
#pragma warning(disable: 4355)

template<signals::EType ET>
class CAttribute : public CAttributeBase
{
protected:
	typedef typename StoreType<ET>::type store_type;
	typedef const store_type& param_type;
private:
	typedef CAttribute<ET> my_type;
	CAttribute(const my_type& other);
	my_type& operator=(const my_type& other);
protected:
	inline CAttribute(const char* pName, const char* pDescr, param_type deflt)
		:CAttributeBase(pName, pDescr), m_func(this, &CAttribute<ET>::catcher), m_value(deflt) { }
public:
//	virtual ~CAttribute()				{ }
	virtual signals::EType Type()		{ return ET; }
	virtual const void* getValue()		{ return &m_value; }
	const store_type& nativeGetValue()	{ return m_value; }
//	virtual BOOL isReadOnly();
//	virtual BOOL setValue(const void* newVal) = 0;

protected:
	void privateSetValue(const store_type& newVal)
	{
		if(newVal != m_value)
		{
			m_value = newVal;
			onSetValue(newVal);
		}
	}

	virtual void onSetValue(const store_type& value)
	{
		TObserverList transList;
		{
			ReadLocker obslock(m_observersLock);
			transList = m_observers;
		}
		Locker listlock(m_funcListLock);
		TFuncList::iterator nextFunc = m_funcList.begin();
		for(TObserverList::const_iterator trans = transList.begin(); trans != transList.end(); trans++)
		{
			while(nextFunc != m_funcList.end() && nextFunc->isPending()) nextFunc++;
			if(nextFunc == m_funcList.end())
			{
				m_funcList.push_back(TFuncType(m_func));
				nextFunc--;
			}
			ASSERT(nextFunc != m_funcList.end() && !nextFunc->isPending());
			nextFunc->fire(*trans, value);
		}
	}

private:
	fastdelegate::FastDelegate2<signals::IAttributeObserver*, store_type> m_func;
	typedef AsyncDelegate<signals::IAttributeObserver*, store_type> TFuncType;
	typedef std::list<TFuncType> TFuncList;
	TFuncList  m_funcList;
	Lock       m_funcListLock;
	store_type m_value;

	void catcher(signals::IAttributeObserver* obs, store_type value)
	{
		ReadLocker obslock(m_observersLock);
		if(m_observers.find(obs) != m_observers.end())
		{
			obs->OnChanged(this, &value);
		}
	}
};

template<>
class CAttribute<signals::etypString> : public CAttributeBase
{
protected:
	typedef StoreType<signals::etypString>::type store_type;
	typedef const char* param_type;
protected:
	inline CAttribute(const char* pName, const char* pDescr, param_type deflt)
		:CAttributeBase(pName, pDescr), m_func(this, &CAttribute<signals::etypString>::catcher), m_value(deflt) { }
public:
	virtual ~CAttribute()				{ }
	virtual signals::EType Type()		{ return signals::etypString; }
	virtual const void* getValue()		{ return m_value.c_str(); }
//	virtual BOOL isReadOnly();
//	virtual BOOL setValue(const void* newVal) = 0;

protected:
	virtual void onSetValue(const store_type& value);

	void privateSetValue(const store_type& newVal)
	{
		if(newVal != m_value)
		{
			m_value = newVal;
			onSetValue(newVal);
		}
	}

private:
	fastdelegate::FastDelegate2<signals::IAttributeObserver*, store_type> m_func;
	typedef AsyncDelegate<signals::IAttributeObserver*, store_type> TFuncType;
	typedef std::list<TFuncType> TFuncList;
	TFuncList m_funcList;
	Lock      m_funcListLock;

	void catcher(signals::IAttributeObserver* obs, store_type value)
	{
		ReadLocker obslock(m_observersLock);
		if(m_observers.find(obs) != m_observers.end())
		{
			obs->OnChanged(this, value.c_str());
		}
	}

private:
	typedef CAttribute<signals::etypString> my_type;
	CAttribute(const my_type& other);
	my_type& operator=(const my_type& other);
private:
	std::string m_value;
};

template<signals::EType ET>
class CRWAttribute : public CAttribute<ET>
{
private:
	typedef CRWAttribute<ET> my_type;
	typedef CAttribute<ET> base_type;
	CRWAttribute(const my_type& other);
	my_type& operator=(const my_type& other);
public:
	inline CRWAttribute(const char* pName, const char* pDescr, param_type deflt)
		:base_type(pName, pDescr, deflt) { }

//	virtual ~CRWAttribute()				{ }
	virtual BOOL isReadOnly()			{ return false; }

	virtual BOOL setValue(const void* newVal)
	{
		if(!newVal) { ASSERT(FALSE); return false; }
		return nativeSetValue(*(store_type*)newVal);
	}

	virtual bool nativeSetValue(const store_type& newVal)
	{
		if(!isValidValue(newVal)) return false;
		Locker lock(m_valueLock);
		privateSetValue(newVal);
		return true;
	}

	virtual bool isValidValue(const store_type& newVal) const { UNUSED_ALWAYS(newVal); return true; }

private:
	Lock m_valueLock;
};

template<>
class CRWAttribute<signals::etypString> : public CAttribute<signals::etypString>
{
private:
	typedef CRWAttribute<signals::etypString> my_type;
	typedef CAttribute<signals::etypString> base_type;
	CRWAttribute(const my_type& other);
	my_type& operator=(const my_type& other);
protected:
	inline CRWAttribute(const char* pName, const char* pDescr, param_type deflt)
		:base_type(pName, pDescr, deflt) { }
public:
	virtual ~CRWAttribute()				{ }
	virtual BOOL isReadOnly()			{ return false; }

	virtual BOOL setValue(const void* newVal)
	{
		if(!newVal) { ASSERT(false); return false; }
		return nativeSetValue((const char*)newVal);
	}

	virtual bool nativeSetValue(const std::string& newVal)
	{
		if(!isValidValue(newVal)) return false;
		Locker lock(m_valueLock);
		privateSetValue(newVal);
		return true;
	}

	virtual bool isValidValue(const store_type& newVal) const { UNUSED_ALWAYS(newVal); return true; }

private:
	Lock m_valueLock;
};

class CEventAttribute : public CAttributeBase
{
public:
	inline CEventAttribute(const char* pName, const char* pDescr)
		:CAttributeBase(pName, pDescr), m_func(this, &CEventAttribute::catcher) { }
	virtual ~CEventAttribute()			{ }
	virtual signals::EType Type()		{ return signals::etypNone; }
	virtual BOOL isReadOnly()			{ return false; }
	virtual const void* getValue()		{ return NULL; }
	virtual BOOL setValue(const void* newVal) { UNUSED_ALWAYS(newVal); fire(); return true; }
	void fire();

private:
	CEventAttribute(const CEventAttribute& other);
	CEventAttribute& operator=(const CEventAttribute& other);

	fastdelegate::FastDelegate1<signals::IAttributeObserver*> m_func;
	typedef AsyncDelegate<signals::IAttributeObserver*> TFuncType;
	typedef std::list<TFuncType> TFuncList;
	TFuncList m_funcList;
	Lock      m_funcListLock;

	void catcher(signals::IAttributeObserver* obs)
	{
		ReadLocker obslock(m_observersLock);
		if(m_observers.find(obs) != m_observers.end())
		{
			obs->OnChanged(this, NULL);
		}
	}
};

#pragma warning(pop)

template<signals::EType ET>
class CROAttribute : public CAttribute<ET>
{
private:
	typedef CAttribute<ET> parent_type;
	typedef CROAttribute<ET> my_type;
	CROAttribute(const my_type& other);
	my_type& operator=(const my_type& other);
public:
	inline CROAttribute(const char* pName, const char* pDescr, param_type deflt)
		:parent_type(pName, pDescr, deflt) { }
	virtual BOOL isReadOnly()					{ return true; }
	virtual BOOL setValue(const void* /* newVal */) { return false; }
};

template<signals::EType ET>
class CEPBuffer : public signals::IEPBuffer, protected CRefcountObject
{
protected:
	typedef typename StoreType<ET>::type store_type;
	typedef typename StoreType<ET>::buffer_type buffer_type;

	buffer_type buffer;
	signals::IInEndpoint* m_iep;
	signals::IOutEndpoint* m_oep;

public:
	inline CEPBuffer(typename buffer_type::size_type capacity):buffer(capacity), m_iep(NULL), m_oep(NULL)
	{
	}

public: // IEPBuffer
	virtual signals::EType Type()	{ return ET; }
	virtual unsigned Capacity()		{ return buffer.capacity(); }
	virtual unsigned Used()			{ return buffer.size(); }

public: // IEPSendTo
	virtual unsigned Write(signals::EType type, const void* pBuffer, unsigned numElem, unsigned msTimeout)
	{
		if(type == ET)
		{
			store_type* pBuf = (store_type*)pBuffer;
			unsigned idx = 0;
			while(idx < numElem)
			{
				unsigned numWritten = buffer.push_back_vector(&pBuf[idx], numElem-idx, msTimeout);
				idx += numWritten;
				if(!numWritten) break;
				ASSERT(!buffer_type::is_vector || numWritten == numElem);
			}
			return idx;
		}
		else return 0; // implicit translation not yet supported
	}

	virtual void onSinkConnected(signals::IInEndpoint* src)
	{
		ASSERT(!m_iep);
		signals::IInEndpoint* oldEp(m_iep);
		m_iep = src;
		if(m_iep) m_iep->AddRef();
		if(oldEp) oldEp->Release();
	}

	virtual void onSinkDisconnected(signals::IInEndpoint* src)
	{
		ASSERT(m_iep == src);
		if(m_iep == src)
		{
			m_iep = NULL;
			if(src) src->Release();
		}
	}

	virtual signals::IAttributes* InputAttributes()
	{
		return m_oep ? m_iep->Attributes() : NULL;
	}

public: // IEPRecvFrom
	virtual unsigned Read(signals::EType type, void* pBuffer, unsigned numAvail, BOOL bFillAll, unsigned msTimeout)
	{
		if(type == ET)
		{
			store_type* pBuf = (store_type*)pBuffer;
			unsigned idx = 0;
			while(idx < numAvail)
			{
				unsigned numRead = buffer.pop_front_vector(&pBuf[idx], numAvail-idx, msTimeout);
				idx += numRead;
				if(buffer_type::is_vector || !numRead || !bFillAll) break;
			}
			return idx;
		}
		else return 0; // implicit translation not yet supported
	}

	virtual signals::IAttributes* OutputAttributes()
	{
		return m_iep ? m_oep->Attributes() : NULL;
	}

//	virtual unsigned AddRef()							{ return CRefcountObject::AddRef(); }
//	virtual unsigned Release()							{ return CRefcountObject::Release(); }
	virtual unsigned AddRef(signals::IOutEndpoint* oep)
	{
		if(oep) m_oep = oep;
		return CRefcountObject::AddRef();
	}

	virtual unsigned Release(signals::IOutEndpoint* oep)
	{
		if(oep && oep == m_oep) m_oep = NULL;
		return CRefcountObject::Release();
	}
};


template<signals::EType ET, class CBASE>
class CAttr_callback : public CRWAttribute<ET>
{
private:
	typedef CRWAttribute<ET> base;
public:
	typedef void (CBASE::*TCallback)(const store_type& newVal);
public:
	inline CAttr_callback(CBASE& parent, const char* name, const char* descr, TCallback cb, store_type deflt)
		:base(name, descr, deflt), m_parent(parent), m_callback(cb)
	{
		(m_parent.*m_callback)(this->nativeGetValue());
	}

protected:
	CBASE& m_parent;
	TCallback m_callback;

protected:
	virtual void onSetValue(const store_type& newVal)
	{
		(m_parent.*m_callback)(newVal);
		base::onSetValue(newVal);
	}
};

// ----------------------------------------------------------------------------- inline implementations

unsigned CRefcountObject::Release()
{
	unsigned newref = _InterlockedDecrement(&m_refCount);
	if(!newref) delete this;
	return newref;
}

// using a template for type safety, hopefully this collapses down to the same implemenation...
template<class ATTR> ATTR* CAttributesBase::addLocalAttr(bool bVisible, ATTR* attr)
{
	if(attr)
	{
		const char* name = attr->Name();
		m_ownedAttrs.insert(attr);
		m_attributes.insert(TVoidMapToAttr::value_type(name, attr));
		m_attrNames.insert(TStringMapToVoid::value_type(name, name));
		if(bVisible) m_visibleAttrs.insert(attr);
	}
	return attr;
}

template<class ATTR> ATTR* CAttributesBase::addRemoteAttr(const char* name, ATTR* attr)
{
	if(attr)
	{
		m_attributes.insert(TVoidMapToAttr::value_type(name, attr));
		m_attrNames.insert(TStringMapToVoid::value_type(name, name));
		m_visibleAttrs.insert(attr);
	}
	return attr;
}
