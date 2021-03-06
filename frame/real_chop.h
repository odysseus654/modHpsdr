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

template<class AttrsType>
class CAttr_widthProxy : public CAttribute<signals::etypLong>, public signals::IAttributeObserver
{
private:
	typedef CAttribute<signals::etypLong> my_base;
	RWLock m_proxyLock;
	signals::IAttribute* m_proxyObject;
	AttrsType* m_attrs;

public:
	inline CAttr_widthProxy(AttrsType* attrs)
		:my_base(NAME, DESCR, 0), m_attrs(attrs), m_proxyObject(NULL) { }

	virtual ~CAttr_widthProxy();
	virtual const void* getValue();
	virtual BOOL isReadOnly() const;
	virtual BOOL setValue(const void* newVal);
	virtual void OnDetached(IAttribute* attr);
	virtual void OnChanged(IAttribute* attr, const void* newVal);
	void establishProxy(bool bForce = false);

private:
	static const char* NAME;
	static const char* DESCR;
};

template<signals::EType ET>
class ChopRealFrame : public signals::IFunctionSpec
{
protected:
	static signals::IVector* chop_frame(signals::IVector* frame);

protected:
	template<typename Derived>
	class ChopAttributeBase : public CCascadedAttributesBase<Derived>
	{
	private:
		typedef ChopAttributeBase<Derived> my_attr_type;

	protected:
		inline ChopAttributeBase()
			:CCascadedAttributesBase<Derived>(*static_cast<Derived*>(this))
		{
			buildAttrs();
		}

		struct
		{
			CAttr_widthProxy<my_attr_type>* blockSize;
			CAttributeBase* isComplexInput;
		} attrs;

		void buildAttrs()
		{
			attrs.blockSize = addLocalAttr(true, new CAttr_widthProxy<my_attr_type>(this));
			attrs.isComplexInput = addLocalAttr(true, new CROAttribute<signals::etypBoolean>("isComplexInput", "Is this stream based on complex data?", false));
		}

		virtual signals::IAttribute* GetByName(const char* name)
		{
			signals::IAttribute* attr = CCascadedAttributesBase<Derived>::GetByName(name);
			if(attr == attrs.blockSize && attrs.blockSize) attrs.blockSize->establishProxy(true);
			return attr;
		}

		inline void verifyProxy()
		{
			if(attrs.blockSize) attrs.blockSize->establishProxy();
		}
	};

public:
	inline ChopRealFrame() {}
	virtual const char* Name()			{ return NAME; }
	virtual const char* Description()	{ return DESCR; }
	virtual const unsigned char* Fingerprint() { return FINGERPRINT; }
	
protected:
	class InputFunction : public InputFunctionBase, public ChopAttributeBase<InputFunction>
	{
	protected:
		typedef std::vector<signals::IVector*> TInBuffer;
		TInBuffer m_buffer;

		inline signals::IAttributes* RemoteAttributes() { return m_readFrom ? m_readFrom->OutputAttributes() : NULL; }

	public:
		inline InputFunction(signals::IFunction* parent, signals::IFunctionSpec* spec)
			:InputFunctionBase(parent, spec) {}

	public: // IInEndpoint implementaton
		virtual signals::EType Type()	{ return ET; }

	public: // IEPReceiver implementaton
		virtual unsigned Read(signals::EType type, void* buffer, unsigned numAvail, BOOL bFillAll, unsigned msTimeout);
		virtual BOOL ReadOne(signals::EType type, void* buffer, unsigned msTimeout);

		virtual signals::IEPBuffer* CreateBuffer()
		{
			if(!m_readFrom) return NULL;
			signals::IEPBuffer* buff = m_readFrom->CreateBuffer();
			ASSERT(buff && buff->Type() == ET);
			return buff;
		}

		virtual signals::IAttributes* OutputAttributes() { return this; }

		friend class CCascadedAttributesBase<InputFunction>;
	};

	class OutputFunction : public OutputFunctionBase, public ChopAttributeBase<OutputFunction>
	{
	protected:
		typedef std::vector<signals::IVector*> TOutBuffer;
		TOutBuffer m_buffer;

	public:
		inline OutputFunction(signals::IFunction* parent, signals::IFunctionSpec* spec)
			:OutputFunctionBase(parent, spec) {}

	protected:
		inline signals::IAttributes* RemoteAttributes() { return m_writeFrom ? m_writeFrom->Attributes() : NULL; }

	public: // IOutEndpoint implementaton
		virtual signals::EType Type()	{ return ET; }
		virtual signals::IAttributes* Attributes() { return this; }

		virtual signals::IEPBuffer* CreateBuffer()
		{
			if(!m_writeFrom) return NULL;
			signals::IEPBuffer* buff = m_writeFrom->CreateBuffer();
			ASSERT(buff && buff->Type() == ET);
			return buff;
		}

	public: // IEPSender implementaton
		virtual unsigned Write(signals::EType type, const void* buffer, unsigned numElem, unsigned msTimeout);
		virtual BOOL WriteOne(signals::EType type, const void* buffer, unsigned msTimeout);

		friend class CCascadedAttributesBase<OutputFunction>;
	};

	class Instance : public CRefcountObject, public signals::IFunction
	{
	public:
		inline Instance(signals::IFunctionSpec* spec)
			:m_spec(spec),m_input(this,spec),m_output(this,spec) {}

		virtual signals::IFunctionSpec* Spec()			{ return m_spec; }
		virtual unsigned AddRef()						{ return CRefcountObject::AddRef(); }
		virtual unsigned Release()						{ return CRefcountObject::Release(); }
		virtual signals::IInputFunction* Input()		{ AddRef(); return &m_input; }
		virtual signals::IOutputFunction* Output()		{ AddRef(); return &m_output; }

	protected:
		signals::IFunctionSpec* m_spec;
		InputFunction m_input;
		OutputFunction m_output;
	};
	
public:
	virtual signals::IFunction* Create()
	{
		signals::IFunction* func = new Instance(this);
		func->AddRef();
		return func;
	}
	
private:
	static const char* NAME;
	static const char* DESCR;
	static const unsigned char FINGERPRINT[];
};

// ------------------------------------------------------------------------------------------------ CAttr_widthProxy

template<class AttrsType>
char const * CAttr_widthProxy<AttrsType>::NAME = "blockSize";

template<class AttrsType>
char const * CAttr_widthProxy<AttrsType>::DESCR = "Number of samples to process in each block";

template<class AttrsType>
const void* CAttr_widthProxy<AttrsType>::getValue()
{
	establishProxy();
	return my_base::getValue();
}

template<class AttrsType>
BOOL CAttr_widthProxy<AttrsType>::isReadOnly() const
{
	ReadLocker lock(m_proxyLock);
	return m_proxyObject ? m_proxyObject->isReadOnly() : TRUE;
}

template<class AttrsType>
BOOL CAttr_widthProxy<AttrsType>::setValue(const void* newVal)
{
	establishProxy();
	ReadLocker lock(m_proxyLock);
	if(!m_proxyObject) return FALSE;
	long* pNewVal = (long*)newVal;
	switch(m_proxyObject->Type())
	{
	case signals::etypByte:
		{
			if(*pNewVal > 126) return FALSE;
			unsigned char newVal = (unsigned char)(*pNewVal * 2);
			return m_proxyObject->setValue(&newVal);
		}
	case signals::etypShort:
		{
			if(*pNewVal > 16382) return FALSE;
			short newVal = short(*pNewVal * 2);
			return m_proxyObject->setValue(&newVal);
		}
	case signals::etypLong:
		{
			if(*pNewVal > (MAXLONG32/2-2)) return FALSE;
			long newVal = *pNewVal * 2;
			return m_proxyObject->setValue(&newVal);
		}
	case signals::etypInt64:
		{
			__int64 newVal = __int64(*pNewVal) * 2;
			return m_proxyObject->setValue(&newVal);
		}
	default:
		return FALSE;
	}
}

template<class AttrsType>
CAttr_widthProxy<AttrsType>::~CAttr_widthProxy()
{
	WriteLocker lock(m_proxyLock);
	if(m_proxyObject)
	{
		m_proxyObject->Unobserve(this);
		m_proxyObject = NULL;
	}
}

template<class AttrsType>
void CAttr_widthProxy<AttrsType>::OnDetached(IAttribute* attr)
{
	WriteLocker lock(m_proxyLock);
	ASSERT(attr == m_proxyObject && m_proxyObject);
	if(attr == m_proxyObject && m_proxyObject)
	{
		privateSetValue(0);
		m_proxyObject = NULL;
	}
}

template<class AttrsType>
void CAttr_widthProxy<AttrsType>::OnChanged(IAttribute* attr, const void* newVal)
{
	switch(attr->Type())
	{
	case signals::etypByte:
		privateSetValue(*((unsigned char*)newVal) / 2);
		break;
	case signals::etypShort:
		privateSetValue(*((short*)newVal) / 2);
		break;
	case signals::etypLong:
		privateSetValue(*((long*)newVal) / 2);
		break;
	case signals::etypInt64:
		privateSetValue(long(*((__int64*)newVal) / 2));
		break;
	}
}

template<class AttrsType>
void CAttr_widthProxy<AttrsType>::establishProxy(bool bForce = false)
{
	if(!bForce)
	{
		ReadLocker lock(m_proxyLock);
		if(m_proxyObject) return;
	}

	WriteLocker lock(m_proxyLock);
	if(m_proxyObject && !bForce) return;

	signals::IAttribute* attr = m_attrs->RemoteGetByName(NAME);
	if(attr == m_proxyObject) return;

	if(m_proxyObject)
	{
		m_proxyObject->Unobserve(this);
		m_proxyObject = NULL;
	}

	if(attr && (attr->Type() == signals::etypShort || attr->Type() == signals::etypLong))
	{
		m_proxyObject = attr;
		m_proxyObject->Observe(this);
		OnChanged(attr, attr->getValue());
	}
}

// ------------------------------------------------------------------------------------------------

template<signals::EType ET>
const unsigned char ChopRealFrame<ET>::FINGERPRINT[] = { (unsigned char)ET, (unsigned char)ET };

template<signals::EType ET>
const char * ChopRealFrame<ET>::NAME = "chop real frame";

template<signals::EType ET>
const char * ChopRealFrame<ET>::DESCR = "After an FFT operation from real inputs, drop the second half of the frame";

template<signals::EType ET>
unsigned ChopRealFrame<ET>::InputFunction::Read(signals::EType type, void* buffer, unsigned numAvail,
	BOOL bFillAll, unsigned msTimeout)
{
	ASSERT(type == ET);
	if(!m_readFrom) return 0;
	verifyProxy();

	if(m_buffer.size() < numAvail) m_buffer.resize(numAvail);
	unsigned numElem = m_readFrom->Read(ET, &m_buffer[0], numAvail, bFillAll, msTimeout);
	if(numElem)
	{
		signals::IVector** nativeBuff = (signals::IVector**)buffer;
		for(unsigned idx=0; idx < numElem; idx++)
		{
			nativeBuff[idx] = chop_frame(m_buffer[idx]);
		}
	}
	return numElem;
}

template<signals::EType ET>
BOOL ChopRealFrame<ET>::InputFunction::ReadOne(signals::EType type, void* buffer, unsigned msTimeout)
{
	ASSERT(type == ET);
	if(!m_readFrom) return FALSE;
	verifyProxy();

	signals::IVector* localBuffer;
	BOOL numElem = m_readFrom->ReadOne(ET, &localBuffer, msTimeout);
	if(numElem) *(signals::IVector**)buffer = chop_frame(localBuffer);
	return numElem;
}

template<signals::EType ET>
unsigned ChopRealFrame<ET>::OutputFunction::Write(signals::EType type, const void* buffer,
	unsigned numElem, unsigned msTimeout)
{
	ASSERT(type == ET);
	if(!m_writeTo || !numElem) return 0;
	verifyProxy();

	if(m_buffer.size() < numElem) m_buffer.resize(numElem);
	signals::IVector** nativeBuff = (signals::IVector**)buffer;
	for(unsigned idx=0; idx < numElem; idx++)
	{
		m_buffer[idx] = chop_frame(nativeBuff[idx]);
	}

	return m_writeTo->Write(ET, &m_buffer[0], numElem, msTimeout);
}

template<signals::EType ET>
BOOL ChopRealFrame<ET>::OutputFunction::WriteOne(signals::EType type, const void* buffer, unsigned msTimeout)
{
	ASSERT(type == ET);
	if(!m_writeTo) return FALSE;
	verifyProxy();

	signals::IVector* localBuffer = chop_frame(*(signals::IVector**)buffer);
	return m_writeTo->WriteOne(ET, &localBuffer, msTimeout);
}

template<signals::EType ET>
signals::IVector* ChopRealFrame<ET>::chop_frame(signals::IVector* frame)
{
	typedef StoreType<ET> my_type;
	typedef StoreType<(signals::EType)my_type::base_enum> base_type;

	if(!frame) { ASSERT(FALSE); return NULL; }
	ASSERT(frame->Type() == my_type::base_enum);
	unsigned copySize = frame->Size() / 2;
	my_type::buffer_templ* newBuff = my_type::buffer_templ::retrieve(copySize);
	base_type::type* srcData = (base_type::type*)frame->Data();
	if(base_type::is_blittable)
	{
		memcpy(newBuff->data, srcData, sizeof(base_type::type) * copySize);
	}
	else for(unsigned idx=0; idx < copySize; idx++)
	{
		newBuff->data[idx] = srcData[idx];
	}
	frame->Release();
	return newBuff;
}
