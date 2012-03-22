#include "stdafx.h"
#include "BlockImpl.h"

#include <complex>
#include "windows.h"

// ------------------------------------------------------------------ class CRefcountObject

unsigned CRefcountObject::AddRef()
{
	return InterlockedIncrement(&m_refCount);
}

unsigned CRefcountObject::Release()
{
	unsigned newref = InterlockedDecrement(&m_refCount);
	if(!newref) delete this;
	return newref;
}

// ------------------------------------------------------------------ class CInEndpointBase

bool CInEndpointBase::Connect(signals::IEPReceiver* recv)
{
	if(recv != m_connRecv)
	{
		if(recv) recv->AddRef();
		if(m_connRecv) m_connRecv->Release();
		m_connRecv = recv;
	}
	return true;
}

unsigned CInEndpointBase::Read(signals::EType type, void* buffer, unsigned numAvail, unsigned msTimeout)
{
	return m_connRecv ? m_connRecv->Read(type, buffer, numAvail, msTimeout) : 0;
}

// ------------------------------------------------------------------ class COutEndpointBase

bool COutEndpointBase::Connect(signals::IEPSender* send)
{
	if(send != m_connSend)
	{
		if(send) send->onSourceConnected(this);
		if(m_connSend) m_connSend->onSourceDisconnected(this);
		m_connSend = send;
	}
	return true;
}

unsigned COutEndpointBase::Write(signals::EType type, void* buffer, unsigned numElem, unsigned msTimeout)
{
	return m_connSend ? m_connSend->Write(type, buffer, numElem, msTimeout) : 0;
}

// ------------------------------------------------------------------ class CAttributeBase

void CAttributeBase::Observe(signals::IAttributeObserver* obs)
{
	if(m_observers.find(obs) != m_observers.end())
	{
		m_observers.insert(obs);
	}
}

void CAttributeBase::Unobserve(signals::IAttributeObserver* obs)
{
	TObserverList::iterator lookup = m_observers.find(obs);
	if(lookup != m_observers.end())
	{
		m_observers.erase(lookup);
	}
}

void CAttributeBase::onSetValue(const void* value)
{
	TObserverList transList(m_observers);
	for(TObserverList::const_iterator trans = transList.begin(); trans != transList.end(); trans++)
	{
		(*trans)->OnChanged(m_name, Type(), value);
	}
}

// ------------------------------------------------------------------ class IAttributesBase

CAttributesBase::~CAttributesBase()
{
	for(TAttrSet::const_iterator trans = m_ownedAttrs.begin(); trans != m_ownedAttrs.end(); trans++)
	{
		delete *trans;
	}
	m_ownedAttrs.clear();
	m_attributes.clear();
	m_attrNames.clear();
	m_ownedAttrs.clear();
}

CAttributeBase* CAttributesBase::GetByName2(const char* name)
{
	// try a fast lookup first
	TVoidMapToAttr::const_iterator lookup1 = m_attributes.find((void*)name);
	if(lookup1 != m_attributes.end()) return lookup1->second;

	// drop down to a slower string search
	TStringMapToVoid::const_iterator lookup2 = m_attrNames.find(name);
	if(lookup2 != m_attrNames.end())
	{
		lookup1 = m_attributes.find(lookup2->second);
		if(lookup1 != m_attributes.end()) return lookup1->second;
	}

	return NULL;
}

void CAttributesBase::buildAttrs(CAttributesBase* parent, CAttributesBase::TAttrDef* attrs, unsigned numAttrs)
{
	for(unsigned idx=0; idx < numAttrs; idx++)
	{
		const TAttrDef& attrDef = attrs[idx];

		CAttributeBase* attr = NULL;
		if(attrDef.bSearchParent)
		{
			attr = parent->GetByName2(attrDef.name);
		}
		else
		{
			switch(attrDef.type)
			{
			case signals::etypBoolean:
				if(attrDef.bReadOnly)
				{
					attr = new CROAttribute<int,signals::etypBoolean>(attrDef.name, attrDef.descr, 0);
				} else {
					attr = new CAttribute<int,signals::etypBoolean>(attrDef.name, attrDef.descr, 0);
				}
				break;
			case signals::etypByte:
				if(attrDef.bReadOnly)
				{
					attr = new CROAttribute<byte,signals::etypByte>(attrDef.name, attrDef.descr, 0);
				} else {
					attr = new CAttribute<byte,signals::etypByte>(attrDef.name, attrDef.descr, 0);
				}
				break;
			case signals::etypShort:
				if(attrDef.bReadOnly)
				{
					attr = new CROAttribute<short,signals::etypShort>(attrDef.name, attrDef.descr, 0);
				} else {
					attr = new CAttribute<short,signals::etypShort>(attrDef.name, attrDef.descr, 0);
				}
				break;
			case signals::etypLong:
				if(attrDef.bReadOnly)
				{
					attr = new CROAttribute<long,signals::etypLong>(attrDef.name, attrDef.descr, 0);
				} else {
					attr = new CAttribute<long,signals::etypLong>(attrDef.name, attrDef.descr, 0);
				}
				break;
			case signals::etypSingle:
				if(attrDef.bReadOnly)
				{
					attr = new CROAttribute<float,signals::etypSingle>(attrDef.name, attrDef.descr, 0);
				} else {
					attr = new CAttribute<float,signals::etypSingle>(attrDef.name, attrDef.descr, 0);
				}
				break;
			case signals::etypDouble:
				if(attrDef.bReadOnly)
				{
					attr = new CROAttribute<double,signals::etypDouble>(attrDef.name, attrDef.descr, 0);
				} else {
					attr = new CAttribute<double,signals::etypDouble>(attrDef.name, attrDef.descr, 0);
				}
				break;
			case signals::etypComplex:
				if(attrDef.bReadOnly)
				{
					attr = new CROAttribute<std::complex<float>,signals::etypComplex>(attrDef.name, attrDef.descr, 0);
				} else {
					attr = new CAttribute<std::complex<float>,signals::etypComplex>(attrDef.name, attrDef.descr, 0);
				}
				break;
			case signals::etypString:
				if(attrDef.bReadOnly)
				{
					attr = new CROAttribute<std::string,signals::etypString>(attrDef.name, attrDef.descr, 0);
				} else {
					attr = new CAttribute<std::string,signals::etypString>(attrDef.name, attrDef.descr, 0);
				}
				break;
			}
			if(attr)
			{
				const char* name = attr->Name();
				m_ownedAttrs.insert(attr);
				m_attributes.insert(TVoidMapToAttr::value_type(name, attr));
				m_attrNames.insert(TStringMapToVoid::value_type(name, name));
				if(attrDef.bVisible) m_visibleAttrs.insert(attr);
			}
		}
	}
}

unsigned CAttributesBase::Itemize(signals::IAttribute** attrs, unsigned availElem)
{
	if(attrs && availElem)
	{
		unsigned i;
		TAttrSet::const_iterator trans;
		for(i=0, trans=m_visibleAttrs.begin(); i < availElem && trans != m_visibleAttrs.end(); i++, trans++)
		{
			CAttributeBase* attr = *trans;
			attrs[i] = attr;
		}
	}
	return m_visibleAttrs.size();
}

void CAttributesBase::Observe(signals::IAttributeObserver* obs)
{
	for(TAttrSet::const_iterator trans=m_visibleAttrs.begin(); trans != m_visibleAttrs.end(); trans++)
	{
		(*trans)->Observe(obs);
	}
}

void CAttributesBase::Unobserve(signals::IAttributeObserver* obs)
{
	for(TAttrSet::const_iterator trans=m_visibleAttrs.begin(); trans != m_visibleAttrs.end(); trans++)
	{
		(*trans)->Unobserve(obs);
	}
}
