/*
	Copyright 2013 Erik Anderson

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
#include "divide_by_n.h"
#include "make_frame.h"

FrameFunctionBase<signals::etypVecByte, DivideByN<unsigned char> > divideByN_byte;
FrameFunctionBase<signals::etypVecShort, DivideByN<short> > divideByN_short;
FrameFunctionBase<signals::etypVecLong, DivideByN<long> > divideByN_long;
FrameFunctionBase<signals::etypVecInt64, DivideByN<__int64> > divideByN_int64;
FrameFunctionBase<signals::etypVecSingle, DivideByN<float> > divideByN_float;
FrameFunctionBase<signals::etypVecDouble, DivideByN<double> > divideByN_double;
FrameFunctionBase<signals::etypVecComplex, DivideByN<std::complex<float>,float> > divideByN_cpx;
FrameFunctionBase<signals::etypVecCmplDbl, DivideByN<std::complex<double>,double> > divideByN_cpxdbl;
FrameFunctionBase<signals::etypVecLRSingle, DivideByN<std::complex<float>,float> > divideByN_lr;

CFrameBuilderDriver<signals::etypBoolean> frame_bool;
CFrameBuilderDriver<signals::etypByte> frame_byte;
CFrameBuilderDriver<signals::etypShort> frame_short;
CFrameBuilderDriver<signals::etypLong> frame_long;
CFrameBuilderDriver<signals::etypInt64> frame_int64;
CFrameBuilderDriver<signals::etypSingle> frame_float;
CFrameBuilderDriver<signals::etypDouble> frame_double;
CFrameBuilderDriver<signals::etypComplex> frame_cpx;
CFrameBuilderDriver<signals::etypCmplDbl> frame_cpxdbl;
CFrameBuilderDriver<signals::etypLRSingle> frame_lr;

signals::IBlockDriver* BLOCKS[] =
{
	// make frame
	&frame_bool, &frame_byte, &frame_short, &frame_long, &frame_int64, &frame_float, &frame_double,
	&frame_cpx, &frame_cpxdbl, &frame_lr,
};

signals::IFunctionSpec* FUNCTIONS[] =
{
	// divide by N
	&divideByN_byte, &divideByN_short, &divideByN_long, &divideByN_int64, &divideByN_float, &divideByN_double,
	&divideByN_cpx, &divideByN_cpxdbl, &divideByN_lr,
};

extern "C" unsigned QueryDrivers(signals::IBlockDriver** drivers, unsigned availDrivers)
{
	if(drivers && availDrivers)
	{
		unsigned xfer = min(availDrivers, _countof(BLOCKS));
		for(unsigned idx=0; idx < xfer; idx++)
		{
			drivers[idx] = BLOCKS[idx];
		}
	}
	return _countof(BLOCKS);
}

extern "C" unsigned QueryFunctions(signals::IFunctionSpec** funcs, unsigned availFuncs)
{
	if(funcs && availFuncs)
	{
		unsigned xfer = min(availFuncs, _countof(FUNCTIONS));
		for(unsigned idx=0; idx < xfer; idx++)
		{
			funcs[idx] = FUNCTIONS[idx];
		}
	}
	return _countof(FUNCTIONS);
}
