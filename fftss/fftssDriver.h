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
#include "fftss/include/fftss.h"

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
	virtual const char* Name()			{ return NAME; }
	virtual const char* Description()	{ return DESCR; }
	virtual BOOL canCreate()			{ return true; }
	virtual BOOL canDiscover()			{ return false; }
	virtual unsigned Discover(signals::IBlock** blocks, unsigned availBlocks) { return 0; }
	virtual signals::IBlock* Create();
	const unsigned char* Fingerprint()	{ return FINGERPRINT; }

protected:
	static const char* NAME;
	static const char* DESCR;
	static const unsigned char FINGERPRINT[];
};

class CFFTransform : public CThreadBlockBase
{
public:
	CFFTransform(signals::IBlockDriver* driver);
	virtual ~CFFTransform();

private:
	CFFTransform(const CFFTransform& other);
	CFFTransform& operator=(const CFFTransform& other);

public: // IBlock implementation
	virtual const char* Name()				{ return NAME; }
	virtual unsigned Incoming(signals::IInEndpoint** ep, unsigned availEP) { return singleIncoming(&m_incoming, ep, availEP); }
	virtual unsigned Outgoing(signals::IOutEndpoint** ep, unsigned availEP) { return singleOutgoing(&m_outgoing, ep, availEP); }

private:
	enum
	{
		IN_BUFFER_TIMEOUT = 1000,
	};

	typedef std::complex<double> TComplexDbl;
	static const char* NAME;

	volatile long m_requestSize;

	void clearPlan();
	void refreshPlan();

public:
	class COutgoing : public CSimpleCascadeOutgoingChild<signals::etypVecCmplDbl>
	{	// This class is assumed to be a static (non-dynamic) member of its parent
	public:
		typedef TComplexDbl store_type;
		inline COutgoing(CFFTransform* parent):CSimpleCascadeOutgoingChild(parent->m_incoming),m_parent(parent) { }
		void buildAttrs(const CFFTransform& parent);

	protected:
		CFFTransform* m_parent;

	public:
		struct
		{
			CEventAttribute* sync_fault;
		} attrs;

	private:
		const static char* EP_NAME;
		const static char* EP_DESCR;
		COutgoing(const COutgoing& other);
		COutgoing& operator=(const COutgoing& other);

	public: // COutEndpointBase interface
		virtual const char* EPName()				{ return EP_NAME; }
		virtual const char* EPDescr()				{ return EP_DESCR; }
	};

	class CIncoming : public CSimpleIncomingChild<signals::etypVecCmplDbl>, public signals::IAttributeObserver
	{	// This class is assumed to be a static (non-dynamic) member of its parent
	public:
		inline CIncoming(signals::IBlock* parent):CSimpleIncomingChild(parent),m_lastWidthAttr(NULL),m_bAttached(false) { }
		virtual ~CIncoming();
		virtual const char* EPName()				{ return EP_NAME; }
		virtual const char* EPDescr()				{ return EP_DESCR; }
		virtual void OnChanged(const char* name, signals::EType type, const void* value);
		virtual void OnDetached(const char* name);

	private:
		const static char* EP_NAME;
		const static char* EP_DESCR;
		signals::IAttribute* m_lastWidthAttr;
		bool m_bAttached;

		CIncoming(const CIncoming& other);
		CIncoming& operator=(const CIncoming& other);
		virtual void OnConnection(signals::IEPRecvFrom* recv);
	};

private:
	CIncoming m_incoming;
	COutgoing m_outgoing;

	Lock m_planLock;
	fftss_plan m_currPlan;
	TComplexDbl* m_inBuffer;
	TComplexDbl* m_outBuffer;
	unsigned m_bufSize;

	void buildAttrs();
	bool startPlan();
	virtual void thread_run();
};

}
