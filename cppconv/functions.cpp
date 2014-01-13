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
#include <funcbase.h>
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

static Function<signals::etypVecByte,signals::etypVecShort,assign<unsigned char, short> > assignVBS("=","byte -> short");
static Function<signals::etypVecByte,signals::etypVecLong,assign<unsigned char,long> > assignVBL("=","byte -> long");
static Function<signals::etypVecByte,signals::etypVecInt64,assign<unsigned char,__int64> > assignVB6("=","byte -> int64");
static Function<signals::etypVecByte,signals::etypVecSingle,assign<unsigned char,float> > assignVBF("=","byte -> single");
static Function<signals::etypVecByte,signals::etypVecDouble,assign<unsigned char,double> > assignVBD("=","byte -> double");
static Function<signals::etypVecByte,signals::etypVecComplex,assign<unsigned char,std::complex<float> > > assignVBC("=","byte -> complex-single");
static Function<signals::etypVecByte,signals::etypVecCmplDbl,assign<unsigned char,std::complex<double> > > assignVBE("=","byte -> complex-double");
static Function<signals::etypVecShort,signals::etypVecLong,assign<short,long> > assignVSL("=","short -> long");
static Function<signals::etypVecShort,signals::etypVecInt64,assign<short,__int64> > assignVS6("=","short -> int64");
static Function<signals::etypVecShort,signals::etypVecSingle,assign<short,float> > assignVSF("=","short -> single");
static Function<signals::etypVecShort,signals::etypVecDouble,assign<short,double> > assignVSD("=","short -> double");
static Function<signals::etypVecShort,signals::etypVecComplex,assign<short,std::complex<float> > > assignVSC("=","short -> complex-single");
static Function<signals::etypVecShort,signals::etypVecCmplDbl,assign<short,std::complex<double> > > assignVSE("=","short -> complex-double");
static Function<signals::etypVecLong,signals::etypVecInt64,assign<long,__int64> > assignVL6("=","long -> int64");
static Function<signals::etypVecLong,signals::etypVecDouble,assign<long,double> > assignVLD("=","long -> double");
static Function<signals::etypVecLong,signals::etypVecCmplDbl,assign<long,std::complex<double> > > assignVLE("=","long -> complex-double");
static Function<signals::etypVecSingle,signals::etypVecDouble,assign<float,double> > assignVFD("=","single -> double");
static Function<signals::etypVecSingle,signals::etypVecComplex,assign<float,std::complex<float> > > assignVFC("=","single -> complex-single");
static Function<signals::etypVecSingle,signals::etypVecCmplDbl,assign<float,std::complex<double> > > assignVFE("=","single -> complex-double");
static Function<signals::etypVecDouble,signals::etypVecCmplDbl,assign<double,std::complex<double> > > assignVDE("=","double -> complex-double");
static Function<signals::etypVecComplex,signals::etypVecCmplDbl,assign<std::complex<float>, std::complex<double> > > assignVCE("=","complex-single -> complex-double");

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

static Function<signals::etypVecShort,signals::etypVecByte,assign<short, unsigned char> > assignVSB("~","short -> byte");
static Function<signals::etypVecLong,signals::etypVecByte,assign<long, unsigned char> > assignVLB("~","long -> byte");
static Function<signals::etypVecLong,signals::etypVecShort,assign<long, short> > assignVLS("~","long -> short");
static Function<signals::etypVecLong,signals::etypVecSingle,assign<long, float> > assignVLF("~","long -> single");
static Function<signals::etypVecLong,signals::etypVecComplex,assign<long, std::complex<float> > > assignVLC("~","long -> complex-single");
static Function<signals::etypVecInt64,signals::etypVecByte,assign<__int64, unsigned char> > assignV6B("~","int64 -> byte");
static Function<signals::etypVecInt64,signals::etypVecShort,assign<__int64, short> > assignV6S("~","int64 -> short");
static Function<signals::etypVecInt64,signals::etypVecLong,assign<__int64, long> > assignV6L("~","int64 -> long");
static Function<signals::etypVecInt64,signals::etypVecSingle,assign<__int64, float> > assignV6F("~","int64 -> single");
static Function<signals::etypVecInt64,signals::etypVecComplex,assign<__int64, std::complex<float> > > assignV6C("~","int64 -> complex-single");
static Function<signals::etypVecInt64,signals::etypVecCmplDbl,assign<__int64, std::complex<double> > > assignV6E("~","int64 -> complex-double");
static Function<signals::etypVecSingle,signals::etypVecByte,assign<float, unsigned char> > assignVFB("~","single -> byte");
static Function<signals::etypVecSingle,signals::etypVecShort,assign<float, short> > assignVFS("~","single -> short");
static Function<signals::etypVecDouble,signals::etypVecByte,assign<double, unsigned char> > assignVDB("~","double -> byte");
static Function<signals::etypVecDouble,signals::etypVecShort,assign<double, short> > assignVDS("~","double -> short");
static Function<signals::etypVecDouble,signals::etypVecLong,assign<double, long> > assignVDL("~","double -> long");
static Function<signals::etypVecDouble,signals::etypVecSingle,assign<double, float> > assignVDF("~","double -> single");
static Function<signals::etypVecDouble,signals::etypVecComplex,assign<double, std::complex<float> > > assignVCD("~","double -> complex-single");
static Function<signals::etypVecCmplDbl,signals::etypVecComplex,assign<std::complex<double>, std::complex<float> > > assignVEC("~","complex-single -> complex-double");

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

template<class BASE>
struct pick_real : public std::unary_function<std::complex<BASE>,BASE>
{
	typedef std::complex<BASE> argument_type;
	typedef BASE result_type;

	inline BASE operator()(const std::complex<BASE>& parm)
	{
		return parm.real();
	}
};

template<class BASE>
struct pick_imag : public std::unary_function<std::complex<BASE>,BASE>
{
	typedef std::complex<BASE> argument_type;
	typedef BASE result_type;

	inline BASE operator()(const std::complex<BASE>& parm)
	{
		return parm.imag();
	}
};

struct func_log10 : public std::unary_function<double,double>
{
	typedef double argument_type;
	typedef double result_type;

	inline double operator()(double parm)
	{
		return parm <= 0 ? -1023.0 : log10(parm);
	}
};

struct func_db : public std::unary_function<double,double>
{
	typedef double argument_type;
	typedef double result_type;

	inline double operator()(double parm)
	{
		return parm <= 0 ? -3100.0 : 10.0 * log10(parm);
	}
};

// complex transforms
static Function<signals::etypComplex,signals::etypDouble,mag2<float> > mag2S("mag^2","squared magnitude (complex-single)");
static Function<signals::etypCmplDbl,signals::etypDouble,mag2<double> > mag2D("mag^2","squared magnitude (complex-double)");
static Function<signals::etypComplex,signals::etypDouble,mag<float> > magS("mag","magnitude (complex-single)");
static Function<signals::etypCmplDbl,signals::etypDouble,mag<double> > magD("mag","magnitude (complex-double)");

static Function<signals::etypComplex,signals::etypSingle,pick_real<float> > prS("pick real","real component (complex-single)");
static Function<signals::etypCmplDbl,signals::etypDouble,pick_real<double> > prD("pick real","real component (complex-double)");
static Function<signals::etypComplex,signals::etypSingle,pick_imag<float> > piS("pick imag","imaginary component (complex-single)");
static Function<signals::etypCmplDbl,signals::etypDouble,pick_imag<double> > piD("pick imag","imaginary component (complex-double)");

static Function<signals::etypDouble,signals::etypDouble,func_log10> log10D("log10","common logarithm");
static Function<signals::etypDouble,signals::etypDouble,func_db> decibelD("dB","decibels");

static Function<signals::etypVecComplex,signals::etypVecDouble,mag2<float> > magV2S("mag^2","squared magnitude (complex-single)");
static Function<signals::etypVecCmplDbl,signals::etypVecDouble,mag2<double> > magV2D("mag^2","squared magnitude (complex-double)");
static Function<signals::etypVecComplex,signals::etypVecDouble,mag<float> > magVS("mag","magnitude (complex-single)");
static Function<signals::etypVecCmplDbl,signals::etypVecDouble,mag<double> > magVD("mag","magnitude (complex-double)");

static Function<signals::etypVecComplex,signals::etypVecSingle,pick_real<float> > prVS("pick real","real component (complex-single)");
static Function<signals::etypVecCmplDbl,signals::etypVecDouble,pick_real<double> > prVD("pick real","real component (complex-double)");
static Function<signals::etypVecComplex,signals::etypVecSingle,pick_imag<float> > piVS("pick imag","imaginary component (complex-single)");
static Function<signals::etypVecCmplDbl,signals::etypVecDouble,pick_imag<double> > piVD("pick imag","imaginary component (complex-double)");

static Function<signals::etypVecDouble,signals::etypVecDouble,func_log10> log10VD("log10","common logarithm");
static Function<signals::etypVecDouble,signals::etypVecDouble,func_db> decibelVD("dB","decibels");

signals::IFunctionSpec* FUNCTIONS[] = {
	// lossless assignments
	&assignBS, &assignBL, &assignB6, &assignBF, &assignBD, &assignBC, &assignBE, &assignSL, &assignS6,
	&assignSF, &assignSD, &assignSC, &assignSE, &assignL6, &assignLD, &assignLE, &assignFD, &assignFC,
	&assignFE, &assignDE, &assignCE,
	
	&assignVBS, &assignVBL, &assignVB6, &assignVBF, &assignVBD, &assignVBC, &assignVBE, &assignVSL, &assignVS6,
	&assignVSF, &assignVSD, &assignVSC, &assignVSE, &assignVL6, &assignVLD, &assignVLE, &assignVFD, &assignVFC,
	&assignVFE, &assignVDE, &assignVCE,

	// lossy assignments
	&assignSB, &assignLB, &assignLS, &assignLF, &assignLC, &assign6B, &assign6S, &assign6L, &assign6F,
	&assign6C, &assign6E, &assignFB, &assignFS, &assignDB, &assignDS, &assignDL, &assignDF, &assignCD,
	&assignEC,

	&assignVSB, &assignVLB, &assignVLS, &assignVLF, &assignVLC, &assignV6B, &assignV6S, &assignV6L, &assignV6F,
	&assignV6C, &assignV6E, &assignVFB, &assignVFS, &assignVDB, &assignVDS, &assignVDL, &assignVDF, &assignVCD,
	&assignVEC,

	// complex transforms
	&mag2S, &mag2D, &magS, &magD, &prS, &prD, &piS, &piD,
	&magV2S, &magV2D, &magVS, &magVD, &prVS, &prVD, &piVS, &piVD,

	// logarithms
	&log10D, &log10VD, &decibelD, &decibelVD
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
