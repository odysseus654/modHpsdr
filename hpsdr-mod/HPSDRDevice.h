#pragma once
#include "blockImpl.h"
typedef unsigned char byte;

class CHpsdrDevice : public CAttributesBase
{
protected:
	bool receive_frame(byte* frame);
	void send_frame(byte* frame);
	void buildAttrs();

	struct
	{
		// high-priority read-only
		CAttributeBase* PPT;
		CAttributeBase* DASH;
		CAttributeBase* DOT;

		// medium-priority read-only
		CAttributeBase* recvOverflow;
		CAttributeBase* hermes_I01;
		CAttributeBase* hermes_I02;
		CAttributeBase* hermes_I03;
		CAttributeBase* merc_software;
		CAttributeBase* penny_software;
		CAttributeBase* ozy_software;
		CAttributeBase* AIN1;
		CAttributeBase* AIN2;
		CAttributeBase* AIN3;
		CAttributeBase* AIN4;
		CAttributeBase* AIN5;
		CAttributeBase* AIN6;
		CAttributeBase* recv1_overflow;
		CAttributeBase* recv1_version;
		CAttributeBase* recv2_overflow;
		CAttributeBase* recv2_version;
		CAttributeBase* recv3_overflow;
		CAttributeBase* recv3_version;
		CAttributeBase* recv4_overflow;
		CAttributeBase* recv4_version;

		// write-only
		CAttributeBase* enable_penny;
		CAttributeBase* enable_merc;
		CAttributeBase* recv_speed;
		CAttributeBase* src_10Mhz;
		CAttributeBase* src_122MHz;
		CAttributeBase* src_mic;
		CAttributeBase* class_e;
		CAttributeBase* send_open_collect;
		CAttributeBase* alex_atten;
		CAttributeBase* recv_atten;
		CAttributeBase* recv_dither;
		CAttributeBase* recv_random;
		CAttributeBase* alex_recv_ant;
		CAttributeBase* alex_send_relay;
		CAttributeBase* timestamp;

		CAttributeBase* recv_duplex;
		CAttributeBase* recv_clone;
		CAttributeBase* send_freq;
		CAttributeBase* recv1_freq;
		CAttributeBase* recv2_freq;
		CAttributeBase* recv3_freq;
		CAttributeBase* recv4_freq;
		CAttributeBase* send_drive_level;
		CAttributeBase* mic_boost;
		CAttributeBase* apollo_filter_enable;
		CAttributeBase* apollo_tuner_enable;
		CAttributeBase* apollo_auto_tune;
		CAttributeBase* hermes_filter_select;

		CAttributeBase* alex_filter_override;
		CAttributeBase* alex_hpf_13MHz;
		CAttributeBase* alex_hpf_20MHz;
		CAttributeBase* alex_hpf_9500kHz;
		CAttributeBase* alex_hpf_6500kHz;
		CAttributeBase* alex_hpf_1500kHz;
		CAttributeBase* alex_hpf_bypass;
		CAttributeBase* alex_lpf_20m;
		CAttributeBase* alex_lpf_40m;
		CAttributeBase* alex_lpf_80m;
		CAttributeBase* alex_lpf_160m;
		CAttributeBase* alex_lpf_6m;
		CAttributeBase* alex_lpf_10m;
		CAttributeBase* alex_lpf_15m;

		CAttributeBase* recv1_atten;
		CAttributeBase* recv2_atten;
		CAttributeBase* recv3_atten;
		CAttributeBase* recv4_atten;
	} attrs;

	enum
	{
		SYNC = 0x7F,
		MIC_RATE = 48000
	};
	const static float SCALE_32;				// scale factor for converting 24-bit int from ADC to float
	const static float SCALE_16;

	unsigned m_sampleRate;							// (config) incoming sample rate
	unsigned m_numReceiver;							// (config) incoming receiver count

//	volatile byte m_recvCC0;
	unsigned m_micSample;							// private to process_data

protected:
	class Receiver : public signals::IBlock, public COutEndpointBase, public CAttributesBase
	{	// This class is assumed to be a static (non-dynamic) member of its parent
	public:
		inline Receiver(signals::IBlock* parent, bool bIsHermes):m_parent(parent),m_bIsHermes(bIsHermes) { }
		virtual ~Receiver();
		//void buildAttrs(CAttributesBase* parent, TAttrDef* attrs, unsigned numAttrs);
		void buildAttrs(const CHpsdrDevice& parent);

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
