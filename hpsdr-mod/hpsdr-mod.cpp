// hpsdr-mod.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "HpsdrEther.h"
#include <iostream>

int _tmain(int argc, _TCHAR* argv[])
{
	_CrtSetDbgFlag ( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );

	signals::IBlock* devices[1];
	int numDevices = DRIVER_HpsdrEthernet.Discover(devices, _countof(devices));
	std::cout << numDevices << " devices found" << std::endl;

	signals::IBlock* hpsdr = devices[0];
	ASSERT(hpsdr != NULL);
//	VERIFY(hpsdr->Start());

	VERIFY(!hpsdr->Release());
	return 0;
}

