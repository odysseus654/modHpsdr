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

class CIdentityBase : public CThreadBlockBase
{
public:
	CIdentityBase(signals::EType type, signals::IBlockDriver* driver);

private:
	CIdentityBase(const CIdentityBase& other);
	CIdentityBase& operator=(const CIdentityBase& other);

public: // IBlock implementation
	virtual const char* Name()				{ return NAME; }
	virtual unsigned Incoming(signals::IInEndpoint** ep, unsigned availEP) { return singleIncoming(&m_incoming, ep, availEP); }
	virtual unsigned Outgoing(signals::IOutEndpoint** ep, unsigned availEP) { return singleOutgoing(&m_outgoing, ep, availEP); }

public:
	class CIncoming;

	class COutgoing : public COutEndpointBase, public CCascadedAttributesBase<CInEndpointBase>
	{	// This class is assumed to be a static (non-dynamic) member of its parent
	public:
		inline COutgoing(CIdentityBase* parent):CCascadedAttributesBase(parent->m_incoming),m_parent(parent) { }
		inline COutgoing(const COutgoing& other):CCascadedAttributesBase(other.m_parent->m_incoming),m_parent(other.m_parent) { }
		void buildAttrs(const CIdentityBase& parent);

		virtual BOOL Connect(signals::IEPSendTo* send)
		{
			return COutEndpointBase::Connect(send);
		}
	protected:
		CIdentityBase* m_parent;

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

	class CIncoming : public CInEndpointBase, public CCascadedAttributesBase<COutEndpointBase>
	{	// This class is assumed to be a static (non-dynamic) member of its parent
	public:
		inline CIncoming(CIdentityBase* parent):CCascadedAttributesBase(parent->m_outgoing),m_parent(parent) { }
		virtual const char* EPName()				{ return EP_NAME; }
		virtual const char* EPDescr()				{ return EP_DESCR; }
		virtual unsigned AddRef()					{ return m_parent->AddRef(); }
		virtual unsigned Release()					{ return m_parent->Release(); }
		virtual signals::EType Type()				{ return m_parent->m_type; }
		virtual signals::IAttributes* Attributes()	{ return this; }

		signals::IEPBuffer* CreateBuffer();

	protected:
		CIdentityBase* m_parent;

	private:
		const static char* EP_NAME;
		const static char* EP_DESCR;
		CIncoming(const CIncoming& other);
		CIncoming& operator=(const CIncoming& other);
	};

private:
	static const char* NAME;
	const signals::EType m_type;

	inline void buildAttrs() { m_outgoing.buildAttrs(*this); }

protected:
	virtual signals::IEPBuffer* CreateBuffer() = 0;
	CIncoming m_incoming;
	COutgoing m_outgoing;

	enum
	{
		IN_BUFFER_TIMEOUT = 1000,
		OUT_BUFFER_TIMEOUT = 1000,
	};
};

template<signals::EType ET, int DEFAULT_BUFSIZE = 4096>
class CIdentity : public CIdentityBase
{
public:
	inline CIdentity(signals::IBlockDriver* driver):CIdentityBase(ET,driver) {}

private:
	CIdentity(const CIdentity& other);
	CIdentity& operator=(const CIdentity& other);

protected:
	enum { is_vector = StoreType<ET>::is_vector };

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
class CIdentityDriver : public signals::IBlockDriver
{
public:
	inline CIdentityDriver() {}
	virtual ~CIdentityDriver() {}

private:
	CIdentityDriver(const CIdentityDriver& other);
	CIdentityDriver operator=(const CIdentityDriver& other);

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
const char* CIdentityDriver<ET,DEFAULT_BUFSIZE>::NAME = "=";

template<signals::EType ET, int DEFAULT_BUFSIZE>
const char* CIdentityDriver<ET,DEFAULT_BUFSIZE>::DESCR = "Dummy module for additional thread isolation";

template<signals::EType ET, int DEFAULT_BUFSIZE>
const unsigned char CIdentityDriver<ET,DEFAULT_BUFSIZE>::FINGERPRINT[]
	= { 1, (unsigned char)ET, 1, (unsigned char)ET };


// ------------------------------------------------------------------ class CIdentityDriver

template<signals::EType ET, int DEFAULT_BUFSIZE>
signals::IBlock * CIdentityDriver<ET,DEFAULT_BUFSIZE>::Create()
{
	signals::IBlock* blk = new CIdentity<ET,DEFAULT_BUFSIZE>(this);
	blk->AddRef();
	return blk;
}

// ------------------------------------------------------------------ class CIdentity

template<signals::EType ET, int DEFAULT_BUFSIZE>
void CIdentity<ET,DEFAULT_BUFSIZE>::thread_run()
{
	ThreadBase::SetThreadName("Identity Thread");

	typedef StoreType<ET>::type store_type;
	std::vector<store_type> buffer(DEFAULT_BUFSIZE);
	while(threadRunning())
	{
		unsigned recvCount = m_incoming.Read(ET, buffer.data(), buffer.size(), FALSE, IN_BUFFER_TIMEOUT);
		ASSERT(recvCount <= buffer.size());
		if(recvCount)
		{
			if(m_outgoing.isConnected())
			{
				unsigned outSent = m_outgoing.Write(ET, buffer.data(), recvCount, OUT_BUFFER_TIMEOUT);
				if(outSent != recvCount)
				{
					m_outgoing.attrs.sync_fault->fire();
					if(is_vector)
					{
						for(unsigned elm=outSent; elm < recvCount; elm++) (*(signals::IVector**)&buffer[elm])->Release();
					}
				}
			}
			else if(is_vector)
			{
				for(unsigned elm=0; elm < recvCount; elm++) (*(signals::IVector**)&buffer[elm])->Release();
			}
		}
	}
}
