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

Function<signals::etypVecByte, signals::etypVecByte, DivideByN<signals::etypVecByte> > divideByN_byte;
Function<signals::etypVecShort, signals::etypVecShort, DivideByN<signals::etypVecShort> > divideByN_short;
Function<signals::etypVecLong, signals::etypVecLong, DivideByN<signals::etypVecLong> > divideByN_long;
Function<signals::etypVecInt64, signals::etypVecInt64, DivideByN<signals::etypVecInt64> > divideByN_int64;
Function<signals::etypVecSingle, signals::etypVecSingle, DivideByN<signals::etypVecSingle> > divideByN_float;
Function<signals::etypVecDouble, signals::etypVecDouble, DivideByN<signals::etypVecDouble> > divideByN_double;
Function<signals::etypVecComplex, signals::etypVecComplex, DivideByN<signals::etypVecComplex, float> > divideByN_cpx;
Function<signals::etypVecCmplDbl, signals::etypVecCmplDbl, DivideByN<signals::etypVecCmplDbl, double> > divideByN_cpxdbl;
Function<signals::etypVecLRSingle, signals::etypVecLRSingle, DivideByN<signals::etypVecLRSingle, float> > divideByN_lr;

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

Function<signals::etypVecBoolean, signals::etypBoolean, frame_max<signals::etypVecBoolean> > summ_max_bool;
Function<signals::etypVecByte, signals::etypByte, frame_max<signals::etypVecByte> > summ_max_byte;
Function<signals::etypVecShort, signals::etypShort, frame_max<signals::etypVecShort> > summ_max_short;
Function<signals::etypVecLong, signals::etypLong, frame_max<signals::etypVecLong> > summ_max_long;
Function<signals::etypVecInt64, signals::etypInt64, frame_max<signals::etypVecInt64> > summ_max_int64;
Function<signals::etypVecSingle, signals::etypSingle, frame_max<signals::etypVecSingle> > summ_max_float;
Function<signals::etypVecDouble, signals::etypDouble, frame_max<signals::etypVecDouble> > summ_max_double;

Function<signals::etypVecBoolean, signals::etypBoolean, frame_min<signals::etypVecBoolean> > summ_min_bool;
Function<signals::etypVecByte, signals::etypByte, frame_min<signals::etypVecByte> > summ_min_byte;
Function<signals::etypVecShort, signals::etypShort, frame_min<signals::etypVecShort> > summ_min_short;
Function<signals::etypVecLong, signals::etypLong, frame_min<signals::etypVecLong> > summ_min_long;
Function<signals::etypVecInt64, signals::etypInt64, frame_min<signals::etypVecInt64> > summ_min_int64;
Function<signals::etypVecSingle, signals::etypSingle, frame_min<signals::etypVecSingle> > summ_min_float;
Function<signals::etypVecDouble, signals::etypDouble, frame_min<signals::etypVecDouble> > summ_min_double;

Function<signals::etypVecBoolean, signals::etypSingle, frame_mean<signals::etypVecBoolean, short, float> > summ_mean_bool;
Function<signals::etypVecByte, signals::etypSingle, frame_mean<signals::etypVecByte, short, float> > summ_mean_byte;
Function<signals::etypVecShort, signals::etypSingle, frame_mean<signals::etypVecShort, long, float> > summ_mean_short;
Function<signals::etypVecLong, signals::etypDouble, frame_mean<signals::etypVecLong, __int64, double> > summ_mean_long;
Function<signals::etypVecInt64, signals::etypDouble, frame_mean<signals::etypVecInt64, double, double> > summ_mean_int64;
Function<signals::etypVecSingle, signals::etypDouble, frame_mean<signals::etypVecSingle, double, double> > summ_mean_float;
Function<signals::etypVecDouble, signals::etypDouble, frame_mean<signals::etypVecDouble, double, double> > summ_mean_double;

Function<signals::etypVecBoolean, signals::etypBoolean, frame_median<signals::etypVecBoolean> > summ_median_bool;
Function<signals::etypVecByte, signals::etypByte, frame_median<signals::etypVecByte> > summ_median_byte;
Function<signals::etypVecShort, signals::etypShort, frame_median<signals::etypVecShort> > summ_median_short;
Function<signals::etypVecLong, signals::etypLong, frame_median<signals::etypVecLong> > summ_median_long;
Function<signals::etypVecInt64, signals::etypInt64, frame_median<signals::etypVecInt64> > summ_median_int64;
Function<signals::etypVecSingle, signals::etypSingle, frame_median<signals::etypVecSingle> > summ_median_float;
Function<signals::etypVecDouble, signals::etypDouble, frame_median<signals::etypVecDouble> > summ_median_double;

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
	&summ_mean_bool, &summ_mean_byte, &summ_mean_short, &summ_mean_long, &summ_mean_int64, &summ_mean_float, &summ_mean_double,

	// frame median
	&summ_median_bool, &summ_median_byte, &summ_median_short, &summ_median_long,
	&summ_median_int64, &summ_median_float, &summ_median_double,
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
