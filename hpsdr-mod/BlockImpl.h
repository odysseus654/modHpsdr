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
public:
	inline CRefcountObject():m_refCount(0) {}
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
public:
	inline CInEndpointBase():m_connRecv(NULL) {}
	unsigned Read(signals::EType type, void* buffer, unsigned numAvail, unsigned msTimeout);

	virtual bool Connect(signals::IEPReceiver* recv);
	virtual bool isConnected() { return !!m_connRecv; }
	virtual bool Disconnect()  { return Connect(NULL); }
//	virtual const char* EPName();
//	virtual unsigned AddRef() = 0;
//	virtual unsigned Release() = 0;
//	virtual signals::IBlock* Block() = 0;
//	virtual signals::EType Type() = 0;
//	virtual signals::IAttributes* Attributes() = 0;
//	virtual signals::IEPBuffer* CreateBuffer() = 0;
private:
	CInEndpointBase(const CInEndpointBase& other);
	CInEndpointBase& operator=(const CInEndpointBase& other);
private:
	signals::IEPReceiver* m_connRecv;
};

class COutEndpointBase : public signals::IOutEndpoint
{
public:
	inline COutEndpointBase():m_connSend(NULL) {}
	unsigned Write(signals::EType type, void* buffer, unsigned numElem, unsigned msTimeout);

	virtual bool Connect(signals::IEPSender* send);
	virtual bool isConnected() { return !!m_connSend; }
	virtual bool Disconnect()  { return Connect(NULL); }
//	virtual const char* EPName();
//	virtual signals::IBlock* Block() = 0;
//	virtual signals::EType Type() = 0;
//	virtual signals::IAttributes* Attributes() = 0;
//	virtual signals::IEPBuffer* CreateBuffer() = 0;
private:
	COutEndpointBase(const COutEndpointBase& other);
	COutEndpointBase& operator=(const COutEndpointBase& other);
private:
	signals::IEPSender* m_connSend;
};

class CAttributeBase : public signals::IAttribute
{
public:
	inline CAttributeBase(const char* pName, const char* pDescr):m_name(pName),m_descr(pDescr) { }
	virtual ~CAttributeBase()			{ }
	virtual const char* Name()			{ return m_name; }
	virtual const char* Description()	{ return m_descr; }
	virtual void Observe(signals::IAttributeObserver* obs);
	virtual void Unobserve(signals::IAttributeObserver* obs);
	virtual unsigned options(const void* /* vals */, const char** /* opts */, unsigned /* availElem */) { return 0; }
//	virtual signals::EType Type() = 0;
//	virtual bool isReadOnly();
//	virtual const void* getValue() = 0;
//	virtual bool setValue(const void* newVal) = 0;
private:
	CAttributeBase(const CAttributeBase& other);
	CAttributeBase& operator=(const CAttributeBase& other);
protected:
	typedef std::set<signals::IAttributeObserver*> TObserverList;
	TObserverList m_observers;
	Lock          m_observersLock;
private:
	const char* m_name;
	const char* m_descr;
};

class CAttributesBase : public signals::IAttributes
{
public:
	inline CAttributesBase() {}
	~CAttributesBase();

	virtual unsigned Itemize(signals::IAttribute** attrs, unsigned availElem);
	virtual signals::IAttribute* GetByName(const char* name)			{ return GetByName2(name); }
	virtual void Observe(signals::IAttributeObserver* obs);
	virtual void Unobserve(signals::IAttributeObserver* obs);
//	virtual signals::IBlock* Block() = 0;

private:
	CAttributesBase(const CAttributesBase& other);
	CAttributesBase& operator=(const CAttributesBase& other);

protected: // using a "fake generic" for typesafety
	template<class ATTR> ATTR* addRemoteAttr(const char* pName, ATTR* attr);
	template<class ATTR> ATTR* addLocalAttr(bool bVisible, ATTR* attr);
//	CAttributeBase* buildAttr(const char* name, signals::EType type, const char* descr, bool bReadOnly, bool bVisible);
	CAttributeBase* GetByName2(const char* name);

private:
	typedef std::map<const void*,CAttributeBase*> TVoidMapToAttr;
	typedef std::map<std::string,const void*> TStringMapToVoid;
	typedef std::set<CAttributeBase*> TAttrSet;
	TVoidMapToAttr   m_attributes;
	TStringMapToVoid m_attrNames;
	TAttrSet         m_ownedAttrs;
	TAttrSet         m_visibleAttrs;
};

template<signals::EType ET> struct StoreType;
template<> struct StoreType<signals::etypEvent>		{ typedef void type; };
template<> struct StoreType<signals::etypBoolean>	{ typedef unsigned char type; };
template<> struct StoreType<signals::etypByte>		{ typedef unsigned char type; };
template<> struct StoreType<signals::etypShort>		{ typedef short type; };
template<> struct StoreType<signals::etypLong>		{ typedef long type; };
template<> struct StoreType<signals::etypSingle>	{ typedef float type; };
template<> struct StoreType<signals::etypDouble>	{ typedef double type; };
template<> struct StoreType<signals::etypComplex>	{ typedef std::complex<float> type; };
template<> struct StoreType<signals::etypString>	{ typedef std::string type; };
template<> struct StoreType<signals::etypLRSingle>	{ typedef std::complex<float> type; };

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
	virtual ~CAttribute()				{ }
	virtual signals::EType Type()		{ return ET; }
	virtual const void* getValue()		{ return &m_value; }
	const store_type& nativeGetValue()	{ return m_value; }
//	virtual bool isReadOnly();
//	virtual bool setValue(const void* newVal) = 0;

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
			Locker obslock(m_observersLock);
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
		obs->OnChanged(Name(), Type(), &value);
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
//	virtual bool isReadOnly();
//	virtual bool setValue(const void* newVal) = 0;

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
		obs->OnChanged(Name(), Type(), value.c_str());
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
protected:
	inline CRWAttribute(const char* pName, const char* pDescr, param_type deflt)
		:base_type(pName, pDescr, deflt) { }
public:
	virtual ~CRWAttribute()				{ }
	virtual bool isReadOnly()			{ return false; }

	virtual bool setValue(const void* newVal)
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
	virtual bool isReadOnly()			{ return false; }

	virtual bool setValue(const void* newVal)
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
	virtual bool isReadOnly()			{ return false; }
	virtual const void* getValue()		{ return NULL; }
	virtual bool setValue(const void* newVal) { UNUSED_ALWAYS(newVal); fire(); return true; }
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
		obs->OnChanged(Name(), Type(), NULL);
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
	virtual bool isReadOnly()					{ return true; }
	virtual bool setValue(const void* /* newVal */) { return false; }
};

template<signals::EType ET>
class CEPBuffer : public signals::IEPBuffer, protected CRefcountObject
{
protected:
	typedef typename StoreType<ET>::type store_type;
	typedef Buffer<store_type> buffer_type;

	buffer_type buffer;
	signals::IInEndpoint* m_iep;

public:
	inline CEPBuffer(typename buffer_type::size_type capacity):buffer(capacity), m_iep(NULL)
	{
	}

public: // IEPBuffer
	virtual signals::EType Type()	{ return ET; }
	virtual unsigned Capacity()		{ return buffer.capacity(); }
	virtual unsigned Used()			{ return buffer.size(); }

public: // IEPSender
	virtual unsigned Write(signals::EType type, const void* pBuffer, unsigned numElem, unsigned msTimeout)
	{
		if(type == ET)
		{
			store_type* pBuf = (store_type*)pBuffer;
			unsigned idx = 0;
			for(; idx < numElem; idx++)
			{
				if(!buffer.push_back(pBuf[idx], msTimeout)) break;
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

public: // IEPReceiver
	virtual unsigned Read(signals::EType type, void* pBuffer, unsigned numAvail, unsigned msTimeout)
	{
		if(type == ET)
		{
			store_type* pBuf = (store_type*)pBuffer;
			unsigned idx = 0;
			for(; idx < numAvail; idx++)
			{
				if(!buffer.pop_front(pBuf[idx], msTimeout)) break;
			}
			return idx;
		}
		else return 0; // implicit translation not yet supported
	}

	virtual unsigned AddRef()		{ return CRefcountObject::AddRef(); }
	virtual unsigned Release()		{ return CRefcountObject::Release(); }
};

// ------------------------------------------------------------------------------------------------

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
