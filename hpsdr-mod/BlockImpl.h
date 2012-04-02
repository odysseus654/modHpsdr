#pragma once
#include "block.h"

#include <complex>
#include <map>
#include <set>
#include <string>

class CRefcountObject
{
public:
	inline CRefcountObject():m_refCount(0) {}
	virtual ~CRefcountObject() {}
	virtual unsigned AddRef();
	virtual unsigned Release();

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
//	virtual signals::EType Type() = 0;
//	virtual bool isReadOnly();
//	virtual const void* getValue() = 0;
//	virtual bool setValue(const void* newVal) = 0;
//	virtual void internalSetValue(const void* newVal) = 0;
protected:
	virtual void onSetValue(const void* value);
private:
	typedef std::set<signals::IAttributeObserver*> TObserverList;
	TObserverList m_observers;
	const char* m_name;
	const char* m_descr;
};

class CAttributesBase : public signals::IAttributes
{
public:
	~CAttributesBase();

	virtual unsigned numAttributes()									{ return m_visibleAttrs.size(); }
	virtual unsigned Itemize(signals::IAttribute** attrs, unsigned availElem);
	virtual signals::IAttribute* GetByName(const char* name)			{ return GetByName2(name); }
	virtual void Observe(signals::IAttributeObserver* obs);
	virtual void Unobserve(signals::IAttributeObserver* obs);
//	virtual signals::IBlock* Block() = 0;

protected:
	/*struct TAttrDef
	{
		const char* name;
		signals::EType type;
		const char* descr;
		bool bReadOnly;
		bool bVisible;
		bool bSearchParent;

		// handled by parent
		inline TAttrDef(const char* pName, bool pVisible)
			:name(pName),type(signals::etypNone),descr(NULL),bReadOnly(true),bVisible(pVisible),bSearchParent(true)
		{}

		// handled by self
		inline TAttrDef(const char* pName, signals::EType pType, const char* pDescr, bool pReadOnly, bool pVisible)
			:name(pName),type(pType),descr(pDescr),bReadOnly(pReadOnly),bVisible(pVisible),bSearchParent(false)
		{}
	};
	*/
//	void buildAttrs(CAttributesBase* parent, TAttrDef* attrs, unsigned numAttrs);
	CAttributeBase* buildAttr(const char* pName, CAttributeBase* attr);
	CAttributeBase* buildAttr(const char* name, signals::EType type, const char* descr, bool bReadOnly, bool bVisible);
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
template<> struct StoreType<signals::etypBoolean>	{ typedef unsigned char type; };
template<> struct StoreType<signals::etypByte>		{ typedef unsigned char type; };
template<> struct StoreType<signals::etypShort>		{ typedef short type; };
template<> struct StoreType<signals::etypLong>		{ typedef long type; };
template<> struct StoreType<signals::etypSingle>	{ typedef float type; };
template<> struct StoreType<signals::etypDouble>	{ typedef double type; };
template<> struct StoreType<signals::etypComplex>	{ typedef std::complex<float> type; };
template<> struct StoreType<signals::etypString>	{ typedef std::string type; };

template<signals::EType ET>
class CAttribute : public CAttributeBase
{
protected:
	typedef typename StoreType<ET>::type T;
public:
	inline CAttribute(const char* pName, const char* pDescr, T deflt):CAttributeBase(pName, pDescr),m_value(deflt) { }
	virtual ~CAttribute()				{ }
	virtual signals::EType Type()		{ return ET; }
	virtual bool isReadOnly()			{ return false; }
	virtual const void* getValue()		{ return &m_value; }
	const T& nativeGetValue()			{ return m_value; }
	virtual void nativeOnSetValue(const T& value) { onSetValue(&value); }

	virtual bool setValue(const void* newVal)
	{
		if(!newVal) { ASSERT(FALSE); return false; }
		return nativeSetValue(*(T*)newVal);
	}

	virtual bool nativeSetValue(const T& newVal)
	{
		if(newVal != m_value)
		{
			m_value = newVal;
			nativeOnSetValue(newVal);
		}
		return true;
	}

private:
	T m_value;
};

template<>
class CAttribute<signals::etypString> : public CAttributeBase
{
public:
	inline CAttribute(const char* pName, const char* pDescr, const char *deflt):CAttributeBase(pName, pDescr),m_value(deflt) { }
	virtual ~CAttribute()				{ }
	virtual signals::EType Type()		{ return signals::etypString; }
	virtual bool isReadOnly()			{ return false; }
	virtual const void* getValue()		{ return m_value.c_str(); }
	virtual void nativeOnSetValue(const std::string& value) { onSetValue(&value); }

	virtual bool setValue(const void* newVal)
	{
		if(!newVal) { ASSERT(FALSE); return false; }
		return nativeSetValue((const char*)newVal);
	}

	virtual bool nativeSetValue(const std::string& newVal)
	{
		if(strcmp(newVal.c_str(), m_value.c_str()) != 0)
		{
			m_value = newVal;
			nativeOnSetValue(newVal);
		}
		return true;
	}

private:
	std::string m_value;
};

template<signals::EType ET>
class CROAttribute : public CAttribute<ET>
{
public:
	inline CROAttribute(const char* pName, const char* pDescr, T deflt):CAttribute<ET>(pName, pDescr, deflt) { }
	virtual bool isReadOnly()					{ return true; }
	virtual bool setValue(const void* newVal)	{ return false; }
	virtual bool nativeSetValue(const T& newVal) { return false; }
};

template<>
class CROAttribute<signals::etypString> : public CAttribute<signals::etypString>
{
public:
	inline CROAttribute(const char* pName, const char* pDescr, const char* deflt)
		:CAttribute<signals::etypString>(pName, pDescr, deflt) { }
	virtual bool isReadOnly()					{ return true; }
	virtual bool setValue(const void* newVal)	{ return false; }
	virtual bool nativeSetValue(const std::string& newVal) { return false; }
};
