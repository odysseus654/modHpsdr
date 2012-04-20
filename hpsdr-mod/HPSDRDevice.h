#pragma once
#include "blockImpl.h"
#include "mt.h"
#include <vector>
typedef unsigned char byte;

class CHpsdrDevice : public CAttributesBase, public signals::IBlock
{
public:
	enum EBoardId { Ozy = -1, Metis = 0, Hermes = 1, Griffin = 2 };

	void setCCbits(byte addr, byte offset, byte mask, byte value);
	void setCCint(byte addr, unsigned long value);

private:
	CHpsdrDevice(const CHpsdrDevice& other);
	CHpsdrDevice& operator=(const CHpsdrDevice& other);

protected:
	explicit CHpsdrDevice(EBoardId boardId);

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
		CAttributeBase* recv_preamp;
		CAttributeBase* recv_adc_dither;
		CAttributeBase* recv_adc_random;
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
		CAttributeBase* alex_6m_amp;

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

		CAttributeBase* recv1_preamp;
		CAttributeBase* recv2_preamp;
		CAttributeBase* recv3_preamp;
		CAttributeBase* recv4_preamp;
	} attrs;

//	unsigned m_sampleRate;							// (config) incoming sample rate
//	unsigned m_numReceiver;							// (config) incoming receiver count

private:
	const static float SCALE_32;					// scale factor for converting 24-bit int from ADC to float
	const static float SCALE_16;

	enum
	{
		SYNC = 0x7F,
		MIC_RATE = 48000,							// all mic input is fixed at 48k regardless of IQ rate
		MAX_CC_OUT = 10
	};

	unsigned int m_micSample;						// private to process_data
	byte m_lastCCout;

	byte m_CCout[16*4];
	unsigned short m_CCoutSet;
	unsigned short m_CCoutPending;
	Lock m_CCoutLock;

	byte* chooseCC();

	volatile byte m_CC0in;
	byte m_CCin[32*4];
	Lock m_CCinLock;

protected:
	class Receiver : public signals::IBlock, public COutEndpointBase, public CAttributesBase
	{	// This class is assumed to be a static (non-dynamic) member of its parent
	public:
		inline Receiver(signals::IBlock* parent, EBoardId boardId):m_parent(parent),m_bIsHermes(boardId == Hermes) { }
		virtual ~Receiver();
		//void buildAttrs(CAttributesBase* parent, TAttrDef* attrs, unsigned numAttrs);
		void buildAttrs(const CHpsdrDevice& parent);

	protected:
		enum { DEFAULT_BUFSIZE = 4096 };
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
		virtual unsigned Children(signals::IBlock** blocks, unsigned availBlocks)	{ return 0; }
		virtual unsigned Incoming(signals::IInEndpoint** ep, unsigned availEP)		{ return 0; }
		virtual unsigned Outgoing(signals::IOutEndpoint** ep, unsigned availEP);
		virtual signals::IAttributes* Attributes()	{ return this; }
		virtual bool Start()						{ return m_parent->Start(); }
		virtual bool Stop()							{ return m_parent->Stop(); }
	public: // COutEndpointBase interface
		virtual signals::IBlock* Block()			{ AddRef(); return this; }
		virtual signals::EType Type()				{ return signals::etypComplex; }
		virtual const char* EPName()				{ return EP_NAME; }
		virtual signals::IEPBuffer* CreateBuffer()	{ return new CEPBuffer<signals::etypComplex>(DEFAULT_BUFSIZE); }

	private:
		Receiver(const Receiver& other);
		Receiver& operator=(const Receiver& other);
	};

protected:
	const EBoardId m_controllerType;
	std::vector<Receiver> m_receivers;

};
