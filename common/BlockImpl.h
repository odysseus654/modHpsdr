/*
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
	BOOL ReadOne(signals::EType type, void* buffer, unsigned msTimeout);

	virtual BOOL Connect(signals::IEPRecvFrom* recv);
	virtual BOOL isConnected() { return !!m_connRecv; }
	virtual BOOL Disconnect()  { return Connect(NULL); }
//	virtual const char* EPName();
//	virtual unsigned AddRef() = 0;
//	virtual unsigned Release() = 0;
//	virtual signals::EType Type() = 0;
//	virtual signals::IAttributes* Attributes() = 0;
	signals::IAttributes* RemoteAttributes();

protected:
	virtual void OnConnection(signals::IEPRecvFrom*) { }
	inline signals::IEPRecvFrom* CurrentEndpoint() const { return m_connRecv; }
private:
	CInEndpointBase(const CInEndpointBase& other);
	CInEndpointBase& operator=(const CInEndpointBase& other);
protected:
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
	BOOL WriteOne(signals::EType type, void* buffer, unsigned msTimeout);

	virtual BOOL Connect(signals::IEPSendTo* send);
	virtual BOOL isConnected() { return !!m_connSend; }
	virtual BOOL Disconnect()  { return Connect(NULL); }
//	virtual const char* EPName();
//	virtual signals::EType Type() = 0;
//	virtual signals::IAttributes* Attributes() = 0;
//	virtual signals::IEPBuffer* CreateBuffer() = 0;
	signals::IAttributes* RemoteAttributes();
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
//	virtual BOOL isReadOnly() const;
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

template<typename RemoteEndpoint>
class CCascadedAttributesBase : public CAttributesBase
{
protected:
	inline CCascadedAttributesBase(RemoteEndpoint& src):m_src(src) {}
public: // CAttributesBase implementaton
	virtual unsigned Itemize(signals::IAttribute** attrs, unsigned availElem, unsigned flags);
	virtual signals::IAttribute* GetByName(const char* name);
	signals::IAttribute* RemoteGetByName(const char* name);
private:
	CCascadedAttributesBase(const CCascadedAttributesBase<RemoteEndpoint>& other);
	CCascadedAttributesBase& operator=(const CCascadedAttributesBase<RemoteEndpoint>& other);
private:
	RemoteEndpoint& m_src;
};

class CSimpleIncomingChild : public CInEndpointBase, public CAttributesBase
{	// This class is assumed to be a static (non-dynamic) member of its parent
protected:
	inline CSimpleIncomingChild(signals::EType type, signals::IBlock* parent):m_type(type),m_parent(parent) { }
public:
	virtual ~CSimpleIncomingChild() {}

protected:
	const signals::EType m_type;
	signals::IBlock* m_parent;

private:
	CSimpleIncomingChild(const CSimpleIncomingChild& other);
	CSimpleIncomingChild& operator=(const CSimpleIncomingChild& other);

public: // CInEndpointBase interface
//	virtual const char* EPName()				{ return EP_NAME; }
//	virtual const char* EPDescr()				{ return EP_DESCR; }
	virtual unsigned AddRef()					{ return m_parent->AddRef(); }
	virtual unsigned Release()					{ return m_parent->Release(); }
	virtual signals::EType Type()				{ return m_type; }
	virtual signals::IAttributes* Attributes()	{ return this; }
};

class CSimpleCascadeIncomingChild : public CInEndpointBase, public CCascadedAttributesBase<COutEndpointBase>
{	// This class is assumed to be a static (non-dynamic) member of its parent
protected:
	inline CSimpleCascadeIncomingChild(signals::EType type, signals::IBlock* parent, COutEndpointBase& src)
		:CCascadedAttributesBase(src),m_type(type),m_parent(parent) { }
public:
	virtual ~CSimpleCascadeIncomingChild() {}

protected:
	const signals::EType m_type;
	signals::IBlock* m_parent;

private:
	CSimpleCascadeIncomingChild(const CSimpleCascadeIncomingChild& other);
	CSimpleCascadeIncomingChild& operator=(const CSimpleCascadeIncomingChild& other);

public: // CInEndpointBase interface
//	virtual const char* EPName()				{ return EP_NAME; }
//	virtual const char* EPDescr()				{ return EP_DESCR; }
	virtual unsigned AddRef()					{ return m_parent->AddRef(); }
	virtual unsigned Release()					{ return m_parent->Release(); }
	virtual signals::EType Type()				{ return m_type; }
	virtual signals::IAttributes* Attributes()	{ return this; }
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
class CSimpleCascadeOutgoingChild : public COutEndpointBase, public CCascadedAttributesBase<CInEndpointBase>
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
template<signals::EType ET> class Vector;
template<> struct StoreType<signals::etypEvent>
{
	typedef void type;
};

template<> struct StoreType<signals::etypBoolean>
{
	typedef unsigned char type;
	typedef Buffer<type> buffer_type;
	enum { is_blittable = 1, is_vector = 0 };
};

template<> struct StoreType<signals::etypWinHdl>
{
	typedef HANDLE type;
	typedef Buffer<type> buffer_type;
	enum { is_blittable = 1, is_vector = 0 };
};

template<> struct StoreType<signals::etypByte>
{
	typedef unsigned char type;
	typedef Buffer<type> buffer_type;
	enum { is_blittable = 1, is_vector = 0 };
};

template<> struct StoreType<signals::etypShort>
{
	typedef short type;
	typedef Buffer<type> buffer_type;
	enum { is_blittable = 1, is_vector = 0 };
};

template<> struct StoreType<signals::etypLong>
{
	typedef long type;
	typedef Buffer<type> buffer_type;
	enum { is_blittable = 1, is_vector = 0 };
};

template<> struct StoreType<signals::etypInt64>
{
	typedef __int64 type;
	typedef Buffer<type> buffer_type;
	enum { is_blittable = 1, is_vector = 0 };
};

template<> struct StoreType<signals::etypSingle>
{
	typedef float type;
	typedef Buffer<type> buffer_type;
	enum { is_blittable = 1, is_vector = 0 };
};

template<> struct StoreType<signals::etypDouble>
{
	typedef double type;
	typedef Buffer<type> buffer_type;
	enum { is_blittable = 1, is_vector = 0 };
};

template<> struct StoreType<signals::etypComplex>
{
	typedef std::complex<float> type;
	typedef Buffer<type> buffer_type;
	enum { is_blittable = 0, is_vector = 0 };
};

template<> struct StoreType<signals::etypCmplDbl>
{
	typedef std::complex<double> type;
	typedef Buffer<type> buffer_type;
	enum { is_blittable = 0, is_vector = 0 };
};

template<> struct StoreType<signals::etypString>
{
	typedef std::string type;
	enum { is_blittable = 0, is_vector = 0 };
};

template<> struct StoreType<signals::etypLRSingle>
{
	typedef std::complex<float> type;
	typedef Buffer<type> buffer_type;
	enum { is_blittable = 0, is_vector = 0 };
};

template<> struct StoreType<signals::etypVecBoolean>
{
	typedef signals::IVector* type;
	typedef unsigned char base_type;
	typedef Vector<signals::etypBoolean> buffer_templ;
	typedef Buffer<type> buffer_type;
	enum { is_vector = 1, base_enum = signals::etypBoolean };
};

template<> struct StoreType<signals::etypVecByte>
{
	typedef signals::IVector* type;
	typedef unsigned char base_type;
	typedef Vector<signals::etypByte> buffer_templ;
	typedef Buffer<type> buffer_type;
	enum { is_vector = 1, base_enum = signals::etypByte };
};

template<> struct StoreType<signals::etypVecShort>
{
	typedef signals::IVector* type;
	typedef short base_type;
	typedef Vector<signals::etypShort> buffer_templ;
	typedef Buffer<type> buffer_type;
	enum { is_vector = 1, base_enum = signals::etypShort };
};

template<> struct StoreType<signals::etypVecLong>
{
	typedef signals::IVector* type;
	typedef long base_type;
	typedef Vector<signals::etypLong> buffer_templ;
	typedef Buffer<type> buffer_type;
	enum { is_vector = 1, base_enum = signals::etypLong };
};

template<> struct StoreType<signals::etypVecInt64>
{
	typedef signals::IVector* type;
	typedef __int64 base_type;
	typedef Vector<signals::etypInt64> buffer_templ;
	typedef Buffer<type> buffer_type;
	enum { is_vector = 1, base_enum = signals::etypInt64 };
};
	
template<> struct StoreType<signals::etypVecSingle>
{
	typedef signals::IVector* type;
	typedef float base_type;
	typedef Vector<signals::etypSingle> buffer_templ;
	typedef Buffer<type> buffer_type;
	enum { is_vector = 1, base_enum = signals::etypSingle };
};

template<> struct StoreType<signals::etypVecDouble>
{
	typedef signals::IVector* type;
	typedef double base_type;
	typedef Vector<signals::etypDouble> buffer_templ;
	typedef Buffer<type> buffer_type;
	enum { is_vector = 1, base_enum = signals::etypDouble };
};

template<> struct StoreType<signals::etypVecComplex>
{
	typedef signals::IVector* type;
	typedef std::complex<float> base_type;
	typedef Vector<signals::etypComplex> buffer_templ;
	typedef Buffer<type> buffer_type;
	enum { is_vector = 1, base_enum = signals::etypComplex };
};

template<> struct StoreType<signals::etypVecCmplDbl>
{
	typedef signals::IVector* type;
	typedef std::complex<double> base_type;
	typedef Vector<signals::etypCmplDbl> buffer_templ;
	typedef Buffer<type> buffer_type;
	enum { is_vector = 1, base_enum = signals::etypCmplDbl };
};

template<> struct StoreType<signals::etypVecLRSingle>
{
	typedef signals::IVector* type;
	typedef std::complex<float> base_type;
	typedef Vector<signals::etypLRSingle> buffer_templ;
	typedef Buffer<type> buffer_type;
	enum { is_vector = 1, base_enum = signals::etypLRSingle };
};

template<signals::EType ET>
class VectorPool
{
protected:
	typedef Vector<ET> vector_type;
	typedef std::list<vector_type*> pool_type;
	pool_type m_pool;
	Lock m_poolLock;
	LONG m_liveCount;
	enum { MAX_POOL_SIZE = 15 };
public:
	inline VectorPool():m_liveCount(0){}
	vector_type* retrieve(unsigned size);
	void release(vector_type* vec);
};

#pragma warning(push)
#pragma warning(disable: 4200)

template<signals::EType ET>
class Vector : public signals::IVector
{
public:
	typedef typename StoreType<ET>::type EntryType;

	virtual signals::EType Type()	{ ASSERT(m_refCount > 0); return ET; }
	virtual unsigned Size()			{ ASSERT(m_refCount > 0); return size; }
	virtual unsigned AddRef()		{ return _InterlockedIncrement(&m_refCount); }
	virtual unsigned Release();
	virtual const void* Data()		{ ASSERT(m_refCount > 0); return data; }

protected:
	inline Vector(unsigned s):size(s),m_refCount(0){}
	static VectorPool<ET> gl_pool;
private:
	Vector(const Vector&);
	Vector& operator=(const Vector&);
	~Vector() {}

	volatile long m_refCount;

public:
	static Vector* retrieve(unsigned size) { return gl_pool.retrieve(size); }
	static Vector* construct(unsigned size);
	void destruct();

	const unsigned size;
	EntryType data[0];
};
#pragma warning(pop)

template<signals::EType ET> VectorPool<ET> Vector<ET>::gl_pool;

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
//	virtual BOOL isReadOnly() const;
//	virtual BOOL setValue(const void* newVal) = 0;

protected:
	bool privateSetValue(const store_type& newVal)
	{
		if(newVal == m_value) return false;
		m_value = newVal;
		onSetValue(newVal);
		return true;
	}

	bool privateSetValue(const store_type& newVal, Locker& lock, bool bDoNotify = true)
	{
		if(newVal == m_value) return false;
		m_value = newVal;
		lock.unlock();
		if(bDoNotify) onSetValue(newVal);
		return true;
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
//	virtual BOOL isReadOnly() const;
//	virtual BOOL setValue(const void* newVal) = 0;

protected:
	virtual void onSetValue(const store_type& value);

	bool privateSetValue(const store_type& newVal)
	{
		if(newVal == m_value) return false;
		m_value = newVal;
		onSetValue(newVal);
		return true;
	}

	bool privateSetValue(const store_type& newVal, Locker& lock, bool bDoNotify = true)
	{
		if(newVal == m_value) return false;
		m_value = newVal;
		lock.unlock();
		if(bDoNotify) onSetValue(newVal);
		return true;
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
	virtual BOOL isReadOnly() const		{ return false; }

	virtual BOOL setValue(const void* newVal)
	{
		if(!newVal) { ASSERT(FALSE); return false; }
		return nativeSetValue(*(store_type*)newVal);
	}

	bool nativeSetValue(const store_type& newVal)
	{
		if(!isValidValue(newVal)) return false;
		Locker lock(m_valueLock);
		privateSetValue(newVal, lock);
		return true;
	}

	bool nativeSimpleSetValue(const store_type& newVal)
	{
		if(!isValidValue(newVal)) return false;
		Locker lock(m_valueLock);
		return privateSetValue(newVal, lock, false);
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
	virtual BOOL isReadOnly() const		{ return false; }

	virtual BOOL setValue(const void* newVal)
	{
		if(!newVal) { ASSERT(false); return false; }
		return nativeSetValue((const char*)newVal);
	}

	bool nativeSetValue(const std::string& newVal)
	{
		if(!isValidValue(newVal)) return false;
		Locker lock(m_valueLock);
		privateSetValue(newVal, lock);
		return true;
	}

	bool nativeSimpleSetValue(const std::string& newVal)
	{
		if(!isValidValue(newVal)) return false;
		Locker lock(m_valueLock);
		return privateSetValue(newVal, lock, false);
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
	virtual signals::EType Type()		{ return signals::etypEvent; }
	virtual BOOL isReadOnly() const		{ return false; }
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
	virtual BOOL isReadOnly() const					{ return true; }
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
				ASSERT(!StoreType<ET>::is_vector || numWritten == numElem);
			}
			return idx;
		}
		else return 0; // implicit translation not yet supported
	}

	virtual BOOL WriteOne(signals::EType type, const void* pBuffer, unsigned msTimeout)
	{
		if(type != ET) return FALSE; // implicit translation not yet supported
		return buffer.push_back(*(store_type*)pBuffer, msTimeout);
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
		return m_iep ? m_iep->Attributes() : NULL;
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
				if(StoreType<ET>::is_vector || !numRead || !bFillAll) break;
			}
			return idx;
		}
		else return 0; // implicit translation not yet supported
	}

	virtual BOOL ReadOne(signals::EType type, void* pBuffer, unsigned msTimeout)
	{
		if(type != ET) return FALSE; // implicit translation not yet supported
		return buffer.pop_front(*(store_type*)pBuffer, msTimeout);
	}

	virtual signals::IEPBuffer* CreateBuffer()
	{
		signals::IEPBuffer* buff = new CEPBuffer<ET>(buffer.capacity());
		buff->AddRef(NULL);
		return buff;
	}

	virtual signals::IAttributes* OutputAttributes()
	{
		return m_oep ? m_oep->Attributes() : NULL;
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

template<signals::EType ET>
typename VectorPool<ET>::vector_type* VectorPool<ET>::retrieve(unsigned size)
{
	{
		Locker lock(m_poolLock);
		pool_type::iterator trans = m_pool.end();
		while(trans != m_pool.begin())
		{
			vector_type* result = *--trans;
			if(result->size == size)
			{
				m_pool.erase(trans);
				result->AddRef();
				return result;
			}
		}
	}

	vector_type* result = vector_type::construct(size);
	VERIFY(++m_liveCount < 100);
	result->AddRef();
	return result;
}

template<signals::EType ET>
void VectorPool<ET>::release(vector_type* vec)
{
	Locker lock(m_poolLock);
	m_pool.push_back(vec);
	while(m_pool.size() > MAX_POOL_SIZE)
	{
		vector_type* first = m_pool.front();
		m_pool.pop_front();
		first->destruct();
		m_liveCount--;
	}
}

template<signals::EType ET>
Vector<ET>* Vector<ET>::construct(unsigned size)
{
	Vector* result = (Vector*)malloc(sizeof(Vector) + size * sizeof(EntryType));
	new(result) Vector(size);
	return result;
}

template<signals::EType ET>
void Vector<ET>::destruct()
{
	this->~Vector();
	free(this);
}

template<signals::EType ET>
unsigned Vector<ET>::Release()
{
	unsigned newref = _InterlockedDecrement(&m_refCount);
	if(!newref) gl_pool.release(this);
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

// ------------------------------------------------------------------------------------------------ CascadeAttributeBase

template<typename RemoteEndpoint>
unsigned CCascadedAttributesBase<RemoteEndpoint>::Itemize(signals::IAttribute** attrs, unsigned availElem, unsigned flags)
{
	if(flags & signals::flgLocalOnly)
	{
		return CAttributesBase::Itemize(attrs, availElem, flags);
	}

	typedef std::map<std::string,signals::IAttribute*> TStringMapToAttr;
	TStringMapToAttr foundAttrs;

	if(flags & signals::flgIncludeHidden)
	{
		for(TVoidMapToAttr::const_iterator trans=m_attributes.begin(); trans != m_attributes.end(); trans++)
		{
			CAttributeBase* attr = trans->second;
			foundAttrs.insert(TStringMapToAttr::value_type(attr->Name(), attr));
		}
	}
	else
	{
		for(TAttrSet::const_iterator trans=m_visibleAttrs.begin(); trans != m_visibleAttrs.end(); trans++)
		{
			CAttributeBase* attr = *trans;
			foundAttrs.insert(TStringMapToAttr::value_type(attr->Name(), attr));
		}
	}

	signals::IAttributes* rattrs = m_src.RemoteAttributes();
	if(rattrs)
	{
		unsigned count = rattrs->Itemize(NULL, 0, flags);
		signals::IAttribute** attrList = new signals::IAttribute*[count];
		rattrs->Itemize(attrList, count, flags);
		for(unsigned idx=0; idx < count; idx++)
		{
			signals::IAttribute* attr = attrList[idx];
			std::string name = attr->Name();
			if(foundAttrs.find(name) == foundAttrs.end())
			{
				foundAttrs.insert(TStringMapToAttr::value_type(name, attr));
			}
		}
		delete [] attrList;
	}

	if(attrs && availElem)
	{
		unsigned i;
		TStringMapToAttr::const_iterator trans;
		for(i=0, trans=foundAttrs.begin(); i < availElem && trans != foundAttrs.end(); i++, trans++)
		{
			signals::IAttribute* attr = trans->second;
			attrs[i] = attr;
		}
	}
	return foundAttrs.size();
}

template<typename RemoteEndpoint>
signals::IAttribute* CCascadedAttributesBase<RemoteEndpoint>::GetByName(const char* name)
{
	signals::IAttribute* attr = CAttributesBase::GetByName(name);
	if(attr) return attr;
	signals::IAttributes* attrs = m_src.RemoteAttributes();
	if(!attrs) return NULL;
	return attrs->GetByName(name);
}

template<typename RemoteEndpoint>
signals::IAttribute* CCascadedAttributesBase<RemoteEndpoint>::RemoteGetByName(const char* name)
{
	signals::IAttributes* attrs = m_src.RemoteAttributes();
	if(!attrs) return NULL;
	return attrs->GetByName(name);
}

