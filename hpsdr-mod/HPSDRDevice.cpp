#include "stdafx.h"
#include "HPSDRDevice.h"

// ------------------------------------------------------------------ class CHpsdrDevice

const float CHpsdrDevice::SCALE_32 = float(1 << 31);
const float CHpsdrDevice::SCALE_16 = float(1 << 15);

CHpsdrDevice::CHpsdrDevice()
	:m_CCoutSet(0),m_CCoutPending(0),m_micSample(0),m_lastCCout(MAX_CC_OUT),m_CC0in(0)
{
	memset(m_CCout, 0, sizeof(m_CCout));
	memset(m_CCin, 0, sizeof(m_CCin));
}

bool CHpsdrDevice::receive_frame(byte* frame)
{
	const int sample_size = 6 * m_numReceiver + 2;

	// check that sync pulses are present in the front of rbuf...JAM
	if(*frame++ != SYNC || *frame++ != SYNC || *frame++ != SYNC) return false;

	byte CC0 = *frame++;
	m_CC0in = CC0;

	byte* recvCC = m_CCin + ((CC0 & 0xF8) >> 1);
	{
		Locker lock(m_CCinLock);
		*recvCC++ = *frame++;
		*recvCC++ = *frame++;
		*recvCC++ = *frame++;
		*recvCC++ = *frame++;
	}

	frame += 5;
	int remain = 504; // 512 - 8 bytes

	// get the I, and Q data from rbuf, convert to float, & put into SignalBuffer.cpx for DSP processing
	while(remain > sample_size)
	{
		// hopefully no one will notice that we're reading 8 bytes off the end of our array?
		// this is almost the definition of "unsafe"...
		for (unsigned recv = 0; recv < m_numReceiver; recv++)
		{
			signed iReal = (frame[0]<<24)|(frame[1]<<16)|(frame[2]<<8);
			frame += 3;
			signed iImag = (frame[0]<<24)|(frame[1]<<16)|(frame[2]<<8);
			frame += 3;
			remain -= 6;
			receive_sample(recv, iReal / SCALE_32, iImag / SCALE_32);
		}

		// Get a microphone sample & store it in a simple buffer.
		// If EP6 sample rate > 48k, the Mic samples received contain duplicates, which we'll ignore.
		// Thus the Mic samples are decimated before processing.  
		// The receive audio is fully processed at the higher sampling rate, and then is decimated.
		// By the time Data_send() is called, both streams are at 48ksps.
		// When the transmit buffer has reached capacity, all samples are processed, and placed in TransmitAudioRing.
		// 
		m_micSample += MIC_RATE;
		if (m_micSample >= m_sampleRate)
		{
			m_micSample = 0;
			short MicAmpl = short((frame[0] << 8) | frame[1]);
			// Place sample value in both real & imaginary parts since we do an FFT next.
			// When Mic AGC and speech processing are added, they should be done here, 
			// working with a stream of real samples.

			// TransmitFilter.Process() effects a relative phase shift between the real & imaginary parts.

			// In the following line, need (short) cast to ensure that the result is sign-extended
			// before being converted to float.
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

void CHpsdrDevice::setCCbyte(byte addr, byte offset, byte value)
{
	if(addr >= MAX_CC_OUT || offset >= 4) {ASSERT(FALSE);return;}

	Locker outLock(m_CCoutLock);
	byte* loc = m_CCout + (addr*4) + offset;
	*loc = value;

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
		float audLeft, audRight;
		speaker_out(&audLeft, &audRight);

		// send left & right data to the speakers on the receiver
		int IntValue = int(SCALE_16 * audLeft);
		*frame++ = (byte)(IntValue >> 8);    // left hi
		*frame++ = (byte)(IntValue & 0xff);  // left lo

		IntValue = int(SCALE_16 * audRight);
		*frame++ = (byte)(IntValue >> 8);    // right hi
		*frame++ = (byte)(IntValue & 0xff);  // right lo

		float xmitI, xmitQ;
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

const char* CHpsdrDevice::Receiver::HERMES_NAME = "Hermes (Receiver)";
const char* CHpsdrDevice::Receiver::MERCURY_NAME = "Mercury (Receiver)";
const char* CHpsdrDevice::Receiver::EP_NAME = "Received IQ Data";

unsigned CHpsdrDevice::Receiver::Outgoing(signals::IOutEndpoint** ep, unsigned availEP)
{
	if(ep && availEP)
	{
		AddRef();
		ep[0] = this;
	}
	return 1;
}

/*
signals::IEPBuffer* CreateBuffer();
*/

// ----------------------------------------------------------------------------

class CAttr_outBit : public CAttribute<byte, signals::etypBoolean>
{
public:
	CAttr_outBit(CHpsdrDevice& parent, const char* name, const char* descr, bool deflt, byte addr, byte offset, byte mask)
		:m_parent(parent),m_addr(addr),m_offset(offset),m_mask(mask),
		 CAttribute<byte,signals::etypBoolean>(name, descr, deflt ? 1 : 0)
	{
		if(deflt) m_parent.setCCbits(m_addr, m_offset, m_mask, m_mask);
	}

	virtual ~CAttr_outBit() { }

protected:
	virtual void onSetValue(const void* value)
	{
		const byte* newVal = (const byte*)value;
		m_parent.setCCbits(m_addr, m_offset, m_mask, !!*newVal ? m_mask : 0);
		CAttribute<byte,signals::etypBoolean>::onSetValue(value);
	}

private:
	CHpsdrDevice& m_parent;
	byte m_addr;
	byte m_offset;
	byte m_mask;
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
//	CAttributeBase* recv_speed;
	if(!hermes)
	{
//		CAttributeBase* src_10Mhz;
		attrs.src_122MHz = new CAttr_outBit(*this, "122MHzSource", "Yes: from Mercury, No: from Penny", true, 0, 0, 0x10);
		attrs.enable_penny = new CAttr_outBit(*this, "EnablePenny", "Yes to enable transmitter", true, 0, 0, 0x20);
		attrs.enable_merc = new CAttr_outBit(*this, "EnableMerc", "Yes to enable receivers", true, 0, 0, 0x40);
//		attrs.mic_src = new CAttr_outBit(*this, "MicSource", "Yes: from Penny, No: from Janus", true, 0, 0, 0x80);
	}
	attrs.class_e = new CAttr_outBit(*this, "Mode", "Yes: Class E, No: Other", true, 0, 1, 0x01);
//	CAttributeBase* send_open_collect;
//	CAttributeBase* alex_atten;
	attrs.recv_preamp = new CAttr_outBit(*this, "RecvPreamp", "Enable receiver preamp?", true, 0, 2, 0x04);
	attrs.recv_adc_dither = new CAttr_outBit(*this, "RecvAdcDither", "Enable receiver ADC dithering?", true, 0, 2, 0x08);
	attrs.recv_adc_random = new CAttr_outBit(*this, "RecvAdcRandom", "Enable receiver ADC random?", true, 0, 2, 0x10);
//	CAttributeBase* alex_recv_ant;
//	CAttributeBase* alex_send_relay;
//	CAttributeBase* timestamp;

//	CAttributeBase* recv_duplex;
//	CAttributeBase* recv_clone;
//	CAttributeBase* send_freq;
//	CAttributeBase* recv1_freq;
//	CAttributeBase* recv2_freq;
//	CAttributeBase* recv3_freq;
//	CAttributeBase* recv4_freq;
//	CAttributeBase* send_drive_level;
	attrs.mic_boost = new CAttr_outBit(*this, "MicBoost", "Boost microphone signal by 20dB?", false, 9, 1, 0x01);
	if(hermes)
	{
		attrs.apollo_filter_enable = new CAttr_outBit(*this, "ApolloFilterEnable", "Enable Apollo filter?", false, 9, 1, 0x04);
		attrs.apollo_tuner_enable = new CAttr_outBit(*this, "ApolloTunerEnable", "Enable Apollo tuner?", false, 9, 1, 0x08);
		attrs.apollo_auto_tune = new CAttr_outBit(*this, "ApolloAutoTune", "Start Apollo auto-tune?", false, 9, 1, 0x10);
		attrs.hermes_filter_select = new CAttr_outBit(*this, "FilterSelect", "Yes: Apollo, No: Alex", false, 9, 1, 0x20);
	}

	attrs.alex_filter_override = new CAttr_outBit(*this, "AlexFilterOverride", "Manually select Alex filers?", false, 9, 1, 0x40);
	attrs.alex_hpf_13MHz = new CAttr_outBit(*this, "AlexHPF13Mhz", "Select Alex 13 MHz HPF?", false, 9, 2, 0x01);
	attrs.alex_hpf_20MHz = new CAttr_outBit(*this, "AlexHPF20Mhz", "Select Alex 20 MHz HPF?", false, 9, 2, 0x02);
	attrs.alex_hpf_9500kHz = new CAttr_outBit(*this, "AlexHPF9500khz", "Select Alex 9.5 MHz HPF?", false, 9, 2, 0x04);
	attrs.alex_hpf_6500kHz = new CAttr_outBit(*this, "AlexHPF6500khz", "Select Alex 6.5 MHz HPF?", false, 9, 2, 0x08);
	attrs.alex_hpf_1500kHz = new CAttr_outBit(*this, "AlexHPF1500khz", "Select Alex 1.5 MHz HPF?", false, 9, 2, 0x10);
	attrs.alex_hpf_bypass = new CAttr_outBit(*this, "AlexHPFBypass", "Bypass all Alex HPF filters?", false, 9, 2, 0x20);
	attrs.alex_6m_amp = new CAttr_outBit(*this, "Alex6mAmp", "Enable Alex 6m low noise amplifier?", false, 9, 2, 0x40);
	attrs.alex_lpf_20m = new CAttr_outBit(*this, "AlexLPF20m", "Select Alex 30/20 m LPF?", false, 9, 3, 0x01);
	attrs.alex_lpf_40m = new CAttr_outBit(*this, "AlexLPF40m", "Select Alex 60/40 m LPF?", false, 9, 3, 0x02);
	attrs.alex_lpf_80m = new CAttr_outBit(*this, "AlexLPF80m", "Select Alex 80 m LPF?", false, 9, 3, 0x04);
	attrs.alex_lpf_160m = new CAttr_outBit(*this, "AlexLPF160m", "Select Alex 160 m LPF?", false, 9, 3, 0x08);
	attrs.alex_lpf_6m = new CAttr_outBit(*this, "AlexLPF6m", "Select Alex 6 m LPF?", false, 9, 3, 0x10);
	attrs.alex_lpf_10m = new CAttr_outBit(*this, "AlexLPF10m", "Select Alex 12/10 m LPF?", false, 9, 3, 0x20);
	attrs.alex_lpf_15m = new CAttr_outBit(*this, "AlexLPF15m", "Select Alex 17/15 m LPF?", false, 9, 3, 0x40);

	attrs.recv1_preamp = new CAttr_outBit(*this, "Recv1Preamp", "Enable receiver 1 preamp?", false, 10, 0, 0x01);
	attrs.recv2_preamp = new CAttr_outBit(*this, "Recv1Preamp", "Enable receiver 2 preamp?", false, 10, 0, 0x02);
	attrs.recv3_preamp = new CAttr_outBit(*this, "Recv1Preamp", "Enable receiver 3 preamp?", false, 10, 0, 0x04);
	attrs.recv4_preamp = new CAttr_outBit(*this, "Recv1Preamp", "Enable receiver 4 preamp?", false, 10, 0, 0x08);
}
