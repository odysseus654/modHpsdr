#pragma once
typedef unsigned char byte;

class CHpsdrDevice
{
protected:
	bool receive_frame(byte* frame);
	void send_frame(byte* frame);

	const static byte   SYNC = 0x7F;
	const static float  SCALE_32;				// scale factor for converting 24-bit int from ADC to float
	const static float  SCALE_16;
	const static int    MIC_RATE = 48000;

	unsigned m_sampleRate;							// (config) incoming sample rate
	unsigned m_numReceiver;							// (config) incoming receiver count

	volatile byte m_recvCC0;
	unsigned m_micSample;							// private to process_data

protected: // TODO
	void status(char* name, bool status);
	void receive_sample(int recv, double real, double imag);
	void mic_sample(float ampl);
};
