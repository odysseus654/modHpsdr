// waterfall.cpp : Defines the exported functions for the DLL application.
//
#include "stdafx.h"
#include "waterfall.h"

HMODULE gl_DllModule;

const char* CDirectxWaterfallDriver::NAME = "waterfall";
const char* CDirectxWaterfallDriver::DESCR = "DirectX waterfall display";
const unsigned char CDirectxWaterfallDriver::FINGERPRINT[] = { 1, signals::etypVecDouble, 0 };
char const * CDirectxWaterfall::NAME = "DirectX waterfall display";
const char* CDirectxWaterfall::CIncoming::EP_NAME = "in";
const char* CDirectxWaterfall::CIncoming::EP_DESCR = "Waterfall incoming endpoint";

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	case DLL_THREAD_ATTACH:
		gl_DllModule = hModule;
		break;
//	case DLL_THREAD_DETACH:
//	case DLL_PROCESS_DETACH:
//		break;
	}
	return TRUE;
}

extern "C" unsigned QueryDrivers(signals::IBlockDriver** drivers, unsigned availDrivers)
{
	static CDirectxWaterfallDriver DRIVER_waterfall;
	if(drivers && availDrivers)
	{
		drivers[0] = &DRIVER_waterfall;
	}
	return 1;
}

signals::IBlock* CDirectxWaterfallDriver::Create()
{
	signals::IBlock* blk = new CDirectxWaterfall(this);
	blk->AddRef();
	return blk;
}

#pragma warning(push)
#pragma warning(disable: 4355)
CDirectxWaterfall::CDirectxWaterfall(signals::IBlockDriver* driver):m_driver(driver),m_incoming(this),m_bDataThreadEnabled(true),
	 m_dataThread(Thread<CDirectxWaterfall*>::delegate_type(&CDirectxWaterfall::process_thread)),m_bufSize(0)
{
	buildAttrs();
	m_dataThread.launch(this, THREAD_PRIORITY_NORMAL);
}
#pragma warning(pop)

unsigned CDirectxWaterfall::Incoming(signals::IInEndpoint** ep, unsigned availEP)
{
	if(ep && availEP)
	{
		ep[0] = &m_incoming;
	}
	return 1;
}

void CDirectxWaterfall::process_thread(CDirectxWaterfall* owner)
{
	ThreadBase::SetThreadName("Waterfall Display Thread");

	std::vector<double> buffer;
	while(owner->m_bDataThreadEnabled)
	{
		{
			Locker lock(owner->m_buffLock);
			if(owner->m_bufSize > buffer.capacity()) buffer.resize(owner->m_bufSize);
		}
		unsigned recvCount = owner->m_incoming.Read(signals::etypCmplDbl, &buffer,
			buffer.capacity(), FALSE, IN_BUFFER_TIMEOUT);
		if(recvCount)
		{
			Locker lock(owner->m_buffLock);
			unsigned bufSize = owner->m_bufSize;
			if(!bufSize) bufSize = recvCount;

			double* buffPtr = buffer.data();
			while(recvCount)
			{
				owner->onReceivedFrame(buffer.data(), bufSize);
				buffPtr += bufSize;
				recvCount -= min(recvCount, bufSize);
			}
		}
	}
}

// void CDirectxWaterfall::buildAttrs()
// void CDirectxWaterfall::Start()
// void CDirectxWaterfall::CIncoming::OnConnection(signals::IEPRecvFrom *)
// void CDirectxWaterfall::onReceivedFrame(double *,unsigned)
