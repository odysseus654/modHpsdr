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

	void setBlockSize(const long& blockSize);

private:
	enum
	{
		IN_BUFFER_TIMEOUT = 1000,
		DEFAULT_BLOCK_SIZE = 1024
	};

	static const char* NAME;

public:
	class COutgoing : public CSimpleCascadeOutgoingChild<OUT_TYPE>
	{	// This class is assumed to be a static (non-dynamic) member of its parent
	public:
		inline COutgoing(CFrameBuilder* parent):CSimpleCascadeOutgoingChild(parent->m_incoming),m_parent(parent) { }
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

	class CIncoming : public CSimpleCascadeIncomingChild
	{	// This class is assumed to be a static (non-dynamic) member of its parent
	public:
		inline CIncoming(CFrameBuilder* parent):CSimpleCascadeIncomingChild(IN_TYPE, parent, parent->m_outgoing) { }
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

// ------------------------------------------------------------------ class CFrameBuilder

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
	attrs.blockSize = addLocalAttr(true, new CAttr_callback<signals::etypLong,CFrameBuilder>
		(*this, "blockSize", "Number of samples to process in each block", &CFrameBuilder::setBlockSize, DEFAULT_BLOCK_SIZE));
	m_outgoing.buildAttrs(*this);
}

template<signals::EType IN_TYPE, signals::EType OUT_TYPE>
void CFrameBuilder<IN_TYPE,OUT_TYPE>::setBlockSize(const long& newBs)
{
	InterlockedExchange(&m_bufSize, newBs);
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
	ThreadBase::SetThreadName("Frame Construction Thread");

	typedef StoreType<OUT_TYPE>::buffer_templ VectorType;
	typedef StoreType<IN_TYPE>::type store_type;
	VectorType* buffer = NULL;
	unsigned inOffset = 0;
	while(threadRunning())
	{
		unsigned bufSize = m_bufSize;
		if(!bufSize)
		{
			Sleep(IN_BUFFER_TIMEOUT);
			continue;
		}
		if(!buffer)
		{
			buffer = VectorType::retrieve(bufSize);
		}
		else if(buffer->size != bufSize)
		{
			VectorType* newBuff = VectorType::retrieve(bufSize);
			if(inOffset >= bufSize)
			{
				unsigned outOffset = 0;
				while(inOffset - outOffset >= bufSize)
				{
					if(StoreType<IN_TYPE>::is_blittable)
					{
						memcpy(newBuff->data, buffer->data + outOffset, bufSize * sizeof(store_type));
					}
					else for(unsigned idx=0; idx < bufSize; idx++)
					{
						newBuff->data[idx] = buffer->data[idx + outOffset];
					}
					BOOL outFrame = m_outgoing.WriteOne(OUT_TYPE, &newBuff, INFINITE);
					if(!outFrame && m_outgoing.isConnected()) m_outgoing.attrs.sync_fault->fire();

					newBuff = VectorType::retrieve(bufSize);
					inOffset += bufSize;
				}
				if(StoreType<IN_TYPE>::is_blittable)
				{
					memcpy(newBuff->data, buffer->data + outOffset, (inOffset - outOffset) * sizeof(store_type));
				}
				else for(unsigned idx=outOffset; idx < inOffset; idx++)
				{
					newBuff->data[idx-outOffset] = buffer->data[idx];
				}
				inOffset -= outOffset;
			}
			else
			{
				if(StoreType<IN_TYPE>::is_blittable)
				{
					memcpy(newBuff->data, buffer->data, inOffset * sizeof(store_type));
				}
				else for(unsigned idx=0; idx < inOffset; idx++)
				{
					newBuff->data[idx] = buffer->data[idx];
				}
			}
			buffer->Release();
			buffer = newBuff;
		}
		if(inOffset < bufSize)
		{
			unsigned recvCount = m_incoming.Read(IN_TYPE, buffer->data + inOffset, bufSize - inOffset, TRUE, IN_BUFFER_TIMEOUT);
			inOffset += recvCount;
		}
		if(inOffset >= bufSize)
		{
			BOOL outFrame = m_outgoing.WriteOne(OUT_TYPE, &buffer, INFINITE);
			if(!outFrame && m_outgoing.isConnected()) m_outgoing.attrs.sync_fault->fire();
			buffer = VectorType::retrieve(bufSize);
			inOffset = 0;
		}
	}
	if(buffer) buffer->Release();
}
