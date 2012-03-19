#pragma once
#include "block.h"
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
	class Receiver : public signals::IBlock, public signals::IOutEndpoint, public signals::IAttributes
	{	// This class is assumed to be a static (non-dynamic) member of its parent
	public:
		inline Receiver(signals::IBlock* parent):m_parent(parent) { }
		virtual ~Receiver();

	protected:
		signals::IBlock* m_parent;

	public: // IBlock interface
		virtual unsigned AddRef()					{ return m_parent->AddRef(); }
		virtual unsigned Release()					{ return m_parent->Release(); }
		virtual const char* Name() = 0;
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
	public: // IOutEndpoint interface
		virtual signals::IBlock* Block()			{ AddRef(); return this; }
		virtual signals::EType Type()				{ return signals::etypComplex; }
		//virtual signals::IAttributes* Attributes();
		virtual bool Connect(signals::IEPSender* send);
		virtual bool isConnected();
		virtual bool Disconnect();
		virtual signals::IEPBuffer* CreateBuffer();
	public: // IAttributes interface
		virtual unsigned numAttributes();
		virtual unsigned Itemize(signals::IAttribute* attrs, unsigned availElem);
		virtual signals::IAttribute* GetByName(char* name);
		virtual void Attach(signals::IAttributeObserver* obs);
		virtual void Detach(signals::IAttributeObserver* obs);
		//virtual signals::IBlock* Block();
	};
};
