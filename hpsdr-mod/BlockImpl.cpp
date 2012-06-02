#include "stdafx.h"
#include "BlockImpl.h"

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
		if(recv) recv->onSinkConnected(this);
		if(m_connRecv) m_connRecv->onSinkDisconnected(this);
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
		if(send) send->AddRef();
		if(m_connSend) m_connSend->Release();
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
