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
	static fftss::CFFTransformDriver<signals::etypCmplDbl,signals::etypVecCmplDbl> fft_dd;
	static fftss::CFFTransformDriver<signals::etypCmplDbl,signals::etypVecComplex> fft_ds;
	static fftss::CFFTransformDriver<signals::etypComplex,signals::etypVecCmplDbl> fft_sd;
	static fftss::CFFTransformDriver<signals::etypComplex,signals::etypVecComplex> fft_ss;
	if(drivers && availDrivers)
	{
		if(availDrivers > 0) drivers[0] = &fft_dd;
		if(availDrivers > 1) drivers[1] = &fft_ds;
		if(availDrivers > 2) drivers[2] = &fft_sd;
		if(availDrivers > 3) drivers[3] = &fft_ss;
	}
	return 4;
}

namespace fftss {

// ------------------------------------------------------------------ class CFFTransform

class CAttr_block_size : public CRWAttribute<signals::etypLong>
{
private:
	typedef CRWAttribute<signals::etypLong> base;
public:
	inline CAttr_block_size(CFFTransformBase& parent, const char* name, const char* descr, long deflt)
		:base(name, descr, deflt), m_parent(parent)
	{
		m_parent.setBlockSize(deflt);
	}

protected:
	CFFTransformBase& m_parent;

protected:
	virtual void onSetValue(const store_type& newVal)
	{
		m_parent.setBlockSize(newVal);
		base::onSetValue(newVal);
	}
};

CFFTransformBase::CFFTransformBase(signals::IBlockDriver* driver)
	:m_driver(driver),m_currPlan(NULL),m_requestSize(0),m_inBuffer(NULL),m_outBuffer(NULL),m_bufSize(0),
	 m_refreshPlanEvent(fastdelegate::FastDelegate0<>(this, &CFFTransformBase::refreshPlan)),m_bFaulted(false)
{
//	buildAttrs();
}

CFFTransformBase::~CFFTransformBase()
{
	clearPlan();
}

const char* CFFTransformBase::NAME = "FFT Transform using fftss";

void CFFTransformBase::clearPlan()
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

void CFFTransformBase::buildAttrs()
{
	attrs.blockSize = addLocalAttr(true, new CAttr_block_size(*this, "blockSize", "Number of samples to process in each block", DEFAULT_BLOCK_SIZE));

//	m_outgoing.buildAttrs(*this);
}

void CFFTransformBase::setBlockSize(long blockSize)
{
	long prevValue = InterlockedExchange(&m_requestSize, blockSize);
	if(blockSize != prevValue)
	{
		m_refreshPlanEvent.fire();
	}
}

void CFFTransformBase::refreshPlan()
{
	{
		Locker lock(m_planLock);
		if(m_requestSize == m_bufSize) return;
	}
	startPlan(false);
}

bool CFFTransformBase::startPlan(bool bLockHeld)
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

void fft_process_thread(CFFTransform<signals::etypCmplDbl,signals::etypCmplDbl>* owner)
{
	ThreadBase::SetThreadName("FFTSS Transform Thread");

	CFFTransformBase::TComplexDbl buffer[CFFTransform<signals::etypCmplDbl,signals::etypCmplDbl>::IN_BUFFER_SIZE];

	unsigned inOffset = 0;
	while(owner->m_bDataThreadEnabled)
	{
		unsigned recvCount = owner->m_incoming.Read(signals::etypCmplDbl, &buffer,
			CFFTransform<signals::etypCmplDbl,signals::etypCmplDbl>::IN_BUFFER_SIZE, TRUE,
			CFFTransform<signals::etypCmplDbl,signals::etypCmplDbl>::IN_BUFFER_TIMEOUT);
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
				memcpy(&owner->m_inBuffer[inOffset], buffer, sizeof(CFFTransformBase::TComplexDbl) * numXfer);
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
				for(unsigned idx=0; idx < numXfer; idx++)
				{
					memcpy(owner->m_inBuffer, &buffer[outOffset], sizeof(CFFTransformBase::TComplexDbl) * numXfer);
				}
				outOffset += numXfer;
				inOffset = recvCount;
			}
		}
	}
}

}
