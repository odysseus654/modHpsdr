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

// cppconv.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"
#include "cppconv.h"

// ---------------------------------------------------------------------------- class InputFunctionBase

BOOL InputFunctionBase::Connect(signals::IEPRecvFrom* recv)
{
	if(recv != m_readFrom)
	{
		if(recv) recv->onSinkConnected(this);
		if(m_readFrom) m_readFrom->onSinkDisconnected(this);
		m_readFrom = recv;
	}
	return true;
}

void InputFunctionBase::onSinkConnected(signals::IInEndpoint* src)
{
	ASSERT(!m_readTo);
	signals::IInEndpoint* oldEp(m_readTo);
	m_readTo = src;
	if(m_readTo) m_readTo->AddRef();
	if(oldEp) oldEp->Release();
}

void InputFunctionBase::onSinkDisconnected(signals::IInEndpoint* src)
{
	ASSERT(m_readTo == src);
	if(m_readTo == src)
	{
		m_readTo = NULL;
		if(src) src->Release();
	}
}

// ---------------------------------------------------------------------------- class OutputFunctionBase

BOOL OutputFunctionBase::Connect(signals::IEPSendTo* send)
{
	if(send != m_writeTo)
	{
		if(send) send->AddRef(this);
		if(m_writeTo) m_writeTo->Release(this);
		m_writeTo = send;
	}
	return true;
}

unsigned OutputFunctionBase::AddRef(signals::IOutEndpoint* iep)
{
	if(iep) m_writeFrom = iep;
	return m_parent->AddRef();
}

unsigned OutputFunctionBase::Release(signals::IOutEndpoint* iep)
{
	if(iep == m_writeFrom) m_writeFrom = NULL;
	return m_parent->Release();
}
