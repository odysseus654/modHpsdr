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

class CFFTransform : public signals::IBlock, public CAttributesBase, protected CRefcountObject
{
public:
	CFFTransform(signals::IBlockDriver* driver);
	virtual ~CFFTransform();

private:
	CFFTransform(const CFFTransform& other);
	CFFTransform& operator=(const CFFTransform& other);

public: // IBlock implementation
	virtual unsigned AddRef()				{ return CRefcountObject::AddRef(); }
	virtual unsigned Release()				{ return CRefcountObject::Release(); }
	virtual const char* Name()				{ return NAME; }
	virtual unsigned NodeId(char* /* buff */ , unsigned /* availChar */) { return 0; }
	virtual signals::IBlockDriver* Driver()	{ return m_driver; }
	virtual signals::IBlock* Parent()		{ return NULL; }
	virtual signals::IAttributes* Attributes() { return this; }
	virtual unsigned Children(signals::IBlock** /* blocks */, unsigned /* availBlocks */) { return 0; }
	virtual unsigned Incoming(signals::IInEndpoint** ep, unsigned availEP);
	virtual unsigned Outgoing(signals::IOutEndpoint** ep, unsigned availEP);
	virtual void Start()					{ startPlan(false); }
	virtual void Stop()						{}

public:
	struct
	{
		CAttributeBase* blockSize;
	} attrs;

	void setBlockSize(const short& blockSize);

private:
	enum
	{
		IN_BUFFER_SIZE = 1000,
		IN_BUFFER_TIMEOUT = 1000,
		DEFAULT_BLOCK_SIZE = 1024
	};

	typedef std::complex<double> TComplexDbl;
	static const char* NAME;

	volatile long m_requestSize;
	AsyncDelegate<> m_refreshPlanEvent;
	signals::IBlockDriver* m_driver;

	void clearPlan();
	void refreshPlan();

public:
	class COutgoing : public CSimpleOutgoingChild<signals::etypVecCmplDbl>
	{	// This class is assumed to be a static (non-dynamic) member of its parent
	public:
		typedef TComplexDbl store_type;
		inline COutgoing(CFFTransform* parent):m_parent(parent) { }
		void buildAttrs(const CFFTransform& parent);

	protected:
		CFFTransform* m_parent;

	public:
		struct
		{
			CEventAttribute* sync_fault;
			CAttributeBase* rate;
			CAttributeBase* blockSize;
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

	class CIncoming : public CSimpleIncomingChild<signals::etypCmplDbl>
	{	// This class is assumed to be a static (non-dynamic) member of its parent
	public:
		inline CIncoming(signals::IBlock* parent):CSimpleIncomingChild(parent) { }
		virtual const char* EPName()				{ return EP_NAME; }
		virtual const char* EPDescr()				{ return EP_DESCR; }

	private:
		const static char* EP_NAME;
		const static char* EP_DESCR;
		CIncoming(const CIncoming& other);
		CIncoming& operator=(const CIncoming& other);
	};

private:
	Thread<CFFTransform*> m_dataThread;
	bool m_bDataThreadEnabled;

	CIncoming m_incoming;
	COutgoing m_outgoing;

	Lock m_planLock;
	fftss_plan m_currPlan;
	TComplexDbl* m_inBuffer;
	TComplexDbl* m_outBuffer;
	unsigned m_bufSize;
	bool m_bFaulted;

	void buildAttrs();
	bool startPlan(bool bLockHeld);
	static void fft_process_thread(CFFTransform* owner);
};

}
