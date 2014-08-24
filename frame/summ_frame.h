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

template<signals::EType ET>
struct frame_max : public std::unary_function<signals::IVector*,typename StoreType<ET>::base_type>
{
	typedef signals::IVector* argument_type;
	typedef typename StoreType<ET>::base_type result_type;
	static const char* NAME;
	static const char* DESCR;

	inline result_type operator()(argument_type parm)
	{
		result_type accum;
		ASSERT(parm && parm->Type() == StoreType<ET>::base_enum);
		unsigned width = parm->Size();
		const result_type* inData = (result_type*)parm->Data();
		for(unsigned idx = 0; idx < width; ++idx)
		{
			const result_type& entry = inData[idx];
			if(!idx || accum < entry) accum = entry;
		}
		parm->Release();
		return accum;
	}
};
template<signals::EType ET> const char* frame_max<ET>::NAME = "frame max";
template<signals::EType ET> const char* frame_max<ET>::DESCR = "maximum value in the frame";

template<signals::EType ET>
struct frame_min : public std::unary_function<signals::IVector*,typename StoreType<ET>::base_type>
{
	typedef signals::IVector* argument_type;
	typedef typename StoreType<ET>::base_type result_type;
	static const char* NAME;
	static const char* DESCR;

	inline result_type operator()(argument_type parm)
	{
		result_type accum;
		ASSERT(parm && parm->Type() == StoreType<ET>::base_enum);
		unsigned width = parm->Size();
		const result_type* inData = (result_type*)parm->Data();
		for(unsigned idx = 0; idx < width; ++idx)
		{
			const result_type& entry = inData[idx];
			if(!idx || accum > entry) accum = entry;
		}
		parm->Release();
		return accum;
	}
};
template<signals::EType ET> const char* frame_min<ET>::NAME = "frame min";
template<signals::EType ET> const char* frame_min<ET>::DESCR = "minimum value in the frame";

template<signals::EType IN_TYPE, typename TEMP_TYPE, typename OUT_TYPE>
struct frame_mean : public std::unary_function<signals::IVector*,OUT_TYPE>
{
	typedef signals::IVector* argument_type;
	typedef OUT_TYPE result_type;
	typedef typename StoreType<IN_TYPE>::base_type base_type;
	static const char* NAME;
	static const char* DESCR;

	inline result_type operator()(argument_type parm)
	{
		TEMP_TYPE accum;
		ASSERT(parm && parm->Type() == StoreType<IN_TYPE>::base_enum);
		unsigned width = parm->Size();
		const base_type* inData = (base_type*)parm->Data();
		for(unsigned idx = 0; idx < width; ++idx)
		{
			const base_type& entry = inData[idx];
			if(!idx)
			{
				accum = (TEMP_TYPE)entry;
			} else {
				accum += entry;
			}
		}
		parm->Release();
		return (OUT_TYPE)(accum / width);
	}
};
template<signals::EType IN_TYPE, typename TEMP_TYPE, typename OUT_TYPE>
const char* frame_mean<IN_TYPE,TEMP_TYPE,OUT_TYPE>::NAME = "frame mean";
template<signals::EType IN_TYPE, typename TEMP_TYPE, typename OUT_TYPE>
const char* frame_mean<IN_TYPE,TEMP_TYPE,OUT_TYPE>::DESCR = "average value in the frame";

template<signals::EType ET>
struct frame_median : public std::unary_function<signals::IVector*,typename StoreType<ET>::base_type>
{
	typedef signals::IVector* argument_type;
	typedef typename StoreType<ET>::base_type result_type;
	static const char* NAME;
	static const char* DESCR;

	inline result_type operator()(argument_type parm)
	{
		ASSERT(parm && parm->Type() == StoreType<ET>::base_enum);
		unsigned width = parm->Size();
		const result_type* inData = (result_type*)parm->Data();
		typedef std::multiset<result_type> TempType;
		TempType accum;
		for(unsigned idx = 0; idx < width; ++idx)
		{
			accum.insert(inData[idx]);
		}
		parm->Release();

		unsigned med = width / 2;
		TempType::const_iterator trans = accum.begin();
		for(unsigned idx=0; idx < med; ++idx, ++trans);
		return *trans;
	}
};
template<signals::EType ET> const char* frame_median<ET>::NAME = "frame median";
template<signals::EType ET> const char* frame_median<ET>::DESCR = "median value in the frame";
