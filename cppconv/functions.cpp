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
#include "cppconv.h"
#include <functional>

// ---------------------------------------------------------------------------- class functions

#pragma warning(push)
#pragma warning(disable: 4244)
template<class PARM, class RET>
struct assign : public std::unary_function<PARM,RET>
{
	typedef PARM argument_type;
	typedef RET result_type;
	inline RET operator()(const PARM& parm) { return (RET)parm; }
};
#pragma warning(pop)

// lossless assignments
static Function<signals::etypByte,signals::etypShort,assign<unsigned char, short> > assignBS("=","byte -> short");
static Function<signals::etypByte,signals::etypLong,assign<unsigned char,long> > assignBL("=","byte -> long");
static Function<signals::etypByte,signals::etypInt64,assign<unsigned char,__int64> > assignB6("=","byte -> int64");
static Function<signals::etypByte,signals::etypSingle,assign<unsigned char,float> > assignBF("=","byte -> single");
static Function<signals::etypByte,signals::etypDouble,assign<unsigned char,double> > assignBD("=","byte -> double");
static Function<signals::etypByte,signals::etypComplex,assign<unsigned char,std::complex<float> > > assignBC("=","byte -> complex-single");
static Function<signals::etypByte,signals::etypCmplDbl,assign<unsigned char,std::complex<double> > > assignBE("=","byte -> complex-double");
static Function<signals::etypShort,signals::etypLong,assign<short,long> > assignSL("=","short -> long");
static Function<signals::etypShort,signals::etypInt64,assign<short,__int64> > assignS6("=","short -> int64");
static Function<signals::etypShort,signals::etypSingle,assign<short,float> > assignSF("=","short -> single");
static Function<signals::etypShort,signals::etypDouble,assign<short,double> > assignSD("=","short -> double");
static Function<signals::etypShort,signals::etypComplex,assign<short,std::complex<float> > > assignSC("=","short -> complex-single");
static Function<signals::etypShort,signals::etypCmplDbl,assign<short,std::complex<double> > > assignSE("=","short -> complex-double");
static Function<signals::etypLong,signals::etypInt64,assign<long,__int64> > assignL6("=","long -> int64");
static Function<signals::etypLong,signals::etypDouble,assign<long,double> > assignLD("=","long -> double");
static Function<signals::etypLong,signals::etypCmplDbl,assign<long,std::complex<double> > > assignLE("=","long -> complex-double");
static Function<signals::etypSingle,signals::etypDouble,assign<float,double> > assignFD("=","single -> double");
static Function<signals::etypSingle,signals::etypComplex,assign<float,std::complex<float> > > assignFC("=","single -> complex-single");
static Function<signals::etypSingle,signals::etypCmplDbl,assign<float,std::complex<double> > > assignFE("=","single -> complex-double");
static Function<signals::etypDouble,signals::etypCmplDbl,assign<double,std::complex<double> > > assignDE("=","double -> complex-double");
static Function<signals::etypComplex,signals::etypCmplDbl,assign<std::complex<float>, std::complex<double> > > assignCE("=","complex-single -> complex-double");

// lossy assignments
static Function<signals::etypShort,signals::etypByte,assign<short, unsigned char> > assignSB("~","short -> byte");
static Function<signals::etypLong,signals::etypByte,assign<long, unsigned char> > assignLB("~","long -> byte");
static Function<signals::etypLong,signals::etypShort,assign<long, short> > assignLS("~","long -> short");
static Function<signals::etypLong,signals::etypSingle,assign<long, float> > assignLF("~","long -> single");
static Function<signals::etypLong,signals::etypComplex,assign<long, std::complex<float> > > assignLC("~","long -> complex-single");
static Function<signals::etypInt64,signals::etypByte,assign<__int64, unsigned char> > assign6B("~","int64 -> byte");
static Function<signals::etypInt64,signals::etypShort,assign<__int64, short> > assign6S("~","int64 -> short");
static Function<signals::etypInt64,signals::etypLong,assign<__int64, long> > assign6L("~","int64 -> long");
static Function<signals::etypInt64,signals::etypSingle,assign<__int64, float> > assign6F("~","int64 -> single");
static Function<signals::etypInt64,signals::etypComplex,assign<__int64, std::complex<float> > > assign6C("~","int64 -> complex-single");
static Function<signals::etypInt64,signals::etypCmplDbl,assign<__int64, std::complex<double> > > assign6E("~","int64 -> complex-double");
static Function<signals::etypSingle,signals::etypByte,assign<float, unsigned char> > assignFB("~","single -> byte");
static Function<signals::etypSingle,signals::etypShort,assign<float, short> > assignFS("~","single -> short");
static Function<signals::etypDouble,signals::etypByte,assign<double, unsigned char> > assignDB("~","double -> byte");
static Function<signals::etypDouble,signals::etypShort,assign<double, short> > assignDS("~","double -> short");
static Function<signals::etypDouble,signals::etypLong,assign<double, long> > assignDL("~","double -> long");
static Function<signals::etypDouble,signals::etypSingle,assign<double, float> > assignDF("~","double -> single");
static Function<signals::etypDouble,signals::etypComplex,assign<double, std::complex<float> > > assignCD("~","double -> complex-single");
static Function<signals::etypCmplDbl,signals::etypComplex,assign<std::complex<double>, std::complex<float> > > assignEC("~","complex-single -> complex-double");

template<class BASE>
struct mag2 : public std::unary_function<std::complex<BASE>,double>
{
	typedef std::complex<BASE> argument_type;
	typedef double result_type;

	inline double operator()(const std::complex<BASE>& parm)
	{
		return double(parm.real()) * parm.real() * parm.imag() * parm.imag();
	}
};

template<class BASE>
struct mag : public std::unary_function<std::complex<BASE>,double>
{
	typedef std::complex<BASE> argument_type;
	typedef double result_type;

	inline double operator()(const std::complex<BASE>& parm)
	{
		return sqrt(double(parm.real()) * parm.real() * parm.imag() * parm.imag());
	}
};

// complex transforms
static Function<signals::etypComplex,signals::etypDouble,mag2<float> > mag2S("mag2","squared magnitude (complex-single)");
static Function<signals::etypCmplDbl,signals::etypDouble,mag2<double> > mag2D("mag2","squared magnitude (complex-double)");
static Function<signals::etypComplex,signals::etypDouble,mag<float> > magS("mag","squared magnitude (complex-single)");
static Function<signals::etypCmplDbl,signals::etypDouble,mag<double> > magD("mag","squared magnitude (complex-double)");

signals::IFunctionSpec* FUNCTIONS[] = {
	// lossless assignments
	&assignBS, &assignBL, &assignB6, &assignBF, &assignBD, &assignBC, &assignBE, &assignSL, &assignS6,
	&assignSF, &assignSD, &assignSC, &assignSE, &assignL6, &assignLD, &assignLE, &assignFD, &assignFC,
	&assignFE, &assignDE, &assignCE,
	
	// lossy assignments
	&assignSB, &assignLB, &assignLS, &assignLF, &assignLC, &assign6B, &assign6S, &assign6L, &assign6F,
	&assign6C, &assign6E, &assignFB, &assignFS, &assignDB, &assignDS, &assignDL, &assignDF, &assignCD,
	&assignEC,

	// complex transforms
	&mag2S, &mag2D, &magS, &magD
};

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
