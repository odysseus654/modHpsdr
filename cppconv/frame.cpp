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
#include "stdafx.h"
#include <blockImpl.h>

template<signals::EType IN_TYPE, signals::EType OUT_TYPE = signals::EType(IN_TYPE + 8) >
class CFrameBuilder : public CThreadBlockBase
{
public:
	CFrameBuilder(signals::IBlockDriver* driver);

private:
	CFrameBuilder(const CFrameBuilder& other);
	CFrameBuilder& operator=(const CFrameBuilder& other);

public: // IBlock implementation
	virtual const char* Name()				{ return NAME; }
	virtual unsigned Incoming(signals::IInEndpoint** ep, unsigned availEP) { return singleIncoming(&m_incoming, ep, availEP); }
	virtual unsigned Outgoing(signals::IOutEndpoint** ep, unsigned availEP) { return singleOutgoing(&m_outgoing, ep, availEP); }

public:
	struct
	{
		CAttributeBase* blockSize;
	} attrs;

	void setBlockSize(const short& blockSize);

private:
	enum
	{
		IN_BUFFER_TIMEOUT = 1000,
		DEFAULT_BLOCK_SIZE = 1024
	};

	static const char* NAME;

public:
	class COutgoing : public CSimpleOutgoingChild<OUT_TYPE>
	{	// This class is assumed to be a static (non-dynamic) member of its parent
	public:
		inline COutgoing(CFrameBuilder* parent):m_parent(parent) { }
		void buildAttrs(const CFrameBuilder& parent);

	protected:
		CFrameBuilder* m_parent;

	public:
		struct
		{
			CEventAttribute* sync_fault;
//			CAttributeBase* rate;
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

	class CIncoming : public CSimpleIncomingChild<IN_TYPE>
	{	// This class is assumed to be a static (non-dynamic) member of its parent
	public:
		inline CIncoming(signals::IBlock* parent):CSimpleIncomingChild(parent) { }
		virtual const char* EPName()	{ return EP_NAME; }
		virtual const char* EPDescr()	{ return EP_DESCR; }

	private:
		const static char* EP_NAME;
		const static char* EP_DESCR;
		CIncoming(const CIncoming& other);
		CIncoming& operator=(const CIncoming& other);
	};

private:
	CIncoming m_incoming;
	COutgoing m_outgoing;

	volatile unsigned m_bufSize;

	void buildAttrs();

protected:
	virtual void thread_run();
};

template<signals::EType IN_TYPE, signals::EType OUT_TYPE = signals::EType(IN_TYPE + 8) >
class CFrameBuilderDriver : public signals::IBlockDriver
{
public:
	inline CFrameBuilderDriver() {}
	virtual ~CFrameBuilderDriver() {}

private:
	CFrameBuilderDriver(const CFrameBuilderDriver& other);
	CFrameBuilderDriver operator=(const CFrameBuilderDriver& other);

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

// ------------------------------------------------------------------------------------------------

template<signals::EType IN_TYPE, signals::EType OUT_TYPE>
const char* CFrameBuilderDriver<IN_TYPE,OUT_TYPE>::NAME = "make frame";

template<signals::EType IN_TYPE, signals::EType OUT_TYPE>
const char* CFrameBuilderDriver<IN_TYPE,OUT_TYPE>::DESCR = "Build frames from a stream";

template<signals::EType IN_TYPE, signals::EType OUT_TYPE>
const unsigned char CFrameBuilderDriver<IN_TYPE,OUT_TYPE>::FINGERPRINT[] = { 1, (unsigned char)IN_TYPE, 1, (unsigned char)OUT_TYPE };

template<signals::EType IN_TYPE, signals::EType OUT_TYPE>
const char* CFrameBuilder<IN_TYPE,OUT_TYPE>::NAME = "Build frames from a stream";

template<signals::EType IN_TYPE, signals::EType OUT_TYPE>
const char* CFrameBuilder<IN_TYPE,OUT_TYPE>::CIncoming::EP_NAME = "in";

template<signals::EType IN_TYPE, signals::EType OUT_TYPE>
const char* CFrameBuilder<IN_TYPE,OUT_TYPE>::CIncoming::EP_DESCR = "\"Make Frame\" incoming endpoint";

template<signals::EType IN_TYPE, signals::EType OUT_TYPE>
const char* CFrameBuilder<IN_TYPE,OUT_TYPE>::COutgoing::EP_NAME = "out";

template<signals::EType IN_TYPE, signals::EType OUT_TYPE>
const char* CFrameBuilder<IN_TYPE,OUT_TYPE>::COutgoing::EP_DESCR = "\"Make Frame\" outgoing endpoint";

// ------------------------------------------------------------------ class CFrameBuilderDriver

template<signals::EType IN_TYPE, signals::EType OUT_TYPE>
signals::IBlock * CFrameBuilderDriver<IN_TYPE,OUT_TYPE>::Create()
{
	signals::IBlock* blk = new CFrameBuilder<IN_TYPE,OUT_TYPE>(this);
	blk->AddRef();
	return blk;
}

// ------------------------------------------------------------------ class CFFTransform

#pragma warning(push)
#pragma warning(disable: 4355)
template<signals::EType IN_TYPE, signals::EType OUT_TYPE>
CFrameBuilder<IN_TYPE,OUT_TYPE>::CFrameBuilder(signals::IBlockDriver* driver)
	:CThreadBlockBase(driver),m_bufSize(0),m_incoming(this),m_outgoing(this)
{
	buildAttrs();
	startThread();
}
#pragma warning(pop)

template<signals::EType IN_TYPE, signals::EType OUT_TYPE>
void CFrameBuilder<IN_TYPE,OUT_TYPE>::buildAttrs()
{
	attrs.blockSize = addLocalAttr(true, new CAttr_callback<signals::etypShort,CFrameBuilder>
		(*this, "blockSize", "Number of samples to process in each block", &CFrameBuilder::setBlockSize, DEFAULT_BLOCK_SIZE));
	m_outgoing.buildAttrs(*this);
}

template<signals::EType IN_TYPE, signals::EType OUT_TYPE>
void CFrameBuilder<IN_TYPE,OUT_TYPE>::setBlockSize(const short& newBs)
{
	long blockSize(newBs);
	InterlockedExchange(&m_bufSize, blockSize);
}

template<signals::EType IN_TYPE, signals::EType OUT_TYPE>
void CFrameBuilder<IN_TYPE,OUT_TYPE>::COutgoing::buildAttrs(const CFrameBuilder& parent)
{
	attrs.sync_fault = addLocalAttr(true, new CEventAttribute("syncFault", "Fires when a sync fault happens in a receive stream"));
	attrs.blockSize = addRemoteAttr("blockSize", parent.attrs.blockSize);
//	attrs.rate = addRemoteAttr("rate", parent.attrs.recv_speed);
}

template<signals::EType IN_TYPE, signals::EType OUT_TYPE>
void CFrameBuilder<IN_TYPE,OUT_TYPE>::thread_run()
{
	ThreadBase::SetThreadName("FFTSS Transform Thread");

	typedef StoreType<IN_TYPE>::type store_type;
	std::vector<store_type> buffer;
	unsigned inOffset = 0;
	while(threadRunning())
	{
		unsigned bufSize = m_bufSize;
		if(!bufSize)
		{
			Sleep(IN_BUFFER_TIMEOUT);
			continue;
		}
		if(buffer.size() < bufSize) buffer.resize(bufSize);
		if(inOffset < bufSize)
		{
			unsigned recvCount = m_incoming.Read(IN_TYPE, buffer.data() + inOffset, bufSize - inOffset, TRUE, IN_BUFFER_TIMEOUT);
			inOffset += recvCount;
		}
		if(inOffset >= bufSize)
		{
			unsigned outSent = m_outgoing.Write(OUT_TYPE, buffer.data(), bufSize, INFINITE);
			if(outSent != m_bufSize && m_outgoing.isConnected())
			{
				m_outgoing.attrs.sync_fault->fire();
			}
			if(inOffset > bufSize)
			{
				memmove(buffer.data(), buffer.data() + bufSize, inOffset * sizeof(store_type));
			}
			inOffset -= bufSize;
		}
	}
}

// ------------------------------------------------------------------------------------------------

CFrameBuilderDriver<signals::etypBoolean> frame_bool;
CFrameBuilderDriver<signals::etypByte> frame_byte;
CFrameBuilderDriver<signals::etypShort> frame_short;
CFrameBuilderDriver<signals::etypLong> frame_long;
CFrameBuilderDriver<signals::etypInt64> frame_int64;
CFrameBuilderDriver<signals::etypSingle> frame_float;
CFrameBuilderDriver<signals::etypDouble> frame_double;
CFrameBuilderDriver<signals::etypComplex> frame_cpx;
CFrameBuilderDriver<signals::etypCmplDbl> frame_cpxdbl;
CFrameBuilderDriver<signals::etypLRSingle> frame_lr;

signals::IBlockDriver* BLOCKS[] =
{
	// make frame
	&frame_bool, &frame_byte, &frame_short, &frame_long, &frame_int64, &frame_float, &frame_double,
	&frame_cpx, &frame_cpxdbl, &frame_lr
};

extern "C" unsigned QueryDrivers(signals::IBlockDriver** drivers, unsigned availDrivers)
{
	if(drivers && availDrivers)
	{
		unsigned xfer = min(availDrivers, _countof(BLOCKS));
		for(unsigned idx=0; idx < xfer; idx++)
		{
			drivers[idx] = BLOCKS[idx];
		}
	}
	return _countof(BLOCKS);
}
