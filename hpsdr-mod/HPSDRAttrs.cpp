#include "stdafx.h"
#include "HPSDRAttrs.h"

namespace hpsdr {

// ------------------------------------------------------------------ attribute proxies

void CAttr_inProxy::setProxy(signals::IAttribute& target)
{
	Locker proxyLock(m_proxyLock);
	if(proxyObject)
	{
		Locker obsLock(m_observersLock);
		for(TObserverList::const_iterator trans=m_observers.begin(); trans != m_observers.end(); trans++)
		{
			proxyObject->Unobserve(*trans);
		}
	}

	proxyObject = &target;

	if(proxyObject)
	{
		Locker obsLock(m_observersLock);
		for(TObserverList::const_iterator trans=m_observers.begin(); trans != m_observers.end(); trans++)
		{
			proxyObject->Observe(*trans);
			(*trans)->OnChanged(Name(), proxyObject->Type(), proxyObject->getValue());
		}
	}
}

// ------------------------------------------------------------------ CAttr_out_recv_speed

const long CAttr_out_recv_speed::recv_speed_options[] = { 48000, 96000, 192000 };

bool CAttr_out_recv_speed::isValidValue(const store_type& newVal) const
{
	int iFreq = (newVal + 500) / 1000;
	for(unsigned idx = 0; idx < _countof(recv_speed_options); idx++)
	{
		if(recv_speed_options[idx] / 1000 == iFreq) return true;
	}
	return false;
}

bool CAttr_out_recv_speed::setValue(const store_type& newVal)
{
	int iFreq = (newVal + 500) / 1000;
	for(byte idx = 0; idx < _countof(recv_speed_options); idx++)
	{
		if(recv_speed_options[idx] / 1000 == iFreq)
		{
			m_rawValue.nativeSetValue(idx);
			m_parent.m_recvSpeed = iFreq;
			return true;
		}
	}
	return false;
}

}