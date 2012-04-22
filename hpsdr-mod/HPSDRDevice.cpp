#include "stdafx.h"
#include "HPSDRDevice.h"

// ------------------------------------------------------------------ class CHpsdrDevice

const float CHpsdrDevice::SCALE_32 = float(1 << 31);
const float CHpsdrDevice::SCALE_16 = float(1 << 15);

#pragma warning(push)
#pragma warning(disable: 4355)
CHpsdrDevice::CHpsdrDevice(EBoardId boardId)
	:m_controllerType(boardId),m_CCoutSet(0),m_CCoutPending(0),m_micSample(0),m_lastCCout(MAX_CC_OUT),m_CC0in(0),
	 m_receivers(4),m_receiver1(this, 1),m_receiver2(this, 2),m_receiver3(this, 3),m_receiver4(this, 4),m_wideRecv(this)
{
	memset(m_CCout, 0, sizeof(m_CCout));
	memset(m_CCin, 0, sizeof(m_CCin));
	memset(m_CCinDirty, 0, sizeof(m_CCinDirty));
}
#pragma warning(pop)

bool CHpsdrDevice::receive_frame(byte* frame)
{
	// check that sync pulses are present in the front of rbuf...JAM
	if(*frame++ != SYNC || *frame++ != SYNC || *frame++ != SYNC) return false;

	// grab the received C&C code and dispatch the necessary notifiers
	byte CC0 = *frame++;
	m_CC0in = CC0;

	byte CCframe = ((CC0 & 0xF8) >> 1);
	byte* recvCC = m_CCin + CCframe;
	{
		Locker lock(m_CCinLock);
		*recvCC++ = *frame++;
		*recvCC++ = *frame++;
		*recvCC++ = *frame++;
		*recvCC++ = *frame++;
		m_CCinDirty[CCframe] = true;
		m_CCinUpdated.wakeAll();
	}

	int remain = 504; // 512 - 8 bytes

	static const unsigned int recv_speed_options[] = { 48, 96, 192 };
	ASSERT(attrs.recv_speed && attrs.recv_speed->Type() == signals::etypByte);
	unsigned char recvIdx = *(unsigned char*)attrs.recv_speed->getValue();
	unsigned int sampleRate = recv_speed_options[recvIdx];

	Locker recvLock(m_recvListLock);
	const std::vector<Receiver>::size_type numReceiver = max(1, m_receivers.size());
	const bool hasReceivers = !m_receivers.empty();
	const int sample_size = 6 * numReceiver + 2;

	while(remain > sample_size)
	{
		// receive a sample for each of the currentl-active receivers
		for (unsigned recv = 0; recv < numReceiver; recv++)
		{
			// we shift the 24bit sample by 32bits because it is a signed number
			signed iReal = (signed(frame[0])<<24)|(frame[1]<<16)|(frame[2]<<8);
			frame += 3;
			signed iImag = (signed(frame[0])<<24)|(frame[1]<<16)|(frame[2]<<8);
			frame += 3;
			remain -= 6;

			// a float contains 24 bits of precision,
			// dividing by 2^32 ensures we don't mess with the mantissa during the conversion
			if(hasReceivers)
			{
				std::complex<float> sample(iReal / SCALE_32, iImag / SCALE_32);
				metis_sync(!!m_receivers[recv].Write(signals::etypComplex, &sample, 1, 0));
			}
		}

		// technique taken from KK: ensure that no matter what the sample rate is we still get the
		// proper mic rate (using a form of error diffusion?)
		m_micSample += MIC_RATE;
		if (m_micSample >= sampleRate)
		{
			m_micSample = 0;

			// force the 16bit number to be signed and convert to float
			short MicAmpl = (short(frame[0]) << 8) | frame[1];
			mic_sample(MicAmpl / SCALE_16);
		}
		frame += 2;
		remain -= 2;
	}
	return true;
}

byte* CHpsdrDevice::chooseCC()
{	// ASSUMES m_CCoutLock IS HELD
	unsigned int CCoutMask = 1 << ++m_lastCCout;
	if(m_CCoutPending)
	{
		for(;;)
		{
			CCoutMask = CCoutMask << 1;
			if(++m_lastCCout > MAX_CC_OUT)
			{
				m_lastCCout = 0;
				CCoutMask = 1;
			}
			if(CCoutMask & m_CCoutPending)
			{
				m_CCoutPending &= ~CCoutMask;
				break;
			}
		}
	}
	else if(m_CCoutSet)
	{
		for(;;)
		{
			CCoutMask = CCoutMask << 1;
			if(++m_lastCCout > MAX_CC_OUT)
			{
				m_lastCCout = 0;
				CCoutMask = 1;
			}
			if(CCoutMask & m_CCoutSet) break;
		}
	}
	else
	{
		m_lastCCout = 0;
	}
	return &m_CCout[m_lastCCout*4];
}

void CHpsdrDevice::setCCbits(byte addr, byte offset, byte mask, byte value)
{
	if(addr >= MAX_CC_OUT || offset >= 4) {ASSERT(FALSE);return;}

	Locker outLock(m_CCoutLock);
	byte* loc = m_CCout + (addr*4) + offset;
	*loc = (*loc & ~mask) | (value & mask);

	unsigned int CCoutMask = 1 << addr;
	m_CCoutSet |= CCoutMask;
	m_CCoutPending |= CCoutMask;
}

void CHpsdrDevice::setCCint(byte addr, unsigned long value)
{
	if(addr >= MAX_CC_OUT) {ASSERT(FALSE);return;}

	Locker outLock(m_CCoutLock);
	byte* loc = m_CCout + (addr*4);

	*loc++ = (value >> 24) & 0xFF;
	*loc++ = (value >> 16) & 0xFF;
	*loc++ = (value >> 8) & 0xFF;
	*loc++ = value & 0xFF;

	unsigned int CCoutMask = 1 << addr;
	m_CCoutSet |= CCoutMask;
	m_CCoutPending |= CCoutMask;
}

void CHpsdrDevice::send_frame(byte* frame)
{
	*frame++ = SYNC;
	*frame++ = SYNC;
	*frame++ = SYNC;

	{
		bool MOX = false;
		Locker outLock(m_CCoutLock);
		byte* CC = chooseCC();
		*frame++ = ((m_lastCCout << 1) | (MOX ? 1 : 0));
		*frame++ = *CC++;
		*frame++ = *CC++;
		*frame++ = *CC++;
		*frame++ = *CC++;
	}

	for (int x = 8; x < 512; x += 8)        // fill out one 512-byte frame
	{
		float audLeft = 0.0f, audRight = 0.0f;
		speaker_out(&audLeft, &audRight);

		// send left & right data to the speakers on the receiver
		int IntValue = int(SCALE_16 * audLeft);
		*frame++ = (byte)(IntValue >> 8);    // left hi
		*frame++ = (byte)(IntValue & 0xff);  // left lo

		IntValue = int(SCALE_16 * audRight);
		*frame++ = (byte)(IntValue >> 8);    // right hi
		*frame++ = (byte)(IntValue & 0xff);  // right lo

		float xmitI = 0.0f, xmitQ = 0.0f;
		xmit_out(&xmitI, &xmitQ);

		// send I & Q data to the exciter
		IntValue = int(SCALE_16 * xmitI);
		*frame++ = (byte)(IntValue >> 8);    // right hi
		*frame++ = (byte)(IntValue & 0xff);  // right lo

		IntValue = int(SCALE_16 * xmitQ);
		*frame++ = (byte)(IntValue >> 8);    // right hi
		*frame++ = (byte)(IntValue & 0xff);  // right lo
	} // end for
}

// ------------------------------------------------------------------ class CHpsdrDevice::Receiver

const char* CHpsdrDevice::Receiver::EP_NAME[] = {"recv1", "recv2", "recv3", "recv4" };
const char* CHpsdrDevice::Receiver::EP_DESCR = "Received IQ Samples";

// ------------------------------------------------------------------ class CHpsdrDevice::WideReceiver

const char* CHpsdrDevice::WideReceiver::EP_NAME = "wide";
const char* CHpsdrDevice::WideReceiver::EP_DESCR = "Received wideband raw ADC samples";

// ----------------------------------------------------------------------------

template<signals::EType ET>
class COptionedAttribute : public CAttribute<ET>
{
public:
	COptionedAttribute(const char* name, const char* descr, byte deflt, unsigned numOpt, const char** optList)
		:CAttribute<ET>(name, descr, deflt), m_numOpts(optList ? numOpt : 0),m_optList(optList)
	{
	}

	virtual ~COptionedAttribute() { }

	virtual unsigned options(const char** opts, unsigned availElem)
	{
		if(opts && availElem)
		{
			unsigned numCopy = min(availElem, m_numOpts);
			for(unsigned idx=0; idx < numCopy; idx++)
			{
				opts[idx] = m_optList[idx];
			}
		}
		return m_numOpts;
	}

private:
	unsigned m_numOpts;
	const char** m_optList;
};

class CAttr_outBit : public CAttribute<signals::etypBoolean>
{
protected:
	typedef CAttribute<signals::etypBoolean> base;
public:
	CAttr_outBit(CHpsdrDevice& parent, const char* name, const char* descr, bool deflt, byte addr, byte offset, byte mask)
		:base(name, descr, deflt ? 1 : 0),m_parent(parent),m_addr(addr),m_offset(offset),m_mask(mask)
	{
		if(deflt) m_parent.setCCbits(m_addr, m_offset, m_mask, m_mask);
	}

	virtual unsigned options(const char** opts, unsigned availElem) { return 0; }
	virtual ~CAttr_outBit() { }

protected:
	virtual void nativeOnSetValue(const store_type& newVal)
	{
		m_parent.setCCbits(m_addr, m_offset, m_mask, !!newVal ? m_mask : 0);
		base::nativeOnSetValue(newVal);
	}

private:
	CHpsdrDevice& m_parent;
	byte m_addr;
	byte m_offset;
	byte m_mask;
};

class CAttr_outBits : public COptionedAttribute<signals::etypByte>
{
protected:
	typedef COptionedAttribute<signals::etypByte> base;
public:
	CAttr_outBits(CHpsdrDevice& parent, const char* name, const char* descr, byte deflt,
		byte addr, byte offset, byte mask, byte shift, unsigned numOpt, const char** optList)
		:base(name, descr, deflt, numOpt, optList),m_parent(parent),m_addr(addr),m_offset(offset),
		 m_mask(mask),m_shift(shift)
	{
		if(deflt) setValue(deflt);
	}

	virtual ~CAttr_outBits() { }

protected:
	virtual void nativeOnSetValue(const store_type& newVal)
	{
		setValue(newVal);
		base::nativeOnSetValue(newVal);
	}

private:
	CHpsdrDevice& m_parent;
	byte m_addr;
	byte m_offset;
	byte m_mask;
	byte m_shift;

	inline void setValue(const store_type& newVal)
	{
		m_parent.setCCbits(m_addr, m_offset, m_mask, (newVal << m_shift) & m_mask);
	}
};

class CAttr_out_alex_recv_ant : public COptionedAttribute<signals::etypByte>
{
protected:
	typedef COptionedAttribute<signals::etypByte> base;
public:
	CAttr_out_alex_recv_ant(CHpsdrDevice& parent, const char* name, const char* descr, byte deflt,
		unsigned numOpt, const char** optList)
		:base(name, descr, deflt, numOpt, optList),m_parent(parent)
	{
		if(deflt) setValue(deflt);
	}

	virtual ~CAttr_out_alex_recv_ant() { }

protected:
	virtual void nativeOnSetValue(const store_type& newVal)
	{
		setValue(newVal);
		base::nativeOnSetValue(newVal);
	}

private:
	CHpsdrDevice& m_parent;

	inline void setValue(const store_type& newVal)
	{
		m_parent.setCCbits(0, 2, 0xe0, newVal ? ((newVal << 5) & 0x60) | 0x80 : 0);
	}
};

class CAttr_out_mic_src : public COptionedAttribute<signals::etypByte>
{
protected:
	typedef COptionedAttribute<signals::etypByte> base;
public:
	CAttr_out_mic_src(CHpsdrDevice& parent, const char* name, const char* descr, byte deflt,
		unsigned numOpt, const char** optList)
		:base(name, descr, deflt, numOpt, optList),m_parent(parent)
	{
		if(deflt) setValue(deflt);
	}

	virtual ~CAttr_out_mic_src() { }

protected:
	virtual void nativeOnSetValue(const store_type& newVal)
	{
		setValue(newVal);
		base::nativeOnSetValue(newVal);
	}

private:
	CHpsdrDevice& m_parent;

	inline void setValue(const store_type& newVal)
	{
		m_parent.setCCbits(0, 0, 0x80, newVal == 0 ? 0 : 0x80);
		m_parent.setCCbits(9, 1, 0x02, newVal <= 1 ? 0 : 0x02);
	}
};

class CAttr_outLong : public CAttribute<signals::etypLong>
{
protected:
	typedef CAttribute<signals::etypLong> base;
public:
	CAttr_outLong(CHpsdrDevice& parent, const char* name, const char* descr, long deflt, byte addr)
		:base(name, descr, deflt),m_parent(parent),m_addr(addr)
	{
		if(deflt) setValue(deflt);
	}

	virtual ~CAttr_outLong() { }
	virtual unsigned options(const char** opts, unsigned availElem) { return 0; }

protected:
	virtual void nativeOnSetValue(const store_type& newVal)
	{
		setValue(newVal);
		base::nativeOnSetValue(newVal);
	}

private:
	CHpsdrDevice& m_parent;
	byte m_addr;

	inline void setValue(const store_type& newVal)
	{
		m_parent.setCCint(m_addr, newVal);
	}
};

void CHpsdrDevice::buildAttrs()
{
/*
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
*/

	// write-only
	static const char* recv_speed_options[] = {"48 kHz", "96 kHz", "192 kHz"};
	attrs.recv_speed = addLocalAttr(true, new CAttr_outBits(*this, "RecvRate", "Rate that receivers send data",
		2, 0, 0, 0x3, 0, _countof(recv_speed_options), recv_speed_options));
	if(m_controllerType != Hermes)
	{
		static const char* src_10MHz_options[] = {"Atlas/Excalibur", "Penny", "Mercury"};
		attrs.src_10Mhz = addLocalAttr(true, new CAttr_outBits(*this, "10MHzSource", "Select source of 10 MHz clock",
			2, 0, 0, 0x0c, 2, _countof(src_10MHz_options), src_10MHz_options));
		static const char* src_122MHz_options[] = {"Penny", "Mercury"};
		attrs.src_122MHz = addLocalAttr(true, new CAttr_outBits(*this, "122MHzSource", "Select source of 122 MHz clock",
			1, 0, 0, 0x10, 4, _countof(src_122MHz_options), src_122MHz_options));
		attrs.enable_penny = addLocalAttr(true, new CAttr_outBit(*this, "EnablePenny", "Enable transmitter?", true, 0, 0, 0x20));
		attrs.enable_merc = addLocalAttr(true, new CAttr_outBit(*this, "EnableMerc", "Enable receiver?", true, 0, 0, 0x40));
		static const char* src_mic_options[] = {"Janus", "Penny Mic", "Penny Line-in"};
		attrs.src_mic = addLocalAttr(true, new CAttr_out_mic_src(*this, "MicSource", "Microphone Source", 1,
			_countof(src_mic_options), src_mic_options));
	}
	static const char* class_e_options[] = {"Other", "Class E"};
	attrs.class_e = addLocalAttr(true, new CAttr_outBits(*this, "Mode", NULL,
		0, 0, 1, 0x1, 0, _countof(class_e_options), class_e_options));
	attrs.send_open_collect = addLocalAttr(true, new CAttr_outBits(*this, "OpenCollector", "Open collector outputs on Penny/Hermes",
		0, 0, 1, 0xfe, 1, 0, NULL));
	static const char* alex_atten_options[] = {"0 dB", "10 dB", "20 dB", "30 dB"};
	attrs.alex_atten = addLocalAttr(true, new CAttr_outBits(*this, "AlexAtten", "Alex Attenuator",
		0, 0, 2, 0x3, 0, _countof(alex_atten_options), alex_atten_options));
	attrs.recv_preamp = addLocalAttr(true, new CAttr_outBit(*this, "RecvPreamp", "Enable receiver preamp?", true, 0, 2, 0x04));
	attrs.recv_adc_dither = addLocalAttr(true, new CAttr_outBit(*this, "RecvAdcDither", "Enable receiver ADC dithering?", true, 0, 2, 0x08));
	attrs.recv_adc_random = addLocalAttr(true, new CAttr_outBit(*this, "RecvAdcRandom", "Enable receiver ADC random?", true, 0, 2, 0x10));
	static const char* alex_recv_ant[] = {"None", "Rx1", "Rx2", "XV"};
	attrs.alex_recv_ant = addLocalAttr(true, new CAttr_out_alex_recv_ant(*this, "AlexRecvAnt", "Alex Rx Antenna",
		0, _countof(alex_recv_ant), alex_recv_ant));
	static const char* alex_send_relay[] = {"Tx1", "Tx2", "Tx3"};
	attrs.alex_send_relay = addLocalAttr(true, new CAttr_outBits(*this, "AlexSendRelay", "Alex Tx relay",
		0, 0, 3, 0x3, 0, _countof(alex_send_relay), alex_send_relay));
//	attrs.timestamp = addLocalAttr(true, new CAttr_outBit(*this, "Timestamp", "Send 1pps through mic stream?", false, 0, 3, 0x40));

	attrs.recv_duplex = addLocalAttr(true, new CAttr_outBit(*this, "Duplex", "Duplex?", true, 0, 3, 0x04));
	attrs.recv_clone = addLocalAttr(true, new CAttr_outBit(*this, "RecvClone", "Common Mercury frequency?", false, 0, 3, 0x80));
	attrs.send_freq = addLocalAttr(true, new CAttr_outLong(*this, "SendFreq", "Transmit Frequency", 0, 1));
	attrs.recv1_freq = addLocalAttr(true, new CAttr_outLong(*this, "Recv1Freq", "Receiver 1 Frequency", 0, 2));
	attrs.recv2_freq = addLocalAttr(true, new CAttr_outLong(*this, "Recv2Freq", "Receiver 2 Frequency", 0, 3));
	attrs.recv3_freq = addLocalAttr(true, new CAttr_outLong(*this, "Recv3Freq", "Receiver 3 Frequency", 0, 4));
	attrs.recv4_freq = addLocalAttr(true, new CAttr_outLong(*this, "Recv4Freq", "Receiver 4 Frequency", 0, 5));
	attrs.send_drive_level = addLocalAttr(true, new CAttr_outBits(*this, "SendDriveLevel", "Hermes/PennyLane Drive Level",
		255, 9, 0, 0xFF, 0, 0, NULL));
	attrs.mic_boost = addLocalAttr(true, new CAttr_outBit(*this, "MicBoost", "Boost microphone signal by 20dB?", false, 9, 1, 0x01));
	if(m_controllerType == Hermes)
	{
		attrs.apollo_filter_enable = addLocalAttr(true, new CAttr_outBit(*this, "ApolloFilterEnable", "Enable Apollo filter?", false, 9, 1, 0x04));
		attrs.apollo_tuner_enable = addLocalAttr(true, new CAttr_outBit(*this, "ApolloTunerEnable", "Enable Apollo tuner?", false, 9, 1, 0x08));
		attrs.apollo_auto_tune = addLocalAttr(true, new CAttr_outBit(*this, "ApolloAutoTune", "Start Apollo auto-tune?", false, 9, 1, 0x10));
		static const char* hermes_filter_select_options[] = {"Alex", "Apollo"};
		attrs.hermes_filter_select = addLocalAttr(true, new CAttr_outBits(*this, "FilterSelect", NULL,
			0, 9, 1, 0x20, 5, _countof(hermes_filter_select_options), hermes_filter_select_options));
	}

	attrs.alex_filter_override = addLocalAttr(true, new CAttr_outBit(*this, "AlexFilterOverride", "Manually select Alex filers?", false, 9, 1, 0x40));
	attrs.alex_hpf_13MHz = addLocalAttr(true, new CAttr_outBit(*this, "AlexHPF13Mhz", "Select Alex 13 MHz HPF?", false, 9, 2, 0x01));
	attrs.alex_hpf_20MHz = addLocalAttr(true, new CAttr_outBit(*this, "AlexHPF20Mhz", "Select Alex 20 MHz HPF?", false, 9, 2, 0x02));
	attrs.alex_hpf_9500kHz = addLocalAttr(true, new CAttr_outBit(*this, "AlexHPF9500khz", "Select Alex 9.5 MHz HPF?", false, 9, 2, 0x04));
	attrs.alex_hpf_6500kHz = addLocalAttr(true, new CAttr_outBit(*this, "AlexHPF6500khz", "Select Alex 6.5 MHz HPF?", false, 9, 2, 0x08));
	attrs.alex_hpf_1500kHz = addLocalAttr(true, new CAttr_outBit(*this, "AlexHPF1500khz", "Select Alex 1.5 MHz HPF?", false, 9, 2, 0x10));
	attrs.alex_hpf_bypass = addLocalAttr(true, new CAttr_outBit(*this, "AlexHPFBypass", "Bypass all Alex HPF filters?", false, 9, 2, 0x20));
	attrs.alex_6m_amp = addLocalAttr(true, new CAttr_outBit(*this, "Alex6mAmp", "Enable Alex 6m low noise amplifier?", false, 9, 2, 0x40));
	attrs.alex_lpf_20m = addLocalAttr(true, new CAttr_outBit(*this, "AlexLPF20m", "Select Alex 30/20 m LPF?", false, 9, 3, 0x01));
	attrs.alex_lpf_40m = addLocalAttr(true, new CAttr_outBit(*this, "AlexLPF40m", "Select Alex 60/40 m LPF?", false, 9, 3, 0x02));
	attrs.alex_lpf_80m = addLocalAttr(true, new CAttr_outBit(*this, "AlexLPF80m", "Select Alex 80 m LPF?", false, 9, 3, 0x04));
	attrs.alex_lpf_160m = addLocalAttr(true, new CAttr_outBit(*this, "AlexLPF160m", "Select Alex 160 m LPF?", false, 9, 3, 0x08));
	attrs.alex_lpf_6m = addLocalAttr(true, new CAttr_outBit(*this, "AlexLPF6m", "Select Alex 6 m LPF?", false, 9, 3, 0x10));
	attrs.alex_lpf_10m = addLocalAttr(true, new CAttr_outBit(*this, "AlexLPF10m", "Select Alex 12/10 m LPF?", false, 9, 3, 0x20));
	attrs.alex_lpf_15m = addLocalAttr(true, new CAttr_outBit(*this, "AlexLPF15m", "Select Alex 17/15 m LPF?", false, 9, 3, 0x40));

	attrs.recv1_preamp = addLocalAttr(true, new CAttr_outBit(*this, "Recv1Preamp", "Enable receiver 1 preamp?", false, 10, 0, 0x01));
	attrs.recv2_preamp = addLocalAttr(true, new CAttr_outBit(*this, "Recv1Preamp", "Enable receiver 2 preamp?", false, 10, 0, 0x02));
	attrs.recv3_preamp = addLocalAttr(true, new CAttr_outBit(*this, "Recv1Preamp", "Enable receiver 3 preamp?", false, 10, 0, 0x04));
	attrs.recv4_preamp = addLocalAttr(true, new CAttr_outBit(*this, "Recv1Preamp", "Enable receiver 4 preamp?", false, 10, 0, 0x08));

	m_receiver1.buildAttrs(*this);
	m_receiver2.buildAttrs(*this);
	m_receiver3.buildAttrs(*this);
	m_receiver4.buildAttrs(*this);
	m_wideRecv.buildAttrs(*this);
}
