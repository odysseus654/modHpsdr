/*
	Copyright 2013-2014 Erik Anderson

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
#include "summ_frame.h"
#include "divide_by_n.h"
#include "make_frame.h"
#include "real_chop.h"

FrameFunctionBase<signals::etypVecByte, DivideByN<unsigned char> > divideByN_byte;
FrameFunctionBase<signals::etypVecShort, DivideByN<short> > divideByN_short;
FrameFunctionBase<signals::etypVecLong, DivideByN<long> > divideByN_long;
FrameFunctionBase<signals::etypVecInt64, DivideByN<__int64> > divideByN_int64;
FrameFunctionBase<signals::etypVecSingle, DivideByN<float> > divideByN_float;
FrameFunctionBase<signals::etypVecDouble, DivideByN<double> > divideByN_double;
FrameFunctionBase<signals::etypVecComplex, DivideByN<std::complex<float>,float> > divideByN_cpx;
FrameFunctionBase<signals::etypVecCmplDbl, DivideByN<std::complex<double>,double> > divideByN_cpxdbl;
FrameFunctionBase<signals::etypVecLRSingle, DivideByN<std::complex<float>,float> > divideByN_lr;

ChopRealFrame<signals::etypVecBoolean> chop_bool;
ChopRealFrame<signals::etypVecByte> chop_byte;
ChopRealFrame<signals::etypVecShort> chop_short;
ChopRealFrame<signals::etypVecLong> chop_long;
ChopRealFrame<signals::etypVecInt64> chop_int64;
ChopRealFrame<signals::etypVecSingle> chop_float;
ChopRealFrame<signals::etypVecDouble> chop_double;
ChopRealFrame<signals::etypVecComplex> chop_cpx;
ChopRealFrame<signals::etypVecCmplDbl> chop_cpxdbl;
ChopRealFrame<signals::etypVecLRSingle> chop_lr;

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

CFrameSummary<signals::etypBoolean, frame_max<unsigned char> > summ_max_bool;
CFrameSummary<signals::etypByte, frame_max<unsigned char> > summ_max_byte;
CFrameSummary<signals::etypShort, frame_max<short> > summ_max_short;
CFrameSummary<signals::etypLong, frame_max<long> > summ_max_long;
CFrameSummary<signals::etypInt64, frame_max<__int64> > summ_max_int64;
CFrameSummary<signals::etypSingle, frame_max<float> > summ_max_float;
CFrameSummary<signals::etypDouble, frame_max<double> > summ_max_double;

CFrameSummary<signals::etypBoolean, frame_min<unsigned char> > summ_min_bool;
CFrameSummary<signals::etypByte, frame_min<unsigned char> > summ_min_byte;
CFrameSummary<signals::etypShort, frame_min<short> > summ_min_short;
CFrameSummary<signals::etypLong, frame_min<long> > summ_min_long;
CFrameSummary<signals::etypInt64, frame_min<__int64> > summ_min_int64;
CFrameSummary<signals::etypSingle, frame_min<float> > summ_min_float;
CFrameSummary<signals::etypDouble, frame_min<double> > summ_min_double;

CFrameSummary<signals::etypSingle, frame_mean<unsigned char, short, float>, signals::etypVecBoolean > summ_mean_bool;
CFrameSummary<signals::etypSingle, frame_mean<unsigned char, short, float>, signals::etypVecByte > summ_mean_byte;
CFrameSummary<signals::etypSingle, frame_mean<short, long, float>, signals::etypVecShort> summ_mean_short;
CFrameSummary<signals::etypDouble, frame_mean<long, __int64, double>, signals::etypVecLong> summ_mean_long;
CFrameSummary<signals::etypDouble, frame_mean<__int64, double, double>, signals::etypVecInt64 > summ_mean_int64;
CFrameSummary<signals::etypDouble, frame_mean<float, double, double>, signals::etypVecSingle > summ_mean_float;
CFrameSummary<signals::etypDouble, frame_mean<double, double, double>, signals::etypVecDouble > summ_mean_double;

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

	// chop real frame
	&chop_bool, &chop_byte, &chop_short, &chop_long, &chop_int64, &chop_float, &chop_double,
	&chop_cpx, &chop_cpxdbl, &chop_lr,

	// frame maximum
	&summ_max_bool, &summ_max_byte, &summ_max_short, &summ_max_long, &summ_max_int64, &summ_max_float, &summ_max_double,

	// frame minimum
	&summ_min_bool, &summ_min_byte, &summ_min_short, &summ_min_long, &summ_min_int64, &summ_min_float, &summ_min_double,

	// frame average
	&summ_mean_bool, &summ_mean_byte, &summ_mean_short, &summ_mean_long, &summ_mean_int64, &summ_mean_float, &summ_mean_double
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
