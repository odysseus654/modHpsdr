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

const unsigned char CFFTransformDriver::FINGERPRINT[] = { 1, (unsigned char)signals::etypCmplDbl, 1, (unsigned char)signals::etypCmplDbl };
const char* CFFTransformDriver::NAME = "fft";
const char* CFFTransformDriver::DESCR = "FFT Transform using fftss";

// ------------------------------------------------------------------ class CFFTransform

class CAttr_block_size : public CRWAttribute<signals::etypShort>
{
private:
	typedef CRWAttribute<signals::etypShort> base;
public:
	inline CAttr_block_size(CFFTransform& parent, const char* name, const char* descr, short deflt)
		:base(name, descr, deflt), m_parent(parent)
	{
		m_parent.setBlockSize(deflt);
	}

protected:
	CFFTransform& m_parent;

protected:
	virtual void onSetValue(const store_type& newVal)
	{
		m_parent.setBlockSize(newVal);
		base::onSetValue(newVal);
	}
};

#pragma warning(push)
#pragma warning(disable: 4355)
CFFTransform::CFFTransform(signals::IBlockDriver* driver)
	:m_driver(driver),m_currPlan(NULL),m_requestSize(0),m_inBuffer(NULL),m_outBuffer(NULL),m_bufSize(0),
	 m_refreshPlanEvent(fastdelegate::FastDelegate0<>(this, &CFFTransform::refreshPlan)),m_bFaulted(false),
	 m_incoming(this),m_outgoing(this),m_bDataThreadEnabled(true),
	 m_dataThread(Thread<CFFTransform*>::delegate_type(&fft_process_thread))
{
	buildAttrs();
	m_dataThread.launch(this, THREAD_PRIORITY_NORMAL);
}
#pragma warning(pop)

CFFTransform::~CFFTransform()
{
	m_bDataThreadEnabled = false;
	m_dataThread.close();
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

unsigned CFFTransform::Incoming(signals::IInEndpoint** ep, unsigned availEP)
{
	if(ep && availEP)
	{
		ep[0] = &m_incoming;
	}
	return 1;
}

unsigned CFFTransform::Outgoing(signals::IOutEndpoint** ep, unsigned availEP)
{
	if(ep && availEP)
	{
		ep[0] = &m_outgoing;
	}
	return 1;
}

void CFFTransform::buildAttrs()
{
	attrs.blockSize = addLocalAttr(true, new CAttr_block_size(*this, "blockSize", "Number of samples to process in each block", DEFAULT_BLOCK_SIZE));
	m_outgoing.buildAttrs(*this);
}

void CFFTransform::setBlockSize(long blockSize)
{
	long prevValue = InterlockedExchange(&m_requestSize, blockSize);
	if(blockSize != prevValue)
	{
		m_refreshPlanEvent.fire();
	}
}

void CFFTransform::refreshPlan()
{
	{
		Locker lock(m_planLock);
		if(m_requestSize == m_bufSize) return;
	}
	startPlan(false);
}

bool CFFTransform::startPlan(bool bLockHeld)
{
	long reqSize = m_requestSize;
	if(!reqSize) return false;

	TComplexDbl* tempIn = (TComplexDbl*)::fftss_malloc(sizeof(TComplexDbl)*reqSize);
	TComplexDbl* tempOut = (TComplexDbl*)::fftss_malloc(sizeof(TComplexDbl)*reqSize);

	fftss_plan newPlan = ::fftss_plan_dft_1d(reqSize, (double*)tempIn, (double*)tempOut, FFTSS_FORWARD, FFTSS_MEASURE);
//	fftss_plan newPlan = ::fftss_plan_dft_1d(reqSize, (double*)tempIn, (double*)tempOut, -1, 0);

	Locker lock(m_planLock, !bLockHeld);
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
//	attrs.rate = addRemoteAttr("rate", parent.attrs.recv_speed);
}

void CFFTransform::fft_process_thread(CFFTransform* owner)
{
	ThreadBase::SetThreadName("FFTSS Transform Thread");

	TComplexDbl buffer[IN_BUFFER_SIZE];

	unsigned inOffset = 0;
	while(owner->m_bDataThreadEnabled)
	{
		unsigned recvCount = owner->m_incoming.Read(signals::etypCmplDbl, &buffer,
			IN_BUFFER_SIZE, TRUE, IN_BUFFER_TIMEOUT);
		if(recvCount)
		{
			Locker lock(owner->m_planLock);
			if(!owner->m_currPlan)
			{
				if(!owner->startPlan(true))
				{
					inOffset = 0;		// no plan!
					continue;
				}
			}

			ASSERT(owner->m_inBuffer && owner->m_bufSize);
			unsigned numXfer = min(owner->m_bufSize - inOffset, recvCount);
			if(numXfer)
			{
				memcpy(&owner->m_inBuffer[inOffset], buffer, sizeof(buffer[0]) * numXfer);
			}
			inOffset += numXfer;
			recvCount -= numXfer;
			unsigned outOffset = numXfer;
			while(owner->m_bufSize <= inOffset)
			{
				ASSERT(owner->m_inBuffer && owner->m_outBuffer && owner->m_bufSize && owner->m_currPlan);
				::fftss_execute_dft(owner->m_currPlan, (double*)owner->m_inBuffer, (double*)owner->m_outBuffer);

				unsigned outSent = owner->m_outgoing.Write(signals::etypCmplDbl, owner->m_outBuffer, owner->m_bufSize, INFINITE);
				if(outSent == owner->m_bufSize)
				{
					owner->m_bFaulted = false;
				}
				else if(!owner->m_bFaulted && owner->m_outgoing.isConnected())
				{
					owner->m_bFaulted = true;
					owner->m_outgoing.attrs.sync_fault->fire();
				}

				numXfer = min(recvCount, owner->m_bufSize);
				if(numXfer)
				{
					memcpy(owner->m_inBuffer, &buffer[outOffset], sizeof(buffer[0]) * numXfer);
				}
				outOffset += numXfer;
				inOffset = recvCount;
			}
		}
	}
}

// ------------------------------------------------------------------ class CFFTransformDriver

signals::IBlock * CFFTransformDriver::Create()
{
	signals::IBlock* blk = new CFFTransform(this);
	blk->AddRef();
	return blk;
}

}
