#pragma once
#include <blockImpl.h>

class CDirectxWaterfallDriver : public signals::IBlockDriver
{
public:
	inline CDirectxWaterfallDriver() {}
	virtual ~CDirectxWaterfallDriver() {}

private:
	CDirectxWaterfallDriver(const CDirectxWaterfallDriver& other);
	CDirectxWaterfallDriver operator=(const CDirectxWaterfallDriver& other);

public:
	virtual const char* Name()			{ return NAME; }
	virtual const char* Description()	{ return DESCR; }
	virtual BOOL canCreate()			{ return true; }
	virtual BOOL canDiscover()			{ return false; }
	virtual unsigned Discover(signals::IBlock** blocks, unsigned availBlocks) { return 0; }
	virtual signals::IBlock* Create();
	virtual const unsigned char* Fingerprint() { return FINGERPRINT; }

protected:
	static const char* NAME;
	static const char* DESCR;
	static const unsigned char FINGERPRINT[];
};

class CDirectxWaterfall : public signals::IBlock, public CAttributesBase, protected CRefcountObject
{
public:
	CDirectxWaterfall(signals::IBlockDriver* driver);
	virtual ~CDirectxWaterfall() {}

private:
	CDirectxWaterfall(const CDirectxWaterfall& other);
	CDirectxWaterfall& operator=(const CDirectxWaterfall& other);

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
	virtual unsigned Outgoing(signals::IOutEndpoint** /* ep */ , unsigned /* availEP */) { return 0; }
	virtual void Start();
	virtual void Stop()						{ }

public:
	class CIncoming : public CInEndpointBase, public CAttributesBase
	{	// This class is assumed to be a static (non-dynamic) member of its parent
	public:
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
		virtual const char* EPName()				{ return EP_NAME; }
		virtual const char* EPDescr()				{ return EP_DESCR; }
		virtual unsigned AddRef()					{ return m_parent->AddRef(); }
		virtual unsigned Release()					{ return m_parent->Release(); }
		virtual signals::EType Type()				{ return signals::etypVecDouble; }
		virtual signals::IAttributes* Attributes()	{ return this; }

		virtual signals::IEPBuffer* CreateBuffer()
		{
			signals::IEPBuffer* buff = new CEPBuffer<signals::etypVecDouble>(DEFAULT_BUFSIZE);
			buff->AddRef(NULL);
			return buff;
		}

	protected:
		virtual void OnConnection(signals::IEPRecvFrom* recv);
	};

private:
	enum
	{
		IN_BUFFER_TIMEOUT = 1000
	};

	static const char* NAME;

	signals::IBlockDriver* m_driver;
	CIncoming m_incoming;
	Thread<CDirectxWaterfall*> m_dataThread;
	bool m_bDataThreadEnabled;
	Lock m_buffLock;
	unsigned m_bufSize;

protected:
	void buildAttrs();
	void onReceivedFrame(double* frame, unsigned size);
	static void process_thread(CDirectxWaterfall* owner);
};
