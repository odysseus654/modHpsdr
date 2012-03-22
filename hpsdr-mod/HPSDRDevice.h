#pragma once
#include "blockImpl.h"
typedef unsigned char byte;

class CHpsdrDevice
{
protected:
	bool receive_frame(byte* frame);
	void send_frame(byte* frame);

	enum
	{
		SYNC = 0x7F,
		MIC_RATE = 48000
	};
	const static float  SCALE_32;				// scale factor for converting 24-bit int from ADC to float
	const static float  SCALE_16;

	unsigned m_sampleRate;							// (config) incoming sample rate
	unsigned m_numReceiver;							// (config) incoming receiver count

	volatile byte m_recvCC0;
	unsigned m_micSample;							// private to process_data

protected:
	class Receiver : public signals::IBlock, public COutEndpointBase, public CAttributesBase
	{	// This class is assumed to be a static (non-dynamic) member of its parent
	public:
		inline Receiver(signals::IBlock* parent, bool bIsHermes):m_parent(parent),m_bIsHermes(bIsHermes) { }
		virtual ~Receiver();
		//void buildAttrs(CAttributesBase* parent, TAttrDef* attrs, unsigned numAttrs);

	protected:
		signals::IBlock* m_parent;
		bool m_bIsHermes;

	private:
		const static char* HERMES_NAME;
		const static char* MERCURY_NAME;
		const static char* EP_NAME;

	public: // IBlock interface
		virtual unsigned AddRef()					{ return m_parent->AddRef(); }
		virtual unsigned Release()					{ return m_parent->Release(); }
		virtual const char* Name()					{ return m_bIsHermes ? HERMES_NAME : MERCURY_NAME; }
		virtual signals::IBlockDriver* Driver()		{ return m_parent->Driver(); }
		virtual signals::IBlock* Parent()			{ m_parent->AddRef(); return m_parent; }
		virtual unsigned numChildren()				{ return 0; }
		virtual unsigned Children(signals::IBlock** blocks, unsigned availBlocks)	{ return 0; }
		virtual unsigned numIncoming()				{ return 0; }
		virtual unsigned Incoming(signals::IInEndpoint** ep, unsigned availEP)		{ return 0; }
		virtual unsigned numOutgoing()				{ return 1; }
		virtual unsigned Outgoing(signals::IOutEndpoint** ep, unsigned availEP);
		virtual signals::IAttributes* Attributes()	{ return this; }
		virtual bool Start()						{ return m_parent->Start(); }
		virtual bool Stop()							{ return m_parent->Stop(); }
	public: // COutEndpointBase interface
		virtual signals::IBlock* Block()			{ AddRef(); return this; }
		virtual signals::EType Type()				{ return signals::etypComplex; }
		virtual const char* EPName()				{ return EP_NAME; }
		//virtual signals::IEPBuffer* CreateBuffer();
	};
};
