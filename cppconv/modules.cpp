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
#include "identity.h"

CSplitterDriver<signals::etypBoolean> split_bool;
CSplitterDriver<signals::etypByte> split_byte;
CSplitterDriver<signals::etypShort> split_short;
CSplitterDriver<signals::etypLong> split_long;
CSplitterDriver<signals::etypInt64> split_int64;
CSplitterDriver<signals::etypSingle> split_float;
CSplitterDriver<signals::etypDouble> split_double;
CSplitterDriver<signals::etypComplex> split_cpx;
CSplitterDriver<signals::etypCmplDbl> split_cpxdbl;
CSplitterDriver<signals::etypLRSingle> split_lr;
CSplitterDriver<signals::etypVecBoolean> split_vec_bool;
CSplitterDriver<signals::etypVecByte> split_vec_byte;
CSplitterDriver<signals::etypVecShort> split_vec_short;
CSplitterDriver<signals::etypVecLong> split_vec_long;
CSplitterDriver<signals::etypVecInt64> split_vec_int64;
CSplitterDriver<signals::etypVecSingle> split_vec_float;
CSplitterDriver<signals::etypVecDouble> split_vec_double;
CSplitterDriver<signals::etypVecComplex> split_vec_cpx;
CSplitterDriver<signals::etypVecCmplDbl> split_vec_cpxdbl;
CSplitterDriver<signals::etypVecLRSingle> split_vec_lr;

CIdentityDriver<signals::etypBoolean> ident_bool;
CIdentityDriver<signals::etypByte> ident_byte;
CIdentityDriver<signals::etypShort> ident_short;
CIdentityDriver<signals::etypLong> ident_long;
CIdentityDriver<signals::etypInt64> ident_int64;
CIdentityDriver<signals::etypSingle> ident_float;
CIdentityDriver<signals::etypDouble> ident_double;
CIdentityDriver<signals::etypComplex> ident_cpx;
CIdentityDriver<signals::etypCmplDbl> ident_cpxdbl;
CIdentityDriver<signals::etypLRSingle> ident_lr;
CIdentityDriver<signals::etypVecBoolean> ident_vec_bool;
CIdentityDriver<signals::etypVecByte> ident_vec_byte;
CIdentityDriver<signals::etypVecShort> ident_vec_short;
CIdentityDriver<signals::etypVecLong> ident_vec_long;
CIdentityDriver<signals::etypVecInt64> ident_vec_int64;
CIdentityDriver<signals::etypVecSingle> ident_vec_float;
CIdentityDriver<signals::etypVecDouble> ident_vec_double;
CIdentityDriver<signals::etypVecComplex> ident_vec_cpx;
CIdentityDriver<signals::etypVecCmplDbl> ident_vec_cpxdbl;
CIdentityDriver<signals::etypVecLRSingle> ident_vec_lr;

signals::IBlockDriver* BLOCKS[] =
{
	// stream split
	&split_bool, &split_byte, &split_short, &split_long, &split_int64, &split_float, &split_double,
	&split_cpx, &split_cpxdbl, &split_lr,
	&split_vec_bool, &split_vec_byte, &split_vec_short, &split_vec_long, &split_vec_int64, &split_vec_float, &split_vec_double,
	&split_vec_cpx, &split_vec_cpxdbl, &split_vec_lr,

	// identity
	&ident_bool, &ident_byte, &ident_short, &ident_long, &ident_int64, &ident_float, &ident_double,
	&ident_cpx, &ident_cpxdbl, &ident_lr,
	&ident_vec_bool, &ident_vec_byte, &ident_vec_short, &ident_vec_long, &ident_vec_int64, &ident_vec_float, &ident_vec_double,
	&ident_vec_cpx, &ident_vec_cpxdbl, &ident_vec_lr
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
