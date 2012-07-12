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
#include "blockImpl.h"
#include "mt.h"
#include <vector>
typedef unsigned char byte;

namespace hpsdr {

class CAttr_outBits;

class CHpsdrDevice : public CAttributesBase, public signals::IBlock
{
public:
	class Receiver;
	virtual ~CHpsdrDevice();

	enum EBoardId { Ozy = -1, Metis = 0, Hermes = 1, Griffin = 2 };

	void setCCbits(byte addr, byte offset, byte mask, byte value);
	void setCCint(byte addr, unsigned long value);

private:
	CHpsdrDevice(const CHpsdrDevice& other);
	CHpsdrDevice& operator=(const CHpsdrDevice& other);

public: // interface
	virtual signals::IBlock* Block() { return this; }		// from CAttributesBase

protected:
	explicit CHpsdrDevice(EBoardId boardId);

	unsigned receive_frame(byte* frame);
	void send_frame(byte* frame, bool no_streams = false);
	void buildAttrs();

	short AttachReceiver(Receiver& recv);
	void DetachReceiver(const Receiver& recv, unsigned short attachedRecv);

	struct
	{
		// events
		CEventAttribute* sync_fault;
		CEventAttribute* sync_mic_fault;
		CEventAttribute* wide_sync_fault;

		// high-priority read-only
		CAttributeBase* PPT;
		CAttributeBase* DASH;
		CAttributeBase* DOT;

		// medium-priority read-only
		CAttributeBase* recv_overflow;
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
		CAttributeBase* recvX_overflow[4];
		CAttributeBase* recvX_version[4];

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
		CAttr_outBits*  num_recv;
		CAttributeBase* recv_clone;
		CAttributeBase* send_freq;
		CRWAttribute<signals::etypLong>* recvX_freq[4];
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

		CRWAttribute<signals::etypBoolean>* recvX_preamp[4];
	} attrs;

public:
	class Receiver : public COutEndpointBase, public CAttributesBase
	{	// This class is assumed to be a static (non-dynamic) member of its parent
	public:
		inline Receiver(CHpsdrDevice* parent, short recvNum):m_parent(parent),m_recvNum(recvNum),m_attachedRecv(-1) { }
		virtual ~Receiver() {}
		void buildAttrs(const CHpsdrDevice& parent);
		void setProxy(CHpsdrDevice& parent, short attachedRecv);

	public:
		template<class target_type>
		class IAttrProxy
		{
		public:
			virtual void setProxy(target_type& target) PURE;
		};

	protected:
		enum { DEFAULT_BUFSIZE = 192000 };
		CHpsdrDevice* m_parent;
		const short m_recvNum;
		short m_attachedRecv;

		struct
		{
			CEventAttribute* sync_fault;
			CAttributeBase* rate;
			IAttrProxy<CRWAttribute<signals::etypBoolean> >* preamp;
			IAttrProxy<CRWAttribute<signals::etypLong> >* freq;
			IAttrProxy<signals::IAttribute>* overflow;
			IAttrProxy<signals::IAttribute>* version;
		} attrs;

	private:
		const static char* EP_NAME[];
		const static char* EP_DESCR;
		Receiver(const Receiver& other);
		Receiver& operator=(const Receiver& other);

	public: // COutEndpointBase interface
		virtual signals::IBlock* Block()			{ m_parent->AddRef(); return m_parent; }
		virtual signals::EType Type()				{ return signals::etypComplex; }
		virtual const char* EPName()				{ return EP_DESCR; }
		virtual signals::IEPBuffer* CreateBuffer()	{ return new CEPBuffer<signals::etypComplex>(DEFAULT_BUFSIZE); }
		virtual signals::IAttributes* Attributes()	{ return this; }
		virtual BOOL Connect(signals::IEPSender* send);
		virtual BOOL Disconnect();
	};

	class Microphone : public COutEndpointBase, public CAttributesBase
	{	// This class is assumed to be a static (non-dynamic) member of its parent
	public:
		inline Microphone(signals::IBlock* parent):m_parent(parent) { }
		virtual ~Microphone() {}
		void buildAttrs(const CHpsdrDevice& parent);

	protected:
		enum { DEFAULT_BUFSIZE = 48000 };
		signals::IBlock* m_parent;

		struct
		{
			CEventAttribute* sync_fault;
			CAttributeBase* rate;
			CAttributeBase* source;
		} attrs;

	private:
		const static char* EP_NAME;
		const static char* EP_DESCR;
		Microphone(const Microphone& other);
		Microphone& operator=(const Microphone& other);

	public: // COutEndpointBase interface
		virtual signals::IBlock* Block()			{ m_parent->AddRef(); return m_parent; }
		virtual signals::EType Type()				{ return signals::etypSingle; }
		virtual const char* EPName()				{ return EP_DESCR; }
		virtual signals::IEPBuffer* CreateBuffer()	{ return new CEPBuffer<signals::etypSingle>(DEFAULT_BUFSIZE); }
		virtual signals::IAttributes* Attributes()	{ return this; }
	};

	class WideReceiver : public COutEndpointBase, public CAttributesBase
	{	// This class is assumed to be a static (non-dynamic) member of its parent
	public:
		inline WideReceiver(signals::IBlock* parent):m_parent(parent) { }
		virtual ~WideReceiver() {}
		void buildAttrs(const CHpsdrDevice& parent);

	protected:
		enum { DEFAULT_BUFSIZE = 48000 };
		signals::IBlock* m_parent;

		struct
		{
			CEventAttribute* sync_fault;
			CAttributeBase* rate;
		} attrs;

	private:
		const static char* EP_NAME;
		const static char* EP_DESCR;
		WideReceiver(const Receiver& other);
		WideReceiver& operator=(const Receiver& other);

	public: // COutEndpointBase interface
		virtual signals::IBlock* Block()			{ m_parent->AddRef(); return m_parent; }
		virtual signals::EType Type()				{ return signals::etypSingle; }
		virtual const char* EPName()				{ return EP_DESCR; }
		virtual signals::IEPBuffer* CreateBuffer()	{ return new CEPBuffer<signals::etypSingle>(DEFAULT_BUFSIZE); }
		virtual signals::IAttributes* Attributes()	{ return this; }
	};

	class Transmitter : public CInEndpointBase, public CAttributesBase
	{	// This class is assumed to be a static (non-dynamic) member of its parent
	public:
		inline Transmitter(signals::IBlock* parent):m_parent(parent) { }
		virtual ~Transmitter() {}
		void buildAttrs(const CHpsdrDevice& parent);

	protected:
		enum { DEFAULT_BUFSIZE = 48000 };
		signals::IBlock* m_parent;

		struct
		{
			CAttributeBase* rate;
			CAttributeBase* version;
		} attrs;

	private:
		const static char* EP_NAME;
		const static char* EP_DESCR;
		Transmitter(const Transmitter& other);
		Transmitter& operator=(const Transmitter& other);

	public: // CInEndpointBase interface
		virtual const char* EPName()				{ return EP_DESCR; }
		virtual unsigned AddRef()					{ return m_parent->AddRef(); }
		virtual unsigned Release()					{ return m_parent->Release(); }
		virtual signals::IBlock* Block()			{ m_parent->AddRef(); return m_parent; }
		virtual signals::EType Type()				{ return signals::etypComplex; }
		virtual signals::IAttributes* Attributes()	{ return this; }
		virtual signals::IEPBuffer* CreateBuffer()	{ return new CEPBuffer<signals::etypComplex>(DEFAULT_BUFSIZE); }
	};

	class Speaker : public CInEndpointBase, public CAttributesBase
	{	// This class is assumed to be a static (non-dynamic) member of its parent
	public:
		inline Speaker(signals::IBlock* parent):m_parent(parent) { }
		virtual ~Speaker() {}
		void buildAttrs(const CHpsdrDevice& parent);

	protected:
		enum { DEFAULT_BUFSIZE = 48000 };
		signals::IBlock* m_parent;

		struct
		{
			CAttributeBase* rate;
		} attrs;

	private:
		const static char* EP_NAME;
		const static char* EP_DESCR;
		Speaker(const Speaker& other);
		Speaker& operator=(const Speaker& other);

	public: // CInEndpointBase interface
		virtual const char* EPName()				{ return EP_DESCR; }
		virtual unsigned AddRef()					{ return m_parent->AddRef(); }
		virtual unsigned Release()					{ return m_parent->Release(); }
		virtual signals::IBlock* Block()			{ m_parent->AddRef(); return m_parent; }
		virtual signals::EType Type()				{ return signals::etypLRSingle; }
		virtual signals::IAttributes* Attributes()	{ return this; }
		virtual signals::IEPBuffer* CreateBuffer()	{ return new CEPBuffer<signals::etypLRSingle>(DEFAULT_BUFSIZE); }
	};

public:
	class CAttr_inMonitor
	{
	public:
		inline CAttr_inMonitor(byte addr):m_addr(addr) {}
		virtual void evaluate(byte* inVal) PURE;

		const byte m_addr;
	private:
		CAttr_inMonitor(const CAttr_inMonitor&);
		CAttr_inMonitor& operator=(const CAttr_inMonitor&);
	};

private:
	void thread_attr();

	enum
	{
		SYNC = 0x7F,
		MAX_CC_OUT = 10
	};

	Thread<> m_attrThread;
	volatile bool m_attrThreadEnabled;
	unsigned int m_micSample;						// private to process_data

	byte m_lastCCout;								// protected by m_CCoutLock
	byte m_CCout[16*4];								// protected by m_CCoutLock
	unsigned short m_CCoutSet;						// protected by m_CCoutLock
	unsigned short m_CCoutPending;					// protected by m_CCoutLock
	Lock m_CCoutLock;

	byte* chooseCC();

	volatile byte m_CC0in;
	byte m_CCin[32*4];								// protected by m_CCinLock
	unsigned int m_CCinDirty;						// protected by m_CCinLock
	Condition m_CCinUpdated;						// protected by m_CCinLock
	Lock m_CCinLock;

	typedef std::list<CAttr_inMonitor*> TInAttrList;
	TInAttrList m_inAttrs;

protected:
	template<class ATTR> ATTR* addLocalInAttr(bool bVisible, ATTR* attr);
	inline bool outPendingExists() const { return !!m_CCoutPending; }

	friend class CAttr_out_recv_speed;

	enum
	{
		MIC_RATE = 48								// all mic input is fixed at 48k regardless of IQ rate
	};

	const static float SCALE_32;					// scale factor for converting 24-bit int from ADC to float
	const static float SCALE_16;

	unsigned int m_recvSpeed;
	const EBoardId m_controllerType;

	std::vector<Receiver*> m_receivers;				// protected by m_recvListLock
	Lock m_recvListLock;

	Receiver m_receiver1, m_receiver2, m_receiver3, m_receiver4;
	WideReceiver m_wideRecv;
	Microphone m_microphone;
	Speaker m_speaker;
	Transmitter m_transmit;
};

// ------------------------------------------------------------------------------------------------

template<class ATTR> ATTR* CHpsdrDevice::addLocalInAttr(bool bVisible, ATTR* attr)
{
	if(attr)
	{
		addLocalAttr(bVisible, attr);
		m_inAttrs.push_back(attr);
	}
	return attr;
}

}