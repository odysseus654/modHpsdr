// tesing.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "fftssDriver.h"

static fftss::CFFTransformDriver<signals::etypCmplDbl,signals::etypCmplDbl> fft_dd;
static fftss::CFFTransformDriver<signals::etypCmplDbl,signals::etypComplex> fft_ds;
static fftss::CFFTransformDriver<signals::etypComplex,signals::etypCmplDbl> fft_sd;
static fftss::CFFTransformDriver<signals::etypComplex,signals::etypComplex> fft_ss;

int _tmain(int argc, _TCHAR* argv[])
{
	_CrtSetDbgFlag ( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );

	signals::IBlock* fft = fft_dd.Create();
	ASSERT(fft != NULL);
/*
	typedef std::list<CAttrObserver*> TObservList;
	TObservList obsList;
	signals::IAttributes* attrs = hpsdr->Attributes();
	{
		unsigned count = attrs->Itemize(NULL, 0);
		signals::IAttribute** attrList = new signals::IAttribute*[count];
		VERIFY(attrs->Itemize(attrList, count) == count);
		for(unsigned i = 0; i < count; i++)
		{
			obsList.push_back(new CAttrObserver("", attrList[i]));
		}
		delete [] attrList;
	}

	unsigned numOut = fft->Outgoing(NULL, 0);
	signals::IOutEndpoint** outEPs = new signals::IOutEndpoint*[numOut];
	VERIFY(hpsdr->Outgoing(outEPs, numOut) == numOut);
	signals::IOutEndpoint* recv1 = outEPs[2];
	signals::IEPBuffer* buff = recv1->CreateBuffer();
	recv1->Connect(buff);

	{
		CStreamObserver<signals::etypComplex> strm(buff);
*/
//		fft->Start();
		Sleep(30000);
/*		hpsdr->Stop();
	}

	for(TObservList::iterator trans = obsList.begin(); trans != obsList.end(); trans++)
	{
		delete *trans;
	}
	buff->Release();
	delete [] outEPs;*/
	VERIFY(!fft->Release());

//	std::cout << numPackets << " entries received" << std::endl;
//	Sleep(30000);
	return 0;
}
