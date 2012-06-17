#include "stdafx.h"
#include "HPSDRDevice.h"

#include "error.h"
#include "HPSDRAttrs.h"

namespace hpsdr {

// ------------------------------------------------------------------ class CHpsdrDevice

const float CHpsdrDevice::SCALE_32 = float(1U << 31);
const float CHpsdrDevice::SCALE_16 = float(1U << 15);

#pragma warning(push)
#pragma warning(disable: 4355)
CHpsdrDevice::CHpsdrDevice(EBoardId boardId)
	:m_controllerType(boardId),m_CCoutSet(0),m_CCoutPending(0),m_micSample(0),m_lastCCout(MAX_CC_OUT),m_CC0in(0),
	 m_receiver1(this, 0),m_receiver2(this, 1),m_receiver3(this, 2),m_receiver4(this, 3),m_wideRecv(this),
	 m_microphone(this),m_speaker(this),m_transmit(this),m_recvSpeed(0),m_attrThreadEnabled(true),
	 m_attrThread(Thread<>::delegate_type(this, &CHpsdrDevice::thread_attr)), m_CCinDirty(0)
{
	memset(m_CCout, 0, sizeof(m_CCout));
	memset(m_CCin, 0, sizeof(m_CCin));

	buildAttrs();

	// launch the attribute thread
	m_attrThread.launch(THREAD_PRIORITY_ABOVE_NORMAL);
}
#pragma warning(pop)

CHpsdrDevice::~CHpsdrDevice()
{
	// shut down attribute thread
	if(m_attrThread.running())
	{
		{
			Locker lock(m_CCinLock);
			m_attrThreadEnabled = false;
			m_CCinUpdated.wakeAll();
		}
		m_attrThread.close();
	}
}

unsigned CHpsdrDevice::receive_frame(byte* frame)
{
	// check that sync pulses are present in the front of rbuf...JAM
	if(*frame++ != SYNC || *frame++ != SYNC || *frame++ != SYNC) return 0;

	// grab the received C&C code and dispatch the necessary notifiers
	byte CC0 = *frame++;
	m_CC0in = CC0;

	byte CCframe = ((CC0 & 0xF8) >> 1);
	byte* recvCC = m_CCin + CCframe;
	CCframe = CCframe >> 2;
	{
		Locker lock(m_CCinLock);
		*recvCC++ = *frame++;
		*recvCC++ = *frame++;
		*recvCC++ = *frame++;
		*recvCC++ = *frame++;
		m_CCinDirty |= (1UL<<CCframe);
		m_CCinUpdated.wakeAll();
	}

	int remain = 504; // 512 - 8 bytes

	Locker recvLock(m_recvListLock);
	const std::vector<Receiver>::size_type numReceiver = max(1, m_receivers.size());
	const bool hasReceivers = !m_receivers.empty();
	const int sample_size = 6 * numReceiver + 2;

	unsigned numSamples = 0;
	while(remain >= sample_size)
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
				ASSERT(m_receivers[recv]->isConnected());
				if(!m_receivers[recv]->Write(signals::etypComplex, &sample, 1, 0) && m_receivers[recv]->isConnected()) attrs.sync_fault->fire();
			}
		}

		// technique taken from KK: ensure that no matter what the sample rate is we still get the
		// proper mic rate (using a form of error diffusion?)
		m_micSample += MIC_RATE;
		if (m_micSample >= m_recvSpeed)
		{
			m_micSample = 0;

			// force the 16bit number to be signed and convert to float
			short MicAmpl = (short(frame[0]) << 8) | frame[1];
			float sample = MicAmpl / SCALE_16;
			if(!m_microphone.Write(signals::etypSingle, &sample, 1, 0) && m_microphone.isConnected()) attrs.sync_mic_fault->fire();
		}
		frame += 2;
		remain -= 2;
		numSamples++;
	}
	return numSamples;
}

void CHpsdrDevice::thread_attr()
{
	ThreadBase::SetThreadName("HPSDR Attribute Monitor Thread");

	byte curAddr = 0;
	byte CCin[4];
	for(;;)
	{
		{
			Locker lock(m_CCinLock);
			for(;;)
			{
				unsigned int nextAddr = curAddr;
				unsigned int nextMask = 1UL << nextAddr;
				if(!m_attrThreadEnabled) return;
				bool bFoundDirty = false;
				if(m_CCinDirty)
				{
					do
					{
						if(m_CCinDirty & nextMask)
						{
							bFoundDirty = true;
							break;
						}
						nextMask = nextMask << 1;
						nextAddr++;
						if(!nextMask)
						{
							nextMask = 1;
							nextAddr = 0;
						}
					}
					while(nextAddr != curAddr);
					ASSERT(bFoundDirty); // if we didn't find anything, how'd we get past the "if"?
				}
				if(bFoundDirty)
				{
					curAddr = nextAddr;
					*(long*)&CCin = *(long*)(m_CCin + curAddr*4);
					m_CCinDirty &= ~nextMask;
					break;
				}
				m_CCinUpdated.sleep(lock);
			}
		}

		for(TInAttrList::const_iterator trans = m_inAttrs.begin(); trans != m_inAttrs.end(); trans++)
		{
			CAttr_inMonitor* monitor = *trans;
			if(monitor->m_addr == curAddr)
			{
				monitor->evaluate(CCin);
			}
		}
	}
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

void CHpsdrDevice::send_frame(byte* frame, bool no_streams)
{
	*frame++ = SYNC;
	*frame++ = SYNC;
	*frame++ = SYNC;

	{
		bool MOX = false; // FUTURE TODO: must be false if no_streams!
		Locker outLock(m_CCoutLock);
		byte* CC = chooseCC();
		*frame++ = ((m_lastCCout << 1) | (MOX ? 1 : 0));
		*frame++ = *CC++;
		*frame++ = *CC++;
		*frame++ = *CC++;
		*frame++ = *CC++;
	}

	if(no_streams)
	{
		memset(frame, 0, 504);
	}
	else
	{
		for (int x = 8; x < 512; x += 8)        // fill out one 512-byte frame
		{
			std::complex<float> audSample;
			m_speaker.Read(signals::etypLRSingle, &audSample, 1, 0);

			// send left & right data to the speakers on the receiver
			int IntValue = int(SCALE_16 * audSample.real());
			*frame++ = (byte)(IntValue >> 8);    // left hi
			*frame++ = (byte)(IntValue & 0xff);  // left lo

			IntValue = int(SCALE_16 * audSample.imag());
			*frame++ = (byte)(IntValue >> 8);    // right hi
			*frame++ = (byte)(IntValue & 0xff);  // right lo

			std::complex<float> iqSample;
			m_transmit.Read(signals::etypComplex, &audSample, 1, 0);

			// send I & Q data to the exciter
			IntValue = int(SCALE_16 * iqSample.real());
			*frame++ = (byte)(IntValue >> 8);    // right hi
			*frame++ = (byte)(IntValue & 0xff);  // right lo

			IntValue = int(SCALE_16 * iqSample.imag());
			*frame++ = (byte)(IntValue >> 8);    // right hi
			*frame++ = (byte)(IntValue & 0xff);  // right lo
		} // end for
	}
}

// ------------------------------------------------------------------ class CHpsdrDevice::Receiver

const char* CHpsdrDevice::Receiver::EP_NAME[] = {"recv1", "recv2", "recv3", "recv4" };
const char* CHpsdrDevice::Receiver::EP_DESCR = "Received IQ Samples";

void CHpsdrDevice::Receiver::buildAttrs(const CHpsdrDevice& parent)
{
	attrs.sync_fault = addRemoteAttr("syncFault", parent.attrs.sync_fault);
	attrs.rate = addRemoteAttr("rate", parent.attrs.recv_speed);
	attrs.preamp = addLocalAttr(true, new CAttr_outProxy<signals::etypBoolean>("preamp", "Enable preamp?", *parent.attrs.recvX_preamp[0]));
	attrs.freq = addLocalAttr(true, new CAttr_outProxy<signals::etypLong>("freq", "Frequency", *parent.attrs.recvX_freq[0]));
	attrs.overflow = addLocalAttr(true, new CAttr_inProxy("overflow", "ADC overloaded?", *parent.attrs.recvX_overflow[0]));
	attrs.version = addLocalAttr(true, new CAttr_inProxy("version", "Software version", *parent.attrs.recvX_version[0]));
}

bool CHpsdrDevice::Receiver::Connect(signals::IEPSender* send)
{
	if(!COutEndpointBase::Connect(send)) return false;
	{
		Lock lock(m_parent->m_recvListLock);
		if(m_attachedRecv < 0)
		{
			m_parent->AttachReceiver(*this);
			ASSERT(m_attachedRecv < 0);
		}
	}
	return true;
}

bool CHpsdrDevice::Receiver::Disconnect()
{
	{
		Lock lock(m_parent->m_recvListLock);
		if(m_attachedRecv >= 0)
		{
			m_parent->DetachReceiver(*this, m_attachedRecv);
			m_attachedRecv = -1;
		}
	}
	return COutEndpointBase::Disconnect();
}

void CHpsdrDevice::Receiver::setProxy(CHpsdrDevice& parent, short attachedRecv)
{
	attrs.preamp->setProxy(*parent.attrs.recvX_preamp[attachedRecv]);
	attrs.freq->setProxy(*parent.attrs.recvX_freq[attachedRecv]);
	attrs.overflow->setProxy(*parent.attrs.recvX_overflow[attachedRecv]);
	attrs.version->setProxy(*parent.attrs.recvX_version[attachedRecv]);
}

// ------------------------------------------------------------------ class CHpsdrDevice::WideReceiver

const char* CHpsdrDevice::WideReceiver::EP_NAME = "wide";
const char* CHpsdrDevice::WideReceiver::EP_DESCR = "Received wideband raw ADC samples";

void CHpsdrDevice::WideReceiver::buildAttrs(const CHpsdrDevice& parent)
{
	attrs.sync_fault = addRemoteAttr("syncFault", parent.attrs.wide_sync_fault);
	attrs.rate = addLocalAttr(true, new CROAttribute<signals::etypSingle>("rate", "Data rate", 48000.0f));
}

// ------------------------------------------------------------------ class CHpsdrDevice::Transmitter

const char* CHpsdrDevice::Transmitter::EP_NAME = "send";
const char* CHpsdrDevice::Transmitter::EP_DESCR = "IQ transmitter data";

void CHpsdrDevice::Transmitter::buildAttrs(const CHpsdrDevice& parent)
{
	attrs.rate = addLocalAttr(true, new CROAttribute<signals::etypSingle>("rate", "Data rate", 48000.0f));
	attrs.version = addRemoteAttr("version", parent.m_controllerType == Hermes ? parent.attrs.merc_software : parent.attrs.penny_software);
}

// ------------------------------------------------------------------ class CHpsdrDevice::Microphone

const char* CHpsdrDevice::Microphone::EP_NAME = "mic";
const char* CHpsdrDevice::Microphone::EP_DESCR = "Microphone audio input";

void CHpsdrDevice::Microphone::buildAttrs(const CHpsdrDevice& parent)
{
	attrs.sync_fault = addRemoteAttr("syncFault", parent.attrs.sync_mic_fault);
	attrs.rate = addLocalAttr(true, new CROAttribute<signals::etypSingle>("rate", "Data rate", 48000.0f));
	attrs.source = addRemoteAttr("source", parent.attrs.src_mic);
}

// ------------------------------------------------------------------ class CHpsdrDevice::Speaker

const char* CHpsdrDevice::Speaker::EP_NAME = "speak";
const char* CHpsdrDevice::Speaker::EP_DESCR = "Speaker audio output";

void CHpsdrDevice::Speaker::buildAttrs(const CHpsdrDevice& parent)
{
	UNUSED_ALWAYS(parent);
	attrs.rate = addLocalAttr(true, new CROAttribute<signals::etypSingle>("rate", "Data rate", 48000.0f));
}

// ------------------------------------------------------------------ main attributes

void CHpsdrDevice::buildAttrs()
{
	// high-priority read-only
//	attr.PPT;
//	attr.DASH;
//	attr.DOT;

	// medium-priority read-only
	attrs.recv_overflow = addLocalInAttr(true, new CAttr_inBit("recvOverflow", "Receiver ADC overloaded?", false, 0, 0, 0x1));
	if(m_controllerType == Hermes)
	{
		attrs.hermes_I01 = addLocalInAttr(true, new CAttr_inBit("hermes_I01", "Hermes I01 active?", false, 0, 0, 0x2));
		attrs.hermes_I02 = addLocalInAttr(true, new CAttr_inBit("hermes_I02", "Hermes I02 active?", false, 0, 0, 0x4));
		attrs.hermes_I03 = addLocalInAttr(true, new CAttr_inBit("hermes_I03", "Hermes I03 active?", false, 0, 0, 0x8));
	}
	attrs.merc_software = addLocalInAttr(true, new CAttr_inBits("recvVersion", "Version of receiver software", 0, 0, 1, 0xFF, 0));
	if(m_controllerType != Hermes)
	{
		attrs.penny_software = addLocalInAttr(true, new CAttr_inBits("xmitVersion", "Version of transmitter software", 0, 0, 2, 0xFF, 0));
		attrs.ozy_software = addLocalInAttr(true, new CAttr_inBits("ifaceVersion", "Version of interface software", 0, 0, 3, 0xFF, 0));
	}
	attrs.AIN1 = addLocalInAttr(true, new CAttr_inShort("AIN1", "Forward power from Alex/Apollo", 0, 1, 2));
	attrs.AIN2 = addLocalInAttr(true, new CAttr_inShort("AIN2", "Reverse power from Alex/Apollo", 0, 2, 0));
	attrs.AIN3 = addLocalInAttr(true, new CAttr_inShort("AIN3", "AIN3 from Penny/Hermes", 0, 2, 2));
	attrs.AIN4 = addLocalInAttr(true, new CAttr_inShort("AIN4", "AIN4 from Penny/Hermes", 0, 3, 0));
	attrs.AIN5 = addLocalInAttr(true, new CAttr_inShort("AIN5", "Forward power from Penny/Hermes", 0, 1, 0));
	attrs.AIN6 = addLocalInAttr(true, new CAttr_inShort("AIN6", "3.8v supply on Penny/Hermes", 0, 3, 2));
	attrs.recvX_overflow[0] = addLocalInAttr(false, new CAttr_inBit("recv1Overflow", "Receiver 1 ADC overloaded?", false, 4, 0, 0x1));
	attrs.recvX_version[0] = addLocalInAttr(false, new CAttr_inBits("recv1Version", "Receiver 1 software version", 0, 4, 0, 0xFE, 1));
	attrs.recvX_overflow[1] = addLocalInAttr(false, new CAttr_inBit("recv2Overflow", "Receiver 2 ADC overloaded?", false, 4, 1, 0x1));
	attrs.recvX_version[1] = addLocalInAttr(false, new CAttr_inBits("recv2Version", "Receiver 2 software version", 0, 4, 1, 0xFE, 1));
	attrs.recvX_overflow[2] = addLocalInAttr(false, new CAttr_inBit("recv3Overflow", "Receiver 3 ADC overloaded?", false, 4, 2, 0x1));
	attrs.recvX_version[2] = addLocalInAttr(false, new CAttr_inBits("recv3Version", "Receiver 3 software version", 0, 4, 2, 0xFE, 1));
	attrs.recvX_overflow[3] = addLocalInAttr(false, new CAttr_inBit("recv4Overflow", "Receiver 4 ADC overloaded?", false, 4, 3, 0x1));
	attrs.recvX_version[3] = addLocalInAttr(false, new CAttr_inBits("recv4Version", "Receiver 4 software version", 0, 4, 3, 0xFE, 1));

	// events
	attrs.sync_fault = addLocalAttr(true, new CEventAttribute("syncFault", "Fires when a sync fault happens in a receive stream"));
	attrs.wide_sync_fault = addLocalAttr(false, new CEventAttribute("wideSyncFault", "Fires when a sync fault happens in the wideband receive stream"));
	attrs.sync_mic_fault = addLocalAttr(false, new CEventAttribute("micSyncFault", "Fires when an overrun occurs receiving microphone data"));

	// write-only
	attrs.recv_speed = addLocalAttr(true, new CAttr_out_recv_speed(*this, "recvRate", "Rate that receivers send data", 192000, 0, 0, 0x3, 0));
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
		attrs.src_mic = addLocalAttr(false, new CAttr_out_mic_src(*this, "MicSource", "Microphone Source", 1,
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
	attrs.recv_preamp = addLocalAttr(true, new CAttr_outBit(*this, "RecvPreamp", "Enable receiver preamp?", false, 0, 2, 0x04));
	attrs.recv_adc_dither = addLocalAttr(true, new CAttr_outBit(*this, "RecvAdcDither", "Enable receiver ADC dithering?", false, 0, 2, 0x08));
	attrs.recv_adc_random = addLocalAttr(true, new CAttr_outBit(*this, "RecvAdcRandom", "Enable receiver ADC random?", false, 0, 2, 0x10));
	static const char* alex_recv_ant[] = {"None", "Rx1", "Rx2", "XV"};
	attrs.alex_recv_ant = addLocalAttr(true, new CAttr_out_alex_recv_ant(*this, "AlexRecvAnt", "Alex Rx Antenna",
		0, _countof(alex_recv_ant), alex_recv_ant));
	static const char* alex_send_relay[] = {"Tx1", "Tx2", "Tx3"};
	attrs.alex_send_relay = addLocalAttr(true, new CAttr_outBits(*this, "AlexSendRelay", "Alex Tx relay",
		0, 0, 3, 0x3, 0, _countof(alex_send_relay), alex_send_relay));
//	attrs.timestamp = addLocalAttr(true, new CAttr_outBit(*this, "Timestamp", "Send 1pps through mic stream?", false, 0, 3, 0x40));

	attrs.recv_duplex = addLocalAttr(true, new CAttr_outBit(*this, "Duplex", "Duplex?", true, 0, 3, 0x04));
	attrs.num_recv = addLocalAttr(false, new CAttr_outBits(*this, "NumRecv", "Number of active receivers - 1",
		(byte)max(m_receivers.size(),1)-1, 0, 3, 0x7, 3, 0, NULL));
	attrs.recv_clone = addLocalAttr(true, new CAttr_outBit(*this, "RecvClone", "Common Mercury frequency?", false, 0, 3, 0x80));
	attrs.send_freq = addLocalAttr(true, new CAttr_outLong(*this, "SendFreq", "Transmit Frequency", 0, 1));
	attrs.recvX_freq[0] = addLocalAttr(false, new CAttr_outLong(*this, "Recv1Freq", "Receiver 1 Frequency", 0, 2));
	attrs.recvX_freq[1] = addLocalAttr(false, new CAttr_outLong(*this, "Recv2Freq", "Receiver 2 Frequency", 0, 3));
	attrs.recvX_freq[2] = addLocalAttr(false, new CAttr_outLong(*this, "Recv3Freq", "Receiver 3 Frequency", 0, 4));
	attrs.recvX_freq[3] = addLocalAttr(false, new CAttr_outLong(*this, "Recv4Freq", "Receiver 4 Frequency", 0, 5));
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

	attrs.recvX_preamp[0] = addLocalAttr(false, new CAttr_outBit(*this, "Recv1Preamp", "Enable receiver 1 preamp?", false, 10, 0, 0x01));
	attrs.recvX_preamp[1] = addLocalAttr(false, new CAttr_outBit(*this, "Recv1Preamp", "Enable receiver 2 preamp?", false, 10, 0, 0x02));
	attrs.recvX_preamp[2] = addLocalAttr(false, new CAttr_outBit(*this, "Recv1Preamp", "Enable receiver 3 preamp?", false, 10, 0, 0x04));
	attrs.recvX_preamp[3] = addLocalAttr(false, new CAttr_outBit(*this, "Recv1Preamp", "Enable receiver 4 preamp?", false, 10, 0, 0x08));

	m_receiver1.buildAttrs(*this);
	m_receiver2.buildAttrs(*this);
	m_receiver3.buildAttrs(*this);
	m_receiver4.buildAttrs(*this);
	m_wideRecv.buildAttrs(*this);
	m_microphone.buildAttrs(*this);
	m_speaker.buildAttrs(*this);
	m_transmit.buildAttrs(*this);
}

short CHpsdrDevice::AttachReceiver(CHpsdrDevice::Receiver& recv)
{
	// ASSUMES m_recvListLock IS HELD
	unsigned numRecv = m_receivers.size();
	if(numRecv >= 4)
	{
		ASSERT(FALSE);
		return -1;
	}
	m_receivers.resize(numRecv+1);
	m_receivers[numRecv] = &recv;
	recv.setProxy(*this, (short)numRecv);
	if(attrs.num_recv) attrs.num_recv->nativeSetValue((byte)numRecv);
	return (short)numRecv;
}

void CHpsdrDevice::DetachReceiver(const CHpsdrDevice::Receiver& recv, unsigned short attachedRecv)
{
	// ASSUMES m_recvListLock IS HELD
	unsigned numRecv = m_receivers.size();
	if(numRecv <= attachedRecv || m_receivers[attachedRecv] != &recv)
	{
		ASSERT(FALSE);
		return;
	}
	if(numRecv > (unsigned)attachedRecv+1)
	{
		Receiver* moved = m_receivers[numRecv-1];
		m_receivers[attachedRecv] = moved;
		moved->setProxy(*this, attachedRecv);
	}
	m_receivers.resize(numRecv-1);
	if(attrs.num_recv) attrs.num_recv->nativeSetValue((byte)max(numRecv-1,1)-1);
}

}