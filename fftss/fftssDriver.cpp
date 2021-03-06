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
#include "fftssDriver.h"

extern "C" unsigned QueryDrivers(signals::IBlockDriver** drivers, unsigned availDrivers)
{
	static fftss::CFFTransformDriver fft_dd;
	if(drivers && availDrivers)
	{
		if(availDrivers > 0) drivers[0] = &fft_dd;
	}
	return 1;
}

namespace fftss {

const char* CFFTransform::NAME = "FFT Transform using fftss";
const char* CFFTransform::CIncoming::EP_NAME = "in";
const char* CFFTransform::CIncoming::EP_DESCR = "FFT Transform incoming endpoint";
const char* CFFTransform::COutgoing::EP_NAME = "out";
const char* CFFTransform::COutgoing::EP_DESCR = "FFT Transform outgoing endpoint";

const unsigned char CFFTransformDriver::FINGERPRINT[] = { 1, (unsigned char)signals::etypVecCmplDbl, 1, (unsigned char)signals::etypVecCmplDbl };
const char* CFFTransformDriver::NAME = "fft";
const char* CFFTransformDriver::DESCR = "FFT Transform using fftss";

// ------------------------------------------------------------------ class CFFTransform

#pragma warning(push)
#pragma warning(disable: 4355)
CFFTransform::CFFTransform(signals::IBlockDriver* driver)
	:CThreadBlockBase(driver),m_currPlan(NULL),m_requestSize(0),m_inBuffer(NULL),m_outBuffer(NULL),m_bufSize(0),
	 m_incoming(this),m_outgoing(this)
{
	buildAttrs();
	startThread();
}
#pragma warning(pop)

CFFTransform::~CFFTransform()
{
	stopThread();
	clearPlan();
}

void CFFTransform::clearPlan()
{
	if(m_currPlan)
	{
		::fftss_destroy_plan(m_currPlan);
		m_currPlan = NULL;
	}
	if(m_inBuffer)
	{
		::fftss_free(m_inBuffer);
		m_inBuffer = NULL;
	}
	if(m_outBuffer)
	{
		::fftss_free(m_outBuffer);
		m_outBuffer = NULL;
	}
	m_bufSize = 0;

}

void CFFTransform::buildAttrs()
{
	m_outgoing.buildAttrs(*this);
}

void CFFTransform::refreshPlan()
{
	Locker lock(m_planLock);
	if(m_requestSize == m_bufSize) return;
	startPlan();
}

bool CFFTransform::startPlan()
{
	// ASSUMES m_planLock IS HELD
	long reqSize = m_requestSize;
	if(!reqSize) return false;

	TComplexDbl* tempIn = (TComplexDbl*)::fftss_malloc(sizeof(TComplexDbl)*reqSize);
	TComplexDbl* tempOut = (TComplexDbl*)::fftss_malloc(sizeof(TComplexDbl)*reqSize);

	fftss_plan newPlan = ::fftss_plan_dft_1d(reqSize, (double*)tempIn, (double*)tempOut, FFTSS_FORWARD, FFTSS_MEASURE);

	if(m_bufSize != reqSize && reqSize == m_requestSize)
	{
		clearPlan();
		m_currPlan = newPlan;
		m_inBuffer = tempIn;
		m_outBuffer = tempOut;
		m_bufSize = reqSize;
		return true;
	}
	else
	{
		::fftss_destroy_plan(newPlan);
		::fftss_free(tempIn);
		::fftss_free(tempOut);
		return m_bufSize == reqSize;
	}
}

void CFFTransform::COutgoing::buildAttrs(const CFFTransform& parent)
{
	attrs.sync_fault = addLocalAttr(true, new CEventAttribute("syncFault", "Fires when a sync fault happens in a receive stream"));
}

void CFFTransform::thread_run()
{
	ThreadBase::SetThreadName("FFTSS Transform Thread");

	while(threadRunning())
	{
		Locker lock(m_planLock);
		if(!m_requestSize)
		{
			Sleep(IN_BUFFER_TIMEOUT);		// no plan!
			continue;
		}
		else if(!m_currPlan || m_requestSize != m_bufSize)
		{
			if(!startPlan())
			{
				Sleep(IN_BUFFER_TIMEOUT);		// no plan!
				continue;
			}
		}
		ASSERT(m_inBuffer && m_bufSize);
		signals::IVector* inVector = NULL;
		BOOL recvFrame = m_incoming.ReadOne(signals::etypVecCmplDbl, &inVector, IN_BUFFER_TIMEOUT);
		if(recvFrame)
		{
			if(inVector->Size() != m_bufSize)
			{
				ASSERT(FALSE); // huh? did the buffer size just change?
				inVector->Release();
				continue;
			}
			memcpy(m_inBuffer, inVector->Data(), m_bufSize * sizeof(TComplexDbl));
			inVector->Release();

			ASSERT(m_inBuffer && m_outBuffer && m_bufSize && m_currPlan);
			::fftss_execute_dft(m_currPlan, (double*)m_inBuffer, (double*)m_outBuffer);

			typedef StoreType<signals::etypVecCmplDbl>::buffer_templ VectorType;
			VectorType* outVector = VectorType::retrieve(m_bufSize);
			memcpy(outVector->data, m_outBuffer, m_bufSize * sizeof(TComplexDbl));

			BOOL outFrame = m_outgoing.WriteOne(signals::etypVecCmplDbl, &outVector, INFINITE);
			if(!outFrame)
			{
				if(m_outgoing.isConnected()) m_outgoing.attrs.sync_fault->fire();
				outVector->Release();
			}
		}
	}
}

// ------------------------------------------------------------------ class CFFTransform::CIncoming

CFFTransform::CIncoming::~CIncoming()
{
	if(m_lastWidthAttr)
	{
		m_lastWidthAttr->Unobserve(this);
		m_lastWidthAttr = NULL;
	}
}

void CFFTransform::CIncoming::OnConnection(signals::IEPRecvFrom *conn)
{
	if(m_lastWidthAttr)
	{
		m_lastWidthAttr->Unobserve(this);
		m_lastWidthAttr = NULL;
	}
	if(!conn) return;
	signals::IAttributes* attrs = conn->OutputAttributes();
	if(!attrs) return;
	signals::IAttribute* attr = attrs->GetByName("blockSize");
	if(attr && (attr->Type() == signals::etypShort || attr->Type() == signals::etypLong))
	{
		m_lastWidthAttr = attr;
		m_lastWidthAttr->Observe(this);
		OnChanged(attr, attr->getValue());
	}
}

void CFFTransform::CIncoming::OnChanged(signals::IAttribute* attr, const void* value)
{
	if(attr == m_lastWidthAttr)
	{
		CFFTransform* base = static_cast<CFFTransform*>(m_parent);
		long newWidth;
		if(attr->Type() == signals::etypShort)
		{
			newWidth = *(short*)value;
		} else {
			newWidth = *(long*)value;
		}
		InterlockedExchange(&base->m_requestSize, newWidth);
	}
	else ASSERT(FALSE);
}

void CFFTransform::CIncoming::OnDetached(signals::IAttribute* attr)
{
	if(attr == m_lastWidthAttr)
	{
		m_lastWidthAttr = NULL;
	}
	else ASSERT(FALSE);
}

// ------------------------------------------------------------------ class CFFTransformDriver

signals::IBlock * CFFTransformDriver::Create()
{
	signals::IBlock* blk = new CFFTransform(this);
	blk->AddRef();
	return blk;
}

}
