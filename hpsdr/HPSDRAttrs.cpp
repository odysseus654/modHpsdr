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
#include "HPSDRAttrs.h"

namespace hpsdr {

// ------------------------------------------------------------------ attribute proxies

void CAttr_inProxy::setProxy(signals::IAttribute& target)
{
	WriteLocker proxyLock(m_proxyLock);
	if(proxyObject)
	{
		ReadLocker obsLock(m_observersLock);
		for(TObserverList::const_iterator trans=m_observers.begin(); trans != m_observers.end(); trans++)
		{
			proxyObject->Unobserve(*trans);
		}
		ASSERT(proxyObject->Type() == Type());
	}

	proxyObject = &target;

	if(proxyObject)
	{
		ASSERT(proxyObject->Type() == Type());
		ReadLocker obsLock(m_observersLock);
		for(TObserverList::const_iterator trans=m_observers.begin(); trans != m_observers.end(); trans++)
		{
			proxyObject->Observe(*trans);
			(*trans)->OnChanged(this, proxyObject->getValue());
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