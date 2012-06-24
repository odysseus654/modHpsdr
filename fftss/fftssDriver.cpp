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

// public: virtual bool __thiscall fftss::CFFTransform<30,30>::COutgoing::Connect(struct signals::IEPSender *)
// public: virtual bool __thiscall fftss::CFFTransform<30,30>::COutgoing::Disconnect(void)

}
