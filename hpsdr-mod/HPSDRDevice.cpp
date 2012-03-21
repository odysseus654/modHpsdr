#include "stdafx.h"
#include "HPSDRDevice.h"

// ------------------------------------------------------------------ class CHpsdrDevice

const float CHpsdrDevice::SCALE_32 = float(1 << 31);
const float CHpsdrDevice::SCALE_16 = float(1 << 15);

bool CHpsdrDevice::receive_frame(byte* frame)
{
	const int sample_size = 6 * m_numReceiver + 2;

	// check that sync pulses are present in the front of rbuf...JAM
	if(*frame++ != SYNC || *frame++ != SYNC || *frame++ != SYNC) return false;

	byte CC0 = *frame++;
	m_recvCC0 = CC0;

	byte* recvCC = m_recvCC + ((CC0 & 0xF8) >> 1);
	lock (recvCC)
	{
		*recvCC++ = *frame++;
		*recvCC++ = *frame++;
		*recvCC++ = *frame++;
		*recvCC++ = *frame++;
	}
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

void CHpsdrDevice::send_frame(byte* frame)
{
	*frame++ = SYNC;
	*frame++ = SYNC;
	*frame++ = SYNC;

	byte* CC = chooseCC();
	*frame++ = *CC++;
	*frame++ = *CC++;
	*frame++ = *CC++;
	*frame++ = *CC++;
	*frame++ = *CC++;

	for (int x = 8; x < 512; x += 8)        // fill out one 512-byte frame
	{
		float audLeft, audRight;
		speaker_out(&audLeft, &audRight);

		// use the following rather than BitConverter.GetBytes since it uses less CPU 
		int IntValue = int(SCALE_16 * audLeft);
		*frame++ = (byte)(IntValue >> 8);        // left hi
		*frame++ = (byte)(IntValue & 0xff);  // left lo

		IntValue = int(SCALE_16 * audRight);
		*frame++ = (byte)(IntValue >> 8);    // right hi
		*frame++ = (byte)(IntValue & 0xff);  // right lo

		float xmitI, xmitQ;
		xmit_out(&xmitI, &xmitQ);

		// send I & Q data to Qzy 
		IntValue = int(SCALE_16 * xmitI);
		*frame++ = (byte)(IntValue >> 8);    // right hi
		*frame++ = (byte)(IntValue & 0xff);  // right lo

		IntValue = int(SCALE_16 * xmitQ);
		*frame++ = (byte)(IntValue >> 8);    // right hi
		*frame++ = (byte)(IntValue & 0xff);  // right lo
	} // end for
}

// ------------------------------------------------------------------ class CHpsdrDevice::Receiver

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
unsigned numAttributes();
unsigned Itemize(signals::IAttribute* attrs, unsigned availElem);
signals::IAttribute* GetByName(char* name);
void Attach(signals::IAttributeObserver* obs);
void Detach(signals::IAttributeObserver* obs);
*/