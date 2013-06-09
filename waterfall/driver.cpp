// waterfall.cpp : Defines the exported functions for the DLL application.
//
#include "stdafx.h"
#include "driver.h"

#include "waterfall.h"
#include "waveform.h"

HMODULE gl_DllModule;

const char* CDirectxDriver<CDirectxWaterfall>::NAME = "waterfall";
const char* CDirectxDriver<CDirectxWaterfall>::DESCR = "DirectX waterfall display";

const char* CDirectxDriver<CDirectxWaveform>::NAME = "waveform";
const char* CDirectxDriver<CDirectxWaveform>::DESCR = "DirectX waterfall display";

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
	static CDirectxDriver<CDirectxWaterfall> DRIVER_waterfall;
	static CDirectxDriver<CDirectxWaveform> DRIVER_waveform;
	if(drivers && availDrivers)
	{
		drivers[0] = &DRIVER_waterfall;
		if(availDrivers > 1) drivers[1] = &DRIVER_waveform;
	}
	return 2;
}
