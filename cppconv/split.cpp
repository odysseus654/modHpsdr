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
#include "stdafx.h"
#include "split.h"

const char* CSplitterBase::NAME = "Send a stream to multiple destinations";
const char* CSplitterBase::CIncoming::EP_NAME = "in";
const char* CSplitterBase::CIncoming::EP_DESCR = "\"splitter\" incoming endpoint";
const char* CSplitterBase::COutgoing::EP_NAME = "out";
const char* CSplitterBase::COutgoing::EP_DESCR = "\"splitter\" outgoing endpoint";

// ------------------------------------------------------------------ class CSplitterBase

#pragma warning(push)
#pragma warning(disable: 4355)
CSplitterBase::CSplitterBase(signals::EType type, signals::IBlockDriver* driver)
	:CThreadBlockBase(driver),m_type(type),m_incoming(this),
	 m_outgoing(std::vector<COutgoing>(NUM_EP, COutgoing(this)))
{
	buildAttrs();
	startThread();
}
#pragma warning(pop)

void CSplitterBase::buildAttrs()
{
	unsigned numEp = m_outgoing.size();
	for(unsigned idx = 0; idx < numEp; idx++)
	{
		m_outgoing[idx].buildAttrs(*this);
	}
}

unsigned CSplitterBase::Outgoing(signals::IOutEndpoint** ep, unsigned availEP)
{
	unsigned numEp = m_outgoing.size();
	unsigned numToCopy = min(availEP, numEp);
	if(numToCopy)
	{
		for(unsigned idx=0; idx < numToCopy; idx++)
		{
			ep[idx] = &m_outgoing[idx];
		}
	}
	return numEp;
}

void CSplitterBase::COutgoing::buildAttrs(const CSplitterBase& parent)
{
	attrs.sync_fault = addLocalAttr(true, new CEventAttribute("syncFault", "Fires when a sync fault happens in a receive stream"));
}

signals::IEPBuffer* CSplitterBase::CIncoming::CreateBuffer()
{
	ReadLocker lock(m_connRecvLock);
	if(!m_connRecv) return NULL;
	return m_connRecv->CreateBuffer();
}
