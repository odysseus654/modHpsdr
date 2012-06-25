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

#include "blockImpl.h"

namespace fftss
{
	class CFFTransformDriver;
}
extern fftss::CFFTransformDriver DRIVER_FFTransform;
extern "C" unsigned QueryDrivers(signals::IBlockDriver** drivers, unsigned availDrivers);

typedef void *fftss_plan;

namespace fftss {

class CFFTransformDriver : public signals::IBlockDriver
{
public:
	inline CFFTransformDriver() {}
	virtual ~CFFTransformDriver() {}

private:
	CFFTransformDriver(const CFFTransformDriver& other);
	CFFTransformDriver operator=(const CFFTransformDriver& other);

public:
	virtual const char* Name()		{ return NAME; }
	virtual bool canCreate()		{ return true; }
	virtual bool canDiscover()		{ return false; }
	virtual unsigned Discover(signals::IBlock** blocks, unsigned availBlocks) { return 0; }
	virtual signals::IBlock* Create();

protected:
	static const char* NAME;
};

class CFFTransformBase : public signals::IBlock, public CAttributesBase, protected CRefcountObject
{
public:
	CFFTransformBase();
	virtual ~CFFTransformBase();

private:
	CFFTransformBase(const CFFTransformBase& other);
	CFFTransformBase& operator=(const CFFTransformBase& other);

public: // IBlock implementation
	virtual unsigned AddRef()				{ return CRefcountObject::AddRef(); }
	virtual unsigned Release()				{ return CRefcountObject::Release(); }
	virtual const char* Name()				{ return NAME; }
	virtual signals::IBlockDriver* Driver()	{ return &DRIVER_FFTransform; }
	virtual signals::IBlock* Parent()		{ return NULL; }
	virtual signals::IBlock* Block()		{ return this; }
	virtual signals::IAttributes* Attributes() { return this; }
	virtual unsigned Children(signals::IBlock** /* blocks */, unsigned /* availBlocks */) { return 0; }
//	virtual unsigned Incoming(signals::IInEndpoint** ep, unsigned availEP);
//	virtual unsigned Outgoing(signals::IOutEndpoint** ep, unsigned availEP);
	virtual void Start()					{ startPlan(false); }
	virtual void Stop()						{}

public:
	struct
	{
		CAttributeBase* blockSize;
	} attrs;

public:
	class CAttr_block_size : public CRWAttribute<signals::etypLong>
	{
	private:
		typedef CRWAttribute<signals::etypLong> base;
	public:
		inline CAttr_block_size(CFFTransformBase& parent, const char* name, const char* descr, long deflt)
			:base(name, descr, deflt), m_parent(parent)
		{
			m_parent.setBlockSize(deflt);
		}

	protected:
		CFFTransformBase& m_parent;

	protected:
		virtual void onSetValue(const store_type& newVal)
		{
			m_parent.setBlockSize(newVal);
			base::onSetValue(newVal);
		}
	};

private:
	static const char* NAME;
	enum
	{
		DEFAULT_BLOCK_SIZE = 1024
	};

	volatile long m_requestSize;
	AsyncDelegate<> m_refreshPlanEvent;

	void clearPlan();
	void setBlockSize(long blockSize);
	void refreshPlan();

protected:
	typedef std::complex<double> TComplexDbl;
	Lock m_planLock;
	fftss_plan m_currPlan;
	TComplexDbl* m_inBuffer;
	TComplexDbl* m_outBuffer;
	unsigned m_bufSize;

	void buildAttrs();
	bool startPlan(bool bLockHeld);
};

#pragma warning(disable: 4355)
template<signals::EType ETin, signals::EType ETout>
class CFFTransform : public CFFTransformBase
{
public:
	CFFTransform();
	virtual ~CFFTransform();

private:
	CFFTransform(const CFFTransform& other);
	CFFTransform& operator=(const CFFTransform& other);

public: // IBlock implementation
	virtual unsigned Incoming(signals::IInEndpoint** ep, unsigned availEP);
	virtual unsigned Outgoing(signals::IOutEndpoint** ep, unsigned availEP);

public:
	class COutgoing : public COutEndpointBase, public CAttributesBase
	{	// This class is assumed to be a static (non-dynamic) member of its parent
	public:
		typedef typename StoreType<ETout>::type store_type;
		inline COutgoing(CFFTransform* parent):m_parent(parent) { }
		virtual ~COutgoing() {}
		void buildAttrs(const CFFTransform& parent);

	protected:
		enum { DEFAULT_BUFSIZE = 4096 };
		CFFTransform* m_parent;

	public:
		struct
		{
			CEventAttribute* sync_fault;
			CAttributeBase* rate;
		} attrs;

	private:
		const static char* EP_NAME[];
		const static char* EP_DESCR;
		COutgoing(const COutgoing& other);
		COutgoing& operator=(const COutgoing& other);

	public: // COutEndpointBase interface
		virtual signals::IBlock* Block()			{ m_parent->AddRef(); return m_parent; }
		virtual signals::EType Type()				{ return ETout; }
		virtual const char* EPName()				{ return EP_DESCR; }
		virtual signals::IEPBuffer* CreateBuffer()	{ return new CEPBuffer<ETout>(DEFAULT_BUFSIZE); }
		virtual signals::IAttributes* Attributes()	{ return this; }
	};

	class CIncoming : public CInEndpointBase, public CAttributesBase
	{	// This class is assumed to be a static (non-dynamic) member of its parent
	public:
		typedef typename StoreType<ETin>::type store_type;
		inline CIncoming(signals::IBlock* parent):m_parent(parent) { }
		virtual ~CIncoming() {}

	protected:
		enum { DEFAULT_BUFSIZE = 4096 };
		signals::IBlock* m_parent;

	private:
		const static char* EP_NAME;
		const static char* EP_DESCR;
		CIncoming(const CIncoming& other);
		CIncoming& operator=(const CIncoming& other);

	public: // CInEndpointBase interface
		virtual const char* EPName()				{ return EP_DESCR; }
		virtual unsigned AddRef()					{ return m_parent->AddRef(); }
		virtual unsigned Release()					{ return m_parent->Release(); }
		virtual signals::IBlock* Block()			{ m_parent->AddRef(); return m_parent; }
		virtual signals::EType Type()				{ return ETin; }
		virtual signals::IAttributes* Attributes()	{ return this; }
		virtual signals::IEPBuffer* CreateBuffer()	{ return new CEPBuffer<ETin>(DEFAULT_BUFSIZE); }
	};

private:
	enum
	{
		IN_BUFFER_SIZE = 100,
		IN_BUFFER_TIMEOUT = 1000
	};

	Thread<> m_dataThread;
	bool m_bDataThreadEnabled;

	CIncoming m_incoming;
	COutgoing m_outgoing;
	std::vector<typename COutgoing::store_type> m_outStage;

	void buildAttrs();
	void thread_data();
	void onBufferFilled(const TComplexDbl* inBuffer);
};

// ------------------------------------------------------------------------------------------------

template<signals::EType ETin, signals::EType ETout>
CFFTransform<ETin,ETout>::CFFTransform()
	:m_incoming(this),m_outgoing(this),m_bDataThreadEnabled(true),
	 m_dataThread(Thread<>::delegate_type(this, &CFFTransform::thread_data))
{
	buildAttrs();
	m_dataThread.launch(THREAD_PRIORITY_NORMAL);
}

template<signals::EType ETin, signals::EType ETout>
CFFTransform<ETin,ETout>::~CFFTransform()
{
	m_bDataThreadEnabled = false;
	m_dataThread.close();
}

template<signals::EType ETin, signals::EType ETout>
const char* CFFTransform<ETin,ETout>::CIncoming::EP_NAME = "in";

template<signals::EType ETin, signals::EType ETout>
const char* CFFTransform<ETin,ETout>::CIncoming::EP_DESCR = "FFT Transform incoming endpoint";

template<signals::EType ETin, signals::EType ETout>
const char* CFFTransform<ETin,ETout>::COutgoing::EP_NAME = "out";

template<signals::EType ETin, signals::EType ETout>
const char* CFFTransform<ETin,ETout>::COutgoing::EP_DESCR = "FFT Transform outgoing endpoint";

template<signals::EType ETin, signals::EType ETout>
unsigned CFFTransform<ETin,ETout>::Incoming(signals::IInEndpoint** ep, unsigned availEP)
{
	if(ep && availEP)
	{
		ep[0] = &m_incoming;
	}
	return 1;
}

template<signals::EType ETin, signals::EType ETout>
unsigned CFFTransform<ETin,ETout>::Outgoing(signals::IOutEndpoint** ep, unsigned availEP)
{
	if(ep && availEP)
	{
		ep[0] = &m_outgoing;
	}
	return 1;
}

template<signals::EType ETin, signals::EType ETout>
void CFFTransform<ETin,ETout>::thread_data()
{
	ThreadBase::SetThreadName("FFTSS Transform Thread");

	CIncoming::store_type buffer[IN_BUFFER_SIZE];
	unsigned residue = 0;
	while(m_bDataThreadEnabled)
	{
		unsigned recvCount = m_incoming.Read(ETin, &buffer + residue, IN_BUFFER_SIZE - residue, IN_BUFFER_TIMEOUT);
		if(recvCount)
		{
			residue += recvCount;
			Locker lock(m_planLock);
			if(!m_currPlan)
			{
				if(!startPlan(true))
				{
					residue = 0;		// no plan!
					continue;
				}
			}

			ASSERT(m_inBuffer && m_bufSize);
			if(m_bufSize >= residue)
			{
				for(unsigned idx=0; idx < m_bufSize; idx++)
				{
					m_inBuffer[idx] = buffer[idx];
				}
				onBufferFilled(m_inBuffer);
				residue -= m_bufSize;
			}
		}
	}
}
/*
template<signals::EType ETout>
void CFFTransform<signals::etypCmplDbl, ETout>::thread_data()
{
	ThreadBase::SetThreadName("FFTSS Transform Thread");

	CIncoming::store_type buffer[IN_BUFFER_SIZE];
	unsigned residue = 0;
	while(m_bDataThreadEnabled)
	{
		unsigned recvCount = m_incoming.Read(ETin, &buffer + residue, IN_BUFFER_SIZE - residue, IN_BUFFER_TIMEOUT);
		if(recvCount)
		{
			residue += recvCount;
			Locker lock(m_planLock);
			if(!m_currPlan)
			{
				residue = 0;		// no plan!
			}
			else
			{
				ASSERT(!m_inBuffer.empty());
				unsigned bufSize = m_inBuffer.size();
				if(bufSize >= residue)
				{
					onBufferFilled(buffer);
					residue -= bufSize;
				}
			}
		}
	}
}
*/
// Assumed m_planLock is held and m_currPlan is valid
template<signals::EType ETin, signals::EType ETout>
void CFFTransform<ETin,ETout>::onBufferFilled(const TComplexDbl* inBuffer)
{
	ASSERT(m_inBuffer && m_outBuffer && m_bufSize && m_currPlan);
	::fftss_execute_dft(m_currPlan, (double*)inBuffer, (double*)m_outBuffer);

	if(m_outStage.size() != m_bufSize) m_outStage.resize(m_bufSize);
	for(unsigned idx=0; idx < m_bufSize; idx++)
	{
		m_outStage[idx] = m_outBuffer[idx];
	}

	unsigned outSent = m_outgoing.Write(ETout, m_outStage.data(), m_bufSize, 0);
	if(outSent < m_bufSize && m_outgoing.isConnected()) m_outgoing.attrs.sync_fault->fire();
}
/*
template<signals::EType ETin>
void CFFTransform<ETin, signals::etypCmplDbl>::onBufferFilled(const TComplexDbl* inBuffer)
{
	ASSERT(m_inBuffer.size() == m_outBuffer.size() && m_currPlan && !m_inBuffer.empty());
	::fftss_execute_dft(m_currPlan, (double*)inBuffer, (double*)m_outBuffer.data());
	unsigned outSent = m_outgoing.Write(ETout, m_outBuffer.data(), bufSize, 0);
	if(outSent < bufSize && m_outgoing.isConnected()) m_outgoing.attrs.sync_fault->fire();
}
*/

template<signals::EType ETin, signals::EType ETout>
void CFFTransform<ETin,ETout>::buildAttrs()
{
	CFFTransformBase::buildAttrs();
	m_outgoing.buildAttrs(*this);
}

template<signals::EType ETin, signals::EType ETout>
void CFFTransform<ETin,ETout>::COutgoing::buildAttrs(const CFFTransform<ETin,ETout>& parent)
{
	attrs.sync_fault = addLocalAttr(true, new CEventAttribute("syncFault", "Fires when a sync fault happens in a receive stream"));
//	attrs.rate = addRemoteAttr("rate", parent.attrs.recv_speed);
}

}
