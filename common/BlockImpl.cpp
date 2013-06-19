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
#include "BlockImpl.h"

// ------------------------------------------------------------------ class CInEndpointBase

BOOL CInEndpointBase::Connect(signals::IEPRecvFrom* recv)
{
	Locker lock(m_connRecvLock);
	if(recv != m_connRecv)
	{
		if(recv) recv->onSinkConnected(this);
		OnConnection(recv);
		if(m_connRecv) m_connRecv->onSinkDisconnected(this);
		m_connRecv = recv;
		if(m_connRecv) m_connRecvConnected.wakeAll();
	}
	return true;
}

unsigned CInEndpointBase::Read(signals::EType type, void* buffer, unsigned numAvail, BOOL bFillAll, unsigned msTimeout)
{
	Locker lock(m_connRecvLock);
	if(!m_connRecv)
	{
		if(!msTimeout) return 0;
		if(!m_connRecvConnected.sleep(lock, msTimeout)) return 0;
	}
	ASSERT(m_connRecv);
	return m_connRecv->Read(type, buffer, numAvail, bFillAll, msTimeout);
}

void CInEndpointBase::MergeRemoteAttrs(std::map<std::string,signals::IAttribute*>& attrs, unsigned flags)
{
	Locker lock(m_connRecvLock);
	if(!m_connRecv) return;
	signals::IAttributes* rattrs = m_connRecv->OutputAttributes();
	if(!rattrs) return;

	unsigned count = rattrs->Itemize(NULL, 0, flags);
	signals::IAttribute** attrList = new signals::IAttribute*[count];
	rattrs->Itemize(attrList, count, flags);
	for(unsigned idx=0; idx < count; idx++)
	{
		signals::IAttribute* attr = attrList[idx];
		std::string name = attr->Name();
		if(attrs.find(name) == attrs.end())
		{
			attrs.insert(std::map<std::string,signals::IAttribute*>::value_type(name, attr));
		}
	}
	delete [] attrList;
}

signals::IAttribute* CInEndpointBase::RemoteGetByName(const char* name)
{
	Locker lock(m_connRecvLock);
	if(!m_connRecv) return NULL;
	signals::IAttributes* attrs = m_connRecv->OutputAttributes();
	return attrs ? attrs->GetByName(name) : NULL;
}

// ------------------------------------------------------------------ class COutEndpointBase

BOOL COutEndpointBase::Connect(signals::IEPSendTo* send)
{
	Locker lock(m_connSendLock);
	if(send != m_connSend)
	{
		if(send) send->AddRef(this);
		OnConnection(send);
		if(m_connSend) m_connSend->Release(this);
		m_connSend = send;
		if(m_connSend) m_connSendConnected.wakeAll();
	}
	return true;
}

unsigned COutEndpointBase::Write(signals::EType type, void* buffer, unsigned numElem, unsigned msTimeout)
{
	Locker lock(m_connSendLock);
	if(!m_connSend)
	{
		if(!msTimeout) return 0;
		if(!m_connSendConnected.sleep(lock, msTimeout)) return 0;
	}
	ASSERT(m_connSend);
	return m_connSend->Write(type, buffer, numElem, msTimeout);
}

// ------------------------------------------------------------------ class CAttributeBase

void CAttributeBase::Observe(signals::IAttributeObserver* obs)
{
	Locker lock(m_observersLock);
	if(m_observers.find(obs) == m_observers.end())
	{
		m_observers.insert(obs);
	}
}

void CAttributeBase::Unobserve(signals::IAttributeObserver* obs)
{
	Locker lock(m_observersLock);
	TObserverList::iterator lookup = m_observers.find(obs);
	if(lookup != m_observers.end())
	{
		m_observers.erase(lookup);
	}
}

// ------------------------------------------------------------------ class CAttributesBase

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

signals::IAttribute* CAttributesBase::GetByName(const char* name)
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

unsigned CAttributesBase::Itemize(signals::IAttribute** attrs, unsigned availElem, unsigned flags)
{
	if(flags & signals::flgIncludeHidden)
	{
		if(attrs && availElem)
		{
			unsigned i;
			TVoidMapToAttr::const_iterator trans;
			for(i=0, trans=m_attributes.begin(); i < availElem && trans != m_attributes.end(); i++, trans++)
			{
				CAttributeBase* attr = trans->second;
				attrs[i] = attr;
			}
		}
		return m_attributes.size();
	}
	else
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

// ------------------------------------------------------------------ class CCascadedAttributesBase

unsigned CCascadedAttributesBase::Itemize(signals::IAttribute** attrs, unsigned availElem, unsigned flags)
{
	if(flags & signals::flgLocalOnly)
	{
		return CAttributesBase::Itemize(attrs, availElem, flags);
	}
	else
	{
		typedef std::map<std::string,signals::IAttribute*> TStringMapToAttr;
		TStringMapToAttr foundAttrs;

		if(flags & signals::flgIncludeHidden)
		{
			for(TVoidMapToAttr::const_iterator trans=m_attributes.begin(); trans != m_attributes.end(); trans++)
			{
				foundAttrs.insert(TStringMapToAttr::value_type(trans->second->Name(), trans->second));
			}
		}
		else
		{
			for(TAttrSet::const_iterator trans=m_visibleAttrs.begin(); trans != m_visibleAttrs.end(); trans++)
			{
				foundAttrs.insert(TStringMapToAttr::value_type((*trans)->Name(), *trans));
			}
		}
		m_src.MergeRemoteAttrs(foundAttrs, flags);

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
}

signals::IAttribute* CCascadedAttributesBase::GetByName(const char* name)
{
	signals::IAttribute* attr = CAttributesBase::GetByName(name);
	return attr ? attr : m_src.RemoteGetByName(name);
}

// ------------------------------------------------------------------ class CBlockBase

unsigned CBlockBase::singleIncoming(signals::IInEndpoint* ep, signals::IInEndpoint** pEp, unsigned pAvailEP)
{
	if(pEp && pAvailEP) pEp[0] = ep;
	return 1;
}

unsigned CBlockBase::singleOutgoing(signals::IOutEndpoint* ep, signals::IOutEndpoint** pEp, unsigned pAvailEP)
{
	if(pEp && pAvailEP) pEp[0] = ep;
	return 1;
}

// ------------------------------------------------------------------ class CThreadBlockBase

void CThreadBlockBase::startThread(int priority)
{
	if(!m_bThreadEnabled)
	{
		m_bThreadEnabled = true;
		m_thread.launch(this, priority);
	}
}

void CThreadBlockBase::stopThread()
{
	if(m_bThreadEnabled)
	{
		m_bThreadEnabled = false;
		m_thread.close();
	}
}

void CThreadBlockBase::process_thread(CThreadBlockBase* owner)
{
	owner->thread_run();
}

// ------------------------------------------------------------------ class CAttribute<etypString>

void CAttribute<signals::etypString>::onSetValue(const store_type& value)
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

// ------------------------------------------------------------------ class CEventAttribute

void CEventAttribute::fire()
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
		nextFunc->fire(*trans);
	}
}
