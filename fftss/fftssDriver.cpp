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

#include "fftss/include/fftss.h"

extern "C" unsigned QueryDrivers(signals::IBlockDriver** drivers, unsigned availDrivers)
{
	if(drivers && availDrivers)
	{
		drivers[0] = &DRIVER_FFTransform;
	}
	return 1;
}

fftss::CFFTransformDriver DRIVER_FFTransform;

namespace fftss {

// ------------------------------------------------------------------ class CFFTransformDriver

const char* CFFTransformDriver::NAME = "FFT Transform using fftss";

signals::IBlock * CFFTransformDriver::Create()
{
	return new CFFTransform<signals::etypCmplDbl,signals::etypCmplDbl>();
}

// ------------------------------------------------------------------ class CFFTransform

CFFTransformBase::CFFTransformBase()
	:m_currPlan(NULL),m_requestSize(0),m_inBuffer(NULL),m_outBuffer(NULL),m_bufSize(0),
	 m_refreshPlanEvent(fastdelegate::FastDelegate0<>(this, &CFFTransformBase::refreshPlan))
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

}
