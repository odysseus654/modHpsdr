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
#include <blockImpl.h>
#include <functional>

namespace signals
{
	__interface IFunctionSpec;
	__interface IFunction;
	__interface IInputFunction;
	__interface IOutputFunction;

	__interface IFunctionSpec
	{
		const char* Name();
		const char* Description();
		IFunction* Create();
	};

	__interface IFunction
	{
		IFunctionSpec* Spec();
		unsigned AddRef();
		unsigned Release();
		IInputFunction* Input();
		IOutputFunction* Output();
	};

	__interface IInputFunction : public IInEndpoint, public IEPReceiver
	{
	};

	__interface IOutputFunction : public IOutEndpoint, public IEPSender
	{
	};
}

#pragma warning(disable: 4355)
template<signals::EType INN, signals::EType OUTT, class OPER>
//	std::unary_function<typename StoreType<in>::type, typename StoreType<out>::type> oper
class Function : public signals::IFunctionSpec
{
protected:
	class Instance;

	typedef Function<INN, OUTT, OPER> my_type;
	typedef typename StoreType<INN>::type in_type;
	typedef typename StoreType<OUTT>::type out_type;

public:
	inline Function(const char* name, const char* descr, OPER oper = OPER())
		:m_name(name),m_descr(descr),m_oper(oper)
	{}

	virtual const char* Name()			{ return m_name; }
	virtual const char* Description()	{ return m_descr; }

	virtual signals::IFunction* Create()
	{
		signals::IFunction* func = new Instance(this);
		func->AddRef();
		return func;
	}

protected:
	class InputFunction;
	class OutputFunction;

	class Instance : public CRefcountObject, public signals::IFunction
	{
	public:
		inline Instance(Function* spec)
			:m_spec(spec),m_input(this,spec),m_output(this,spec)
		{}

		virtual signals::IFunctionSpec* Spec()			{ return m_spec; }
		virtual unsigned AddRef()						{ return CRefcountObject::AddRef(); }
		virtual unsigned Release()						{ return CRefcountObject::Release(); }
		virtual signals::IInputFunction* Input()		{ AddRef(); return &m_input; }
		virtual signals::IOutputFunction* Output()		{ AddRef(); return &m_output; }

	protected:
		Function* m_spec;
		InputFunction m_input;
		OutputFunction m_output;
	};

	class InputFunction : public signals::IInputFunction
	{
	protected:
		typedef std::vector<in_type> TInBuffer;
		TInBuffer m_buffer;
		my_type* m_spec;
		Instance* m_parent;
		signals::IEPReceiver* m_readFrom;
		signals::IInEndpoint* m_readTo;

	public:
		inline InputFunction(Instance* parent, my_type* spec)
			:m_spec(spec),m_parent(parent),m_readFrom(NULL),m_readTo(NULL) {}

	public: // IInEndpoint implementaton
		virtual unsigned AddRef()		{ return m_parent->AddRef(); }
		virtual unsigned Release()		{ return m_parent->Release(); }
		virtual const char* EPName()	{ return m_spec->m_descr; }
		virtual signals::EType Type()	{ return INN; }
		virtual BOOL isConnected()		{ return !!m_readFrom; }
		virtual BOOL Disconnect()		{ return Connect(NULL); }

		virtual signals::IEPBuffer* CreateBuffer()
		{
			if(!m_readTo) return NULL;
			signals::IEPBuffer* buff = m_readTo->CreateBuffer();
			if(!buff) return NULL;
			ASSERT(buff->Type() == OUTT);
			unsigned capacity = buff->Capacity();
			buff->Release(NULL);

			buff = new CEPBuffer<INN>(capacity);
			buff->AddRef(NULL);
			return buff;
		}

		virtual signals::IAttributes* Attributes()
		{
			return m_readTo ? m_readTo->Attributes() : NULL;
		}

		virtual BOOL Connect(signals::IEPReceiver* recv)
		{
			if(recv != m_readFrom)
			{
				if(recv) recv->onSinkConnected(this);
				if(m_readFrom) m_readFrom->onSinkDisconnected(this);
				m_readFrom = recv;
			}
			return true;
		}

	public: // IEPReceiver implementaton
		virtual unsigned Read(signals::EType type, void* buffer, unsigned numAvail, BOOL bFillAll, unsigned msTimeout)
		{
			ASSERT(type == OUTT);
			if(!m_readFrom) return 0;
			out_type* nativeBuff = (out_type*)buffer;

			if(m_buffer.size() < numAvail) m_buffer.resize(numAvail);
			unsigned numElem = m_readFrom->Read(INN, &m_buffer[0], numAvail, bFillAll, msTimeout);
			OPER& oper(m_spec->m_oper);
			for(unsigned idx=0; idx < numElem; idx++)
			{
				nativeBuff[idx] = oper(m_buffer[idx]);
			}
			return numElem;
		}

		virtual void onSinkConnected(signals::IInEndpoint* src)
		{
			ASSERT(!m_readTo);
			signals::IInEndpoint* oldEp(m_readTo);
			m_readTo = src;
			if(m_readTo) m_readTo->AddRef();
			if(oldEp) oldEp->Release();
		}

		virtual void onSinkDisconnected(signals::IInEndpoint* src)
		{
			ASSERT(m_readTo == src);
			if(m_readTo == src)
			{
				m_readTo = NULL;
				if(src) src->Release();
			}
		}
	};

	class OutputFunction : public signals::IOutputFunction
	{
	protected:
		typedef std::vector<out_type> TOutBuffer;
		TOutBuffer m_buffer;
		my_type* m_spec;
		Instance* m_parent;
		signals::IEPSender* m_writeTo;
		signals::IOutEndpoint* m_writeFrom;

	public:
		inline OutputFunction(Instance* parent, my_type* spec)
			:m_spec(spec),m_parent(parent),m_writeTo(NULL),m_writeFrom(NULL) {}

	public: // IOutEndpoint implementaton
		virtual const char* EPName()	{ return m_spec->m_descr; }
		virtual signals::EType Type()	{ return OUTT; }
		virtual BOOL isConnected()		{ return !!m_writeTo; }
		virtual BOOL Disconnect()		{ return Connect(NULL); }

		virtual signals::IEPBuffer* CreateBuffer()
		{
			if(!m_writeFrom) return NULL;
			signals::IEPBuffer* buff = m_writeFrom->CreateBuffer();
			if(!buff) return NULL;
			ASSERT(buff->Type() == INN);
			unsigned capacity = buff->Capacity();
			buff->Release(NULL);
			buff = new CEPBuffer<OUTT>(capacity);
			buff->AddRef(NULL);
			return buff;
		}

		virtual signals::IAttributes* Attributes()
		{
			return m_writeFrom ? m_writeFrom->Attributes() : NULL;
		}

		virtual BOOL Connect(signals::IEPSender* send)
		{
			if(send != m_writeTo)
			{
				if(send) send->AddRef(this);
				if(m_writeTo) m_writeTo->Release(this);
				m_writeTo = send;
			}
			return true;
		}

	public: // IEPSender implementaton
		virtual unsigned Write(signals::EType type, const void* buffer, unsigned numElem, unsigned msTimeout)
		{
			ASSERT(type == INN);
			if(!m_writeTo) return 0;
			in_type* nativeBuff = (in_type*)buffer;

			if(m_buffer.size() < numElem) m_buffer.resize(numElem);
			OPER& oper(m_spec->m_oper);
			for(unsigned idx=0; idx < numElem; idx++)
			{
				m_buffer[idx] = oper(nativeBuff[idx]);
			}
			return m_writeTo->Write(OUTT, &m_buffer[0], numElem, msTimeout);
		}

		virtual unsigned AddRef(signals::IOutEndpoint* iep)
		{
			if(iep) m_writeFrom = iep;
			return m_parent->AddRef();
		}

		virtual unsigned Release(signals::IOutEndpoint* iep)
		{
			if(iep == m_writeFrom) m_writeFrom = NULL;
			return m_parent->Release();
		}
	};

protected:
	OPER m_oper;
	const char* m_name;
	const char* m_descr;
};

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
Function<signals::etypByte,signals::etypShort,assign<unsigned char,short> > test1("=","byte -> short");
Function<signals::etypByte,signals::etypLong,assign<unsigned char,long> > test2("=","byte -> long");
Function<signals::etypByte,signals::etypSingle,assign<unsigned char,float> > test3("=","byte -> single");
Function<signals::etypByte,signals::etypDouble,assign<unsigned char,double> > test4("=","byte -> double");
Function<signals::etypByte,signals::etypComplex,assign<unsigned char,std::complex<float> > > test5("=","byte -> complex-single");
Function<signals::etypByte,signals::etypCmplDbl,assign<unsigned char,std::complex<double> > > test6("=","byte -> complex-double");

Function<signals::etypSingle,signals::etypDouble,assign<float,double> > test7("=","single -> double");
Function<signals::etypDouble,signals::etypSingle,assign<double,float> > test8("~","double -> single");
