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
#pragma once
#include <blockImpl.h>

class InputFunctionBase : public signals::IInputFunction
{
protected:
	signals::IFunction* m_parent;
	signals::IFunctionSpec* m_spec;
	signals::IEPReceiver* m_readFrom;
	signals::IInEndpoint* m_readTo;

public:
	inline InputFunctionBase(signals::IFunction* parent, signals::IFunctionSpec* spec)
		:m_spec(spec),m_parent(parent),m_readFrom(NULL),m_readTo(NULL) {}

public: // IInEndpoint implementaton
	virtual unsigned AddRef()		{ return m_parent->AddRef(); }
	virtual unsigned Release()		{ return m_parent->Release(); }
	virtual const char* EPName()	{ return m_spec->Name(); }
	virtual const char* EPDescr()	{ return m_spec->Description(); }
//	virtual signals::EType Type() = 0;
	virtual BOOL isConnected()		{ return !!m_readFrom; }
	virtual BOOL Disconnect()		{ return Connect(NULL); }
//	virtual signals::IEPBuffer* CreateBuffer() = 0;
	virtual signals::IAttributes* Attributes() { return m_readTo ? m_readTo->Attributes() : NULL; }
	virtual BOOL Connect(signals::IEPReceiver* recv);

public: // IEPReceiver implementaton
//	virtual unsigned Read(signals::EType type, void* buffer, unsigned numAvail, BOOL bFillAll, unsigned msTimeout) = 0;
	virtual void onSinkConnected(signals::IInEndpoint* src);
	virtual void onSinkDisconnected(signals::IInEndpoint* src);
};

class OutputFunctionBase : public signals::IOutputFunction
{
protected:
	signals::IFunction* m_parent;
	signals::IFunctionSpec* m_spec;
	signals::IEPSender* m_writeTo;
	signals::IOutEndpoint* m_writeFrom;

public:
	inline OutputFunctionBase(signals::IFunction* parent, signals::IFunctionSpec* spec)
		:m_spec(spec),m_parent(parent),m_writeTo(NULL),m_writeFrom(NULL) {}

public: // IOutEndpoint implementaton
	virtual const char* EPName()	{ return m_spec->Name(); }
	virtual const char* EPDescr()	{ return m_spec->Description(); }
//	virtual signals::EType Type() = 0;
	virtual BOOL isConnected()		{ return !!m_writeTo; }
	virtual BOOL Disconnect()		{ return Connect(NULL); }
//	virtual signals::IEPBuffer* CreateBuffer() = 0;
	virtual signals::IAttributes* Attributes() { return m_writeFrom ? m_writeFrom->Attributes() : NULL; }
	virtual BOOL Connect(signals::IEPSender* send);

public: // IEPSender implementaton
//	virtual unsigned Write(signals::EType type, const void* buffer, unsigned numElem, unsigned msTimeout) = 0;
	virtual unsigned AddRef(signals::IOutEndpoint* iep);
	virtual unsigned Release(signals::IOutEndpoint* iep);
};

#pragma warning(disable: 4355)
template<signals::EType INN, signals::EType OUTT, typename OPER>
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
	virtual const unsigned char* Fingerprint() { return FINGERPRINT; }

	virtual signals::IFunction* Create()
	{
		signals::IFunction* func = new Instance(this);
		func->AddRef();
		return func;
	}

protected:
	class InputFunction : public InputFunctionBase
	{
	protected:
		typedef std::vector<in_type> TInBuffer;
		TInBuffer m_buffer;

	public:
		inline InputFunction(Instance* parent, my_type* spec)
			:InputFunctionBase(parent, spec) {}

	public: // IInEndpoint implementaton
		virtual signals::EType Type()	{ return INN; }

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

	public: // IEPReceiver implementaton
		virtual unsigned Read(signals::EType type, void* buffer, unsigned numAvail, BOOL bFillAll, unsigned msTimeout)
		{
			ASSERT(type == OUTT);
			if(!m_readFrom) return 0;
			out_type* nativeBuff = (out_type*)buffer;

			if(m_buffer.size() < numAvail) m_buffer.resize(numAvail);
			unsigned numElem = m_readFrom->Read(INN, &m_buffer[0], numAvail, bFillAll, msTimeout);
			OPER& oper(((my_type*)m_spec)->m_oper);
			for(unsigned idx=0; idx < numElem; idx++)
			{
				nativeBuff[idx] = oper(m_buffer[idx]);
			}
			return numElem;
		}
	};

	class OutputFunction : public OutputFunctionBase
	{
	protected:
		typedef std::vector<out_type> TOutBuffer;
		TOutBuffer m_buffer;

	public:
		inline OutputFunction(Instance* parent, my_type* spec)
			:OutputFunctionBase(parent, spec) {}

	public: // IOutEndpoint implementaton
		virtual signals::EType Type()	{ return OUTT; }

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

	public: // IEPSender implementaton
		virtual unsigned Write(signals::EType type, const void* buffer, unsigned numElem, unsigned msTimeout)
		{
			ASSERT(type == INN);
			if(!m_writeTo) return 0;
			in_type* nativeBuff = (in_type*)buffer;

			if(m_buffer.size() < numElem) m_buffer.resize(numElem);
			OPER& oper(((my_type*)m_spec)->m_oper);
			for(unsigned idx=0; idx < numElem; idx++)
			{
				m_buffer[idx] = oper(nativeBuff[idx]);
			}
			return m_writeTo->Write(OUTT, &m_buffer[0], numElem, msTimeout);
		}
	};

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

protected:
	OPER m_oper;
	const char* m_name;
	const char* m_descr;
	static const unsigned char FINGERPRINT[];
};

template<signals::EType INN, signals::EType OUTT, typename OPER>
const unsigned char Function<INN,OUTT,OPER>::FINGERPRINT[] = { (unsigned char)INN, (unsigned char)OUTT };
