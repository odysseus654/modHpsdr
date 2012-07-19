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

// cppconv.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"
#include "cppconv.h"
#include <functional>

// ---------------------------------------------------------------------------- class InputFunctionBase

BOOL InputFunctionBase::Connect(signals::IEPReceiver* recv)
{
	if(recv != m_readFrom)
	{
		if(recv) recv->onSinkConnected(this);
		if(m_readFrom) m_readFrom->onSinkDisconnected(this);
		m_readFrom = recv;
	}
	return true;
}

void InputFunctionBase::onSinkConnected(signals::IInEndpoint* src)
{
	ASSERT(!m_readTo);
	signals::IInEndpoint* oldEp(m_readTo);
	m_readTo = src;
	if(m_readTo) m_readTo->AddRef();
	if(oldEp) oldEp->Release();
}

void InputFunctionBase::onSinkDisconnected(signals::IInEndpoint* src)
{
	ASSERT(m_readTo == src);
	if(m_readTo == src)
	{
		m_readTo = NULL;
		if(src) src->Release();
	}
}

// ---------------------------------------------------------------------------- class OutputFunctionBase

BOOL OutputFunctionBase::Connect(signals::IEPSender* send)
{
	if(send != m_writeTo)
	{
		if(send) send->AddRef(this);
		if(m_writeTo) m_writeTo->Release(this);
		m_writeTo = send;
	}
	return true;
}

unsigned OutputFunctionBase::AddRef(signals::IOutEndpoint* iep)
{
	if(iep) m_writeFrom = iep;
	return m_parent->AddRef();
}

unsigned OutputFunctionBase::Release(signals::IOutEndpoint* iep)
{
	if(iep == m_writeFrom) m_writeFrom = NULL;
	return m_parent->Release();
}

// ---------------------------------------------------------------------------- class functions

template<class PARM, class RET>
struct assign : public std::unary_function<PARM,RET>
{
	typedef PARM argument_type;
	typedef RET result_type;
	inline RET operator()(PARM parm) { return (RET)parm; }
};
/*
		etypByte	= 0x0B, // 0 0001 011
		etypShort	= 0x0C, // 0 0001 100
		etypLong	= 0x0D, // 0 0001 101
		etypSingle	= 0x15, // 0 0010 101
		etypDouble	= 0x16, // 0 0010 110
		etypComplex	= 0x1D, // 0 0011 101
		etypCmplDbl = 0x1E, // 0 0011 110

		etypVecByte		= 0x8B, // 1 0001 011
		etypVecShort	= 0x8C, // 1 0001 100
		etypVecLong		= 0x8D, // 1 0001 101
		etypVecSingle	= 0x95, // 1 0010 101
		etypVecDouble	= 0x96, // 1 0010 110
		etypVecComplex	= 0x9D, // 1 0011 101
		etypVecCmplDbl	= 0x9E, // 1 0011 110
*/
/*
template<signals::EType INN, signals::EType OUTT>
struct StlFunction
{
	Function<signals::etypByte,signals::etypShort,assign<unsigned char,short> > test1("=","byte -> short");
}
*/
//Function<signals::etypByte,signals::etypShort,assign > test1("=","byte -> short");
Function<signals::etypByte,signals::etypLong,assign<unsigned char,long> > test2("=","byte -> long");
Function<signals::etypByte,signals::etypSingle,assign<unsigned char,float> > test3("=","byte -> single");
Function<signals::etypByte,signals::etypDouble,assign<unsigned char,double> > test4("=","byte -> double");
Function<signals::etypByte,signals::etypComplex,assign<unsigned char,std::complex<float> > > test5("=","byte -> complex-single");
Function<signals::etypByte,signals::etypCmplDbl,assign<unsigned char,std::complex<double> > > test6("=","byte -> complex-double");

Function<signals::etypSingle,signals::etypDouble,assign<float,double> > test7("=","single -> double");
Function<signals::etypDouble,signals::etypSingle,assign<double,float> > test8("~","double -> single");
