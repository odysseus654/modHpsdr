/*
	Copyright 2013 Erik Anderson

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

template<signals::EType ET, typename OPER>
class FrameFunctionBase : public FunctionBase<ET,ET,FrameFunctionBase<ET,OPER> >
{
private:
	typedef FunctionBase<ET,ET,FrameFunctionBase<ET,OPER> > my_base;

protected:
	class InputFunction : public my_base::InputFunction<OPER>, public signals::IAttributeObserver
	{
	private:
		typedef typename my_base::InputFunction<OPER> inp_base;
	protected:
		OPER m_oper;
	public:
		inline InputFunction(signals::IFunction* parent, signals::IFunctionSpec* spec)
			:inp_base(parent, spec, m_oper),m_lastWidthAttr(NULL) {}

	protected:
		signals::IAttribute* m_lastWidthAttr;

	public: // virtual overrides
		virtual BOOL Connect(signals::IEPRecvFrom* recv)
		{
			if(recv != m_readFrom)
			{
				if(m_lastWidthAttr != NULL)
				{
					m_lastWidthAttr->Unobserve(this);
					m_lastWidthAttr = NULL;
				}
				inp_base::Connect(recv);
				if(m_readFrom && !m_lastWidthAttr) establishAttributes();
			}
			return TRUE;
		}

		void establishAttributes()
		{
			signals::IAttributes* attrs = m_readFrom->OutputAttributes();
			if(attrs)
			{
				signals::IAttribute* attr = attrs->GetByName("blockSize");
				if(attr && (attr->Type() == signals::etypShort || attr->Type() == signals::etypLong))
				{
					m_lastWidthAttr = attr;
					m_lastWidthAttr->Observe(this);
					OnChanged(attr, attr->getValue());
				}
			}
		}

		virtual unsigned Read(signals::EType type, void* buffer, unsigned numAvail, BOOL bFillAll, unsigned msTimeout)
		{
			if(!m_readFrom) return 0;
			if(!m_lastWidthAttr) establishAttributes();
			return inp_base::Read(type, buffer, numAvail, bFillAll, msTimeout);
		}

	public: // IAttributeObserver implementation
		virtual void OnChanged(signals::IAttribute* attr, const void* value)
		{
			if(attr == m_lastWidthAttr)
			{
				long width;
				if(attr->Type() == signals::etypShort)
				{
					width = *(short*)value;
				} else {
					width = *(long*)value;
				}
				m_oper.setWidth(width);
			}
		}

		virtual void OnDetached(signals::IAttribute* attr)
		{
			if(attr == m_lastWidthAttr)
			{
				m_lastWidthAttr = NULL;
			}
			else ASSERT(FALSE);
		}
	};

	class OutputFunction : public my_base::OutputFunction<OPER>, public signals::IAttributeObserver
	{
	private:
		typedef typename my_base::OutputFunction<OPER> out_base;
	protected:
		OPER m_oper;
	public:
		inline OutputFunction(signals::IFunction* parent, signals::IFunctionSpec* spec)
			:out_base(parent, spec, m_oper),m_lastWidthAttr(NULL) {}

	protected:
		signals::IAttribute* m_lastWidthAttr;

	public: // virtual overrides
		virtual unsigned AddRef(signals::IOutEndpoint* iep)
		{
			if(iep != m_writeFrom && m_lastWidthAttr != NULL)
			{
				m_lastWidthAttr->Unobserve(this);
				m_lastWidthAttr = NULL;
			}
			unsigned ret = out_base::AddRef(iep);
			if(m_writeFrom && !m_lastWidthAttr) establishAttributes();
			return ret;
		}

		void establishAttributes()
		{
			signals::IAttributes* attrs = m_writeFrom->Attributes();
			if(attrs)
			{
				signals::IAttribute* attr = attrs->GetByName("blockSize");
				if(attr && (attr->Type() == signals::etypShort || attr->Type() == signals::etypLong))
				{
					m_lastWidthAttr = attr;
					m_lastWidthAttr->Observe(this);
					OnChanged(attr, attr->getValue());
				}
			}
		}

		virtual unsigned Write(signals::EType type, const void* buffer, unsigned numElem, unsigned msTimeout)
		{
			if(!m_writeTo) return 0;
			if(!m_lastWidthAttr) establishAttributes();
			return out_base::Write(type, buffer, numElem, msTimeout);
		}

		virtual unsigned Release(signals::IOutEndpoint* iep)
		{
			if(m_lastWidthAttr != NULL)
			{
				m_lastWidthAttr->Unobserve(this);
				m_lastWidthAttr = NULL;
			}
			return out_base::Release(iep);
		}

	public: // IAttributeObserver implementation
		virtual void OnChanged(signals::IAttribute* attr, const void* value)
		{
			if(attr == m_lastWidthAttr)
			{
				long width;
				if(attr->Type() == signals::etypShort)
				{
					width = *(short*)value;
				} else {
					width = *(long*)value;
				}
				m_oper.setWidth(width);
			}
		}

		virtual void OnDetached(signals::IAttribute* attr)
		{
			if(attr == m_lastWidthAttr)
			{
				m_lastWidthAttr = NULL;
			}
			else ASSERT(FALSE);
		}
	};

	class Instance : public InstanceBase
	{
	public:
		inline Instance(FrameFunctionBase* spec)
			:InstanceBase(spec),m_input(this,spec),m_output(this,spec)
		{}

		virtual signals::IInputFunction* Input()		{ AddRef(); return &m_input; }
		virtual signals::IOutputFunction* Output()		{ AddRef(); return &m_output; }

	protected:
		InputFunction m_input;
		OutputFunction m_output;
	};

public:
	inline FrameFunctionBase(const char* name, const char* descr)
		:FunctionBase(name, descr)
	{}

	inline FrameFunctionBase()
		:FunctionBase(OPER::NAME, OPER::DESCR)
	{}

	signals::IFunction* CreateImpl()
	{
		return new Instance(this);
	}
};

template<typename in_type, typename width_type = long, typename out_type = in_type>
struct DivideByN : public std::unary_function<in_type,out_type>
{
	typedef in_type argument_type;
	typedef out_type result_type;
	volatile long m_width;
	static const char* NAME;
	static const char* DESCR;

	inline DivideByN():m_width(0) {}

	inline out_type operator()(const in_type& parm)
	{
		return (out_type)(m_width == 0 ? parm : parm / (width_type)m_width);
	}

	void setWidth(long width)
	{
		InterlockedExchange(&m_width, width);
	}
};

// ------------------------------------------------------------------------------------------------

template<typename in_type, typename width_type, typename out_type>
const char * DivideByN<in_type,width_type,out_type>::NAME = "divide by N";

template<typename in_type, typename width_type, typename out_type>
const char * DivideByN<in_type,width_type,out_type>::DESCR = "Adjust magnitudes after an FFT operation";
