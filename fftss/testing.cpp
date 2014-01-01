/*
	Copyright 2012-2013 Erik Anderson

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

// tesing.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "fftssDriver.h"

static fftss::CFFTransformDriver<signals::etypCmplDbl,signals::etypVecCmplDbl> fft_dd;
static fftss::CFFTransformDriver<signals::etypCmplDbl,signals::etypVecComplex> fft_ds;
static fftss::CFFTransformDriver<signals::etypComplex,signals::etypVecCmplDbl> fft_sd;
static fftss::CFFTransformDriver<signals::etypComplex,signals::etypVecComplex> fft_ss;

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
