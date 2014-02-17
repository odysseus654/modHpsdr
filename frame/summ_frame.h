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

template<signals::EType OUT_TYPE, typename OPER, signals::EType IN_TYPE = signals::EType(OUT_TYPE + 8) >
class CFrameSummary : public FunctionBase<IN_TYPE,OUT_TYPE,CFrameSummary<OUT_TYPE,OPER,IN_TYPE> >
{
protected:
	class InputFunction : public InputFunctionBase, public signals::IAttributeObserver
	{
	protected:
		typedef typename OPER::temp_type TEMP_TYPE;
		typedef std::vector<in_type> TInBuffer;
		signals::IAttribute* m_frameSizeAttr;
		TInBuffer m_buffer;
		OPER& m_oper;
		unsigned m_blockSize;

	public:
		inline InputFunction(signals::IFunction* parent, signals::IFunctionSpec* spec, OPER& oper)
			:InputFunctionBase(parent, spec),m_oper(oper),m_frameSizeAttr(NULL),m_blockSize(0) {}

		~InputFunction()
		{
			if(m_frameSizeAttr)
			{
				m_frameSizeAttr->Unobserve(this);
				m_frameSizeAttr = NULL;
			}
		}

	public: // IInEndpoint implementaton
		virtual signals::EType Type()	{ return IN_TYPE; }

	public: // IAttributeObserver implementation
		virtual void OnChanged(signals::IAttribute *attr,const void *val)
		{
			if(attr == m_frameSizeAttr)
			{
				switch(m_frameSizeAttr->Type())
				{
				case signals::etypByte:
					m_blockSize = *(unsigned char*)val;
					break;
				case signals::etypShort:
					m_blockSize = max(0, *(short*)val);
					break;
				case signals::etypLong:
					m_blockSize = max(0, *(long*)val);
					break;
				case signals::etypInt64:
					m_blockSize = (unsigned)max(0, *(__int64*)val);
					break;
				}
			}
		}

		virtual void OnDetached(signals::IAttribute *attr)
		{
			if(attr == m_frameSizeAttr && !attr)
			{
				m_frameSizeAttr->Unobserve(this);
				m_blockSize = 0;
				m_frameSizeAttr = NULL;
			}
		}

	public: // IEPReceiver implementaton
		virtual unsigned Read(signals::EType type, void* buffer, unsigned parentNumAvail, BOOL, unsigned msTimeout)
		{
			ASSERT(type == OUT_TYPE);
			if(!m_readFrom || !parentNumAvail) return 0;

			if(!m_frameSizeAttr)
			{
				signals::IAttributes* attrs = m_readFrom->OutputAttributes();
				if(attrs)
				{
					signals::IAttribute* attr = attrs->GetByName("blockSize");
					if(attr)
					{
						m_frameSizeAttr = attr;
						attr->Observe(this);
						OnChanged(attr, attr->getValue());
					}
				}
				if(!m_frameSizeAttr) return 0;
			}
			if(!m_blockSize) return 0;

			if(m_buffer.size() < m_blockSize) m_buffer.resize(m_blockSize);
			unsigned numElem = m_readFrom->Read(IN_TYPE, &m_buffer[0], m_buffer.size(), FALSE, msTimeout);
			if(!numElem) return 0;
			TEMP_TYPE accum;
			m_oper.init(accum);
			for(unsigned idx=0; idx < numElem; idx++)
			{
				m_oper.iter(accum, m_buffer[idx], idx);
			}
			*(out_type*)buffer = m_oper.finish(accum, numElem);
			return 1;
		}

		virtual signals::IEPBuffer* CreateBuffer()
		{
			if(!m_readFrom) return NULL;
			signals::IEPBuffer* buff = m_readFrom->CreateBuffer();
			if(!buff) return NULL;
			ASSERT(buff->Type() == IN_TYPE);
			unsigned capacity = buff->Capacity();
			buff->Release(NULL);

			buff = new CEPBuffer<OUT_TYPE>(capacity);
			buff->AddRef(NULL);
			return buff;
		}
	};

	class OutputFunction : public OutputFunctionBase
	{
	protected:
		typedef typename OPER::temp_type TEMP_TYPE;
		OPER& m_oper;

	public:
		inline OutputFunction(signals::IFunction* parent, signals::IFunctionSpec* spec, OPER& oper)
			:OutputFunctionBase(parent, spec), m_oper(oper) {}

	public: // IOutEndpoint implementaton
		virtual signals::EType Type()	{ return OUT_TYPE; }

		virtual signals::IEPBuffer* CreateBuffer()
		{
			if(!m_writeFrom) return NULL;
			signals::IEPBuffer* buff = m_writeFrom->CreateBuffer();
			if(!buff) return NULL;
			ASSERT(buff->Type() == IN_TYPE);
			unsigned capacity = buff->Capacity();
			buff->Release(NULL);
			buff = new CEPBuffer<OUT_TYPE>(capacity);
			buff->AddRef(NULL);
			return buff;
		}

	public: // IEPSender implementaton
		virtual unsigned Write(signals::EType type, const void* buffer, unsigned numElem, unsigned msTimeout)
		{
			ASSERT(type == IN_TYPE);
			if(!m_writeTo) return 0;
			in_type* nativeBuff = (in_type*)buffer;

			TEMP_TYPE accum;
			m_oper.init(accum);
			for(unsigned idx=0; idx < numElem; idx++)
			{
				m_oper.iter(accum, nativeBuff[idx], idx);
			}
			out_type result = m_oper.finish(accum, numElem);
			return m_writeTo->Write(OUT_TYPE, &result, 1, msTimeout);
		}
	};

	class Instance : public InstanceBase
	{
	public:
		inline Instance(CFrameSummary* spec)
			:InstanceBase(spec),m_input(this,spec,spec->m_oper),m_output(this,spec,spec->m_oper)
		{}

		virtual signals::IInputFunction* Input()		{ AddRef(); return &m_input; }
		virtual signals::IOutputFunction* Output()		{ AddRef(); return &m_output; }

	protected:
		InputFunction m_input;
		OutputFunction m_output;
	};

public:
	signals::IFunction* CreateImpl()
	{
		return new Instance(this);
	}

	inline CFrameSummary(const OPER& oper = OPER())
		:FunctionBase(oper.NAME, oper.DESC), m_oper(oper)
	{}

protected:
	OPER m_oper;
};

template<typename TYPE>
struct frame_max
{
	typedef TYPE temp_type;
	static const char* NAME;
	static const char* DESC;

	void init(TYPE& accum) { }
	void iter(TYPE& accum, const TYPE& val, unsigned iter) { if(!iter || accum < val) accum = val; }
	TYPE finish(const TYPE& accum, unsigned numIter) { return (TYPE)accum; }
};
template<typename TYPE> const char* frame_max<TYPE>::NAME = "frame max";
template<typename TYPE> const char* frame_max<TYPE>::DESC = "maximum value in the frame";

template<typename TYPE>
struct frame_min
{
	typedef TYPE temp_type;
	static const char* NAME;
	static const char* DESC;

	void init(TYPE& accum) { }
	void iter(TYPE& accum, const TYPE& val, unsigned iter) { if(!iter || accum > val) accum = val; }
	TYPE finish(const TYPE& accum, unsigned numIter) { return accum; }
};
template<typename TYPE> const char* frame_min<TYPE>::NAME = "frame min";
template<typename TYPE> const char* frame_min<TYPE>::DESC = "minimum value in the frame";

template<typename IN_TYPE, typename TEMP_TYPE, typename OUT_TYPE>
struct frame_mean
{
	typedef TEMP_TYPE temp_type;
	static const char* NAME;
	static const char* DESC;

	void init(TEMP_TYPE& accum) { }
	void iter(TEMP_TYPE& accum, const IN_TYPE& val, unsigned iter)
	{
		if(!iter)
		{
			accum = (TEMP_TYPE)val;
		} else {
			accum += val;
		}
	}

	OUT_TYPE finish(const TEMP_TYPE& accum, unsigned numIter) { return (OUT_TYPE)(accum / numIter); }
};
template<typename IN_TYPE, typename TEMP_TYPE, typename OUT_TYPE>
const char* frame_mean<IN_TYPE,TEMP_TYPE,OUT_TYPE>::NAME = "frame mean";
template<typename IN_TYPE, typename TEMP_TYPE, typename OUT_TYPE>
const char* frame_mean<IN_TYPE,TEMP_TYPE,OUT_TYPE>::DESC = "average value in the frame";
