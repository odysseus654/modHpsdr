/*
	Copyright 2014 Erik Anderson

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
#include "identity.h"

const char* CIdentityBase::NAME = "Empty module for thread isolation";
const char* CIdentityBase::CIncoming::EP_NAME = "in";
const char* CIdentityBase::CIncoming::EP_DESCR = "\"identity\" incoming endpoint";
const char* CIdentityBase::COutgoing::EP_NAME = "out";
const char* CIdentityBase::COutgoing::EP_DESCR = "\"identity\" outgoing endpoint";

// ------------------------------------------------------------------ class CSplitterBase

#pragma warning(push)
#pragma warning(disable: 4355)
CIdentityBase::CIdentityBase(signals::EType type, signals::IBlockDriver* driver)
	:CThreadBlockBase(driver),m_type(type),m_incoming(this),m_outgoing(this)
{
	buildAttrs();
	startThread();
}
#pragma warning(pop)

void CIdentityBase::COutgoing::buildAttrs(const CIdentityBase& parent)
{
	attrs.sync_fault = addLocalAttr(true, new CEventAttribute("syncFault", "Fires when a sync fault happens in a receive stream"));
}

signals::IEPBuffer* CIdentityBase::CIncoming::CreateBuffer()
{
	ReadLocker lock(m_connRecvLock);
	if(!m_connRecv) return NULL;
	return m_connRecv->CreateBuffer();
}
