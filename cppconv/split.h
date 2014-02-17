/*
	Copyright 2012-2013 Erik Anderson

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

class CSplitterBase : public CThreadBlockBase
{
public:
	CSplitterBase(signals::EType type, signals::IBlockDriver* driver);

private:
	CSplitterBase(const CSplitterBase& other);
	CSplitterBase& operator=(const CSplitterBase& other);
	enum { NUM_EP = 4 };

public: // IBlock implementation
	virtual const char* Name()				{ return NAME; }
	virtual unsigned Incoming(signals::IInEndpoint** ep, unsigned availEP) { return singleIncoming(&m_incoming, ep, availEP); }
	virtual unsigned Outgoing(signals::IOutEndpoint** ep, unsigned availEP);

public:
	class COutgoing : public COutEndpointBase, public CCascadedAttributesBase<CInEndpointBase>
	{	// This class is assumed to be a static (non-dynamic) member of its parent
	public:
		inline COutgoing(CSplitterBase* parent):CCascadedAttributesBase(parent->m_incoming),m_parent(parent) { }
		inline COutgoing(const COutgoing& other):CCascadedAttributesBase(other.m_parent->m_incoming),m_parent(other.m_parent) { }
		void buildAttrs(const CSplitterBase& parent);

	protected:
		CSplitterBase* m_parent;

	public:
		struct
		{
			CEventAttribute* sync_fault;
		} attrs;

	private:
		const static char* EP_NAME;
		const static char* EP_DESCR;
		COutgoing& operator=(const COutgoing& other);

	public: // COutEndpointBase interface
		virtual signals::EType Type()				{ return m_parent->m_type; }
		virtual const char* EPName()				{ return EP_NAME; }
		virtual const char* EPDescr()				{ return EP_DESCR; }
		virtual signals::IAttributes* Attributes()	{ return this; }
		virtual signals::IEPBuffer* CreateBuffer()	{ return m_parent->CreateBuffer(); }
	};

	class CIncoming : public CInEndpointBase, public CAttributesBase
	{	// This class is assumed to be a static (non-dynamic) member of its parent
	public:
		inline CIncoming(CSplitterBase* parent):m_parent(parent) { }
		virtual const char* EPName()				{ return EP_NAME; }
		virtual const char* EPDescr()				{ return EP_DESCR; }
		virtual unsigned AddRef()					{ return m_parent->AddRef(); }
		virtual unsigned Release()					{ return m_parent->Release(); }
		virtual signals::EType Type()				{ return m_parent->m_type; }
		virtual signals::IAttributes* Attributes()	{ return this; }

		signals::IEPBuffer* CreateBuffer();

	protected:
		CSplitterBase* m_parent;

	private:
		const static char* EP_NAME;
		const static char* EP_DESCR;
		CIncoming(const CIncoming& other);
		CIncoming& operator=(const CIncoming& other);
	};

private:
	static const char* NAME;
	const signals::EType m_type;
	void buildAttrs();

protected:
	virtual signals::IEPBuffer* CreateBuffer() = 0;
	CIncoming m_incoming;
	std::vector<COutgoing> m_outgoing;

	enum
	{
		IN_BUFFER_TIMEOUT = 1000,
		OUT_BUFFER_TIMEOUT = 1000,
	};
};

template<signals::EType ET, int DEFAULT_BUFSIZE = 4096>
class CSplitter : public CSplitterBase
{
public:
	inline CSplitter(signals::IBlockDriver* driver):CSplitterBase(ET,driver) {}

private:
	CSplitter(const CSplitter& other);
	CSplitter& operator=(const CSplitter& other);

protected:
	virtual signals::IEPBuffer* CreateBuffer()
	{
		signals::IEPBuffer* buffer = m_incoming.CreateBuffer();
		if(!buffer)
		{
			buffer = new CEPBuffer<ET>(DEFAULT_BUFSIZE);
			buffer->AddRef(NULL);
		}
		return buffer;
	}

	virtual void thread_run();
};

template<signals::EType ET, int DEFAULT_BUFSIZE = 4096>
class CSplitterDriver : public signals::IBlockDriver
{
public:
	inline CSplitterDriver() {}
	virtual ~CSplitterDriver() {}

private:
	CSplitterDriver(const CSplitterDriver& other);
	CSplitterDriver operator=(const CSplitterDriver& other);

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

template<signals::EType ET, int DEFAULT_BUFSIZE>
const char* CSplitterDriver<ET,DEFAULT_BUFSIZE>::NAME = "splitter";

template<signals::EType ET, int DEFAULT_BUFSIZE>
const char* CSplitterDriver<ET,DEFAULT_BUFSIZE>::DESCR = "Send a stream to multiple destinations";

template<signals::EType ET, int DEFAULT_BUFSIZE>
const unsigned char CSplitterDriver<ET,DEFAULT_BUFSIZE>::FINGERPRINT[]
	= { 1, (unsigned char)ET, 4, (unsigned char)ET, (unsigned char)ET, (unsigned char)ET, (unsigned char)ET };


// ------------------------------------------------------------------ class CFrameBuilderDriver

template<signals::EType ET, int DEFAULT_BUFSIZE>
signals::IBlock * CSplitterDriver<ET,DEFAULT_BUFSIZE>::Create()
{
	signals::IBlock* blk = new CSplitter<ET,DEFAULT_BUFSIZE>(this);
	blk->AddRef();
	return blk;
}

// ------------------------------------------------------------------ class CSplitter

template<signals::EType ET, int DEFAULT_BUFSIZE>
void CSplitter<ET,DEFAULT_BUFSIZE>::thread_run()
{
	ThreadBase::SetThreadName("Splitter Thread");

	typedef StoreType<ET>::type store_type;
	unsigned numEp = m_outgoing.size();
	std::vector<store_type> buffer(DEFAULT_BUFSIZE);
	while(threadRunning())
	{
		unsigned recvCount = m_incoming.Read(ET, buffer.data(), buffer.size(), FALSE, IN_BUFFER_TIMEOUT);
		if(recvCount)
		{
			for(unsigned idx = 0; idx < numEp; idx++)
			{
				COutgoing& here = m_outgoing[idx];
				if(here.isConnected())
				{
					unsigned outSent = here.Write(ET, buffer.data(), recvCount, OUT_BUFFER_TIMEOUT);
					if(outSent != recvCount)
					{
						here.attrs.sync_fault->fire();
					}
				}
			}
		}
	}
}
