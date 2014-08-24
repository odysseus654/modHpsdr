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
#pragma once
#include <funcbase.h>

template<signals::EType ET, typename denomType = unsigned>
struct DivideByN : public std::unary_function<signals::IVector*,signals::IVector*>
{
	typedef signals::IVector* argument_type;
	typedef signals::IVector* result_type;
	typedef typename StoreType<ET>::buffer_templ buffer_templ;
	typedef typename StoreType<ET>::base_type base_type;
	static const char* NAME;
	static const char* DESCR;

	inline result_type operator()(argument_type parm)
	{
		if(!parm || parm->Type() != StoreType<ET>::base_enum)
		{
			ASSERT(FALSE);
			return parm;
		}
		unsigned width = parm->Size();
		buffer_templ* out = buffer_templ::retrieve(width);
		const base_type* inData = (base_type*)parm->Data();
		base_type* outData = out->data;
		for(unsigned idx = 0; idx < width; ++idx)
		{
			outData[idx] = inData[idx] / (denomType)width;
		}
		parm->Release();
		return out;
	}
};

// ------------------------------------------------------------------------------------------------

template<signals::EType ET, typename denomType>
const char * DivideByN<ET,denomType>::NAME = "divide by N";

template<signals::EType ET, typename denomType>
const char * DivideByN<ET,denomType>::DESCR = "Adjust magnitudes after an FFT operation";
