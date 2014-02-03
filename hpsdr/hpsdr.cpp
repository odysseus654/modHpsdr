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
// hpsdr-mod.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "HpsdrEther.h"
#include <iostream>

static Lock st_screenLock;
unsigned numPackets = 0;

class CAttrObserver : public signals::IAttributeObserver
{
public:
	inline CAttrObserver(const std::string& prefix, signals::IAttribute* attr)
		:m_prefix(prefix), m_attr(attr), m_name(attr->Name())
	{
		m_attr->Observe(this);
		reportValue(m_attr->Type(), m_attr->getValue());
	}

	~CAttrObserver()
	{
		if(m_attr)
		{
			m_attr->Unobserve(this);
			m_attr = NULL;
		}
	}

	virtual void OnChanged(const char* name, signals::EType type, const void* value)
	{
		ASSERT(strcmp(name, m_name.c_str()) == 0);
		reportValue(type, value);
	}

	std::string m_name;
	std::string m_prefix;
	signals::IAttribute* m_attr;

protected:
	inline void reportValue(signals::EType type, const void* value) const
	{
		Locker lock(st_screenLock);
		std::cout << m_prefix.c_str() << "/" << m_name.c_str() << ": " << valueToString(type, value).c_str() << std::endl;
	}

	static std::string valueToString(signals::EType type, const void* value)
	{
		char buffer[200];
		if(type == signals::etypEvent) return "(fired)";
		if(!value) return "(null)";
		switch(type)
		{
		case signals::etypWinHdl:
			sprintf_s(buffer, _countof(buffer), "(HWND)%08X", (int)*(HANDLE*)value);
			break;
		case signals::etypBoolean:
		case signals::etypVecBoolean:
			return *(unsigned char*)value ? "(boolean)true" : "(boolean)false";
		case signals::etypByte:
		case signals::etypVecByte:
			sprintf_s(buffer, _countof(buffer), "(byte)%d", (int)*(unsigned char*)value);
			break;
		case signals::etypShort:
		case signals::etypVecShort:
			sprintf_s(buffer, _countof(buffer), "(short)%d", (int)*(short*)value);
			break;
		case signals::etypLong:
		case signals::etypVecLong:
			sprintf_s(buffer, _countof(buffer), "(long)%d", (int)*(long*)value);
			break;
		case signals::etypInt64:
		case signals::etypVecInt64:
			sprintf_s(buffer, _countof(buffer), "(int64)%I64d", *(__int64*)value);
			break;
		case signals::etypSingle:
		case signals::etypVecSingle:
			sprintf_s(buffer, _countof(buffer), "(single)%f", (double)*(float*)value);
			break;
		case signals::etypDouble:
		case signals::etypVecDouble:
			sprintf_s(buffer, _countof(buffer), "(double)%f", *(double*)value);
			break;
		case signals::etypString:
			return std::string("(string)") + (char*)value;
		case signals::etypComplex:
		case signals::etypVecComplex:
			{
				std::complex<float>* pComplex = (std::complex<float>*)value;
				sprintf_s(buffer, _countof(buffer), "(complex) <real: %f, imag: %f>", (double)pComplex->real(), (double)pComplex->imag());
				break;
			}
		case signals::etypCmplDbl:
		case signals::etypVecCmplDbl:
			{
				std::complex<double>* pComplex = (std::complex<double>*)value;
				sprintf_s(buffer, _countof(buffer), "(complex) <real: %f, imag: %f>", pComplex->real(), pComplex->imag());
				break;
			}
		case signals::etypLRSingle:
		case signals::etypVecLRSingle:
			{
				std::complex<float>* pComplex = (std::complex<float>*)value;
				sprintf_s(buffer, _countof(buffer), "(left/right) <left: %f, right: %f>", (double)pComplex->real(), (double)pComplex->imag());
				break;
			}
		default:
			sprintf_s(buffer, _countof(buffer), "(unknown type %d)", (int)type);
			break;
		}
		return buffer;
	}
};

template<signals::EType ET>
class CStreamObserver
{
private:
	typedef typename StoreType<ET>::type store_type;
	Thread<> m_thread;
	bool m_bThreadOkay;
	signals::IEPReceiver* m_recv;
	enum { BUFFER_SIZE = 10000 };

public:
	CStreamObserver(signals::IEPReceiver* recv)
		:m_recv(recv),m_bThreadOkay(true),m_thread(Thread<>::delegate_type(this, &CStreamObserver::start))
	{
		m_thread.launch();
	}

	~CStreamObserver()
	{
		m_bThreadOkay = false;
		m_thread.close();
	}

private:
	void start()
	{
		while(m_bThreadOkay)
		{
			store_type buffer[BUFFER_SIZE];
			unsigned numRead = m_recv->Read(ET, buffer, BUFFER_SIZE, FALSE, 1000);
			if(numRead)
			{
				Locker lock(st_screenLock);
				std::cout << "got " << numRead << " entries" << std::endl;
				numPackets += numRead;
			}
		}
	}
};

// IBlock
// IOutEndpoint

static CHpsdrEthernetDriver DRIVER_HpsdrEthernet;

int _tmain(int argc, _TCHAR* argv[])
{
	_CrtSetDbgFlag ( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );

	signals::IBlock* devices[1];
	int numDevices = DRIVER_HpsdrEthernet.Discover(devices, _countof(devices));
	std::cout << numDevices << " devices found" << std::endl;

	signals::IBlock* hpsdr = devices[0];
	ASSERT(hpsdr != NULL);

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

	signals::IAttribute* recvSpeed = attrs->GetByName("recvRate");
	ASSERT(recvSpeed);
	long newSpeed = 48000;
	recvSpeed->setValue(&newSpeed);

	unsigned numOut = hpsdr->Outgoing(NULL, 0);
	signals::IOutEndpoint** outEPs = new signals::IOutEndpoint*[numOut];
	VERIFY(hpsdr->Outgoing(outEPs, numOut) == numOut);
	signals::IOutEndpoint* recv1 = outEPs[2];
	signals::IEPBuffer* buff = recv1->CreateBuffer();
	recv1->Connect(buff);

	{
		CStreamObserver<signals::etypComplex> strm(buff);

		hpsdr->Start();
		Sleep(30000);
		hpsdr->Stop();
	}

	for(TObservList::iterator trans = obsList.begin(); trans != obsList.end(); trans++)
	{
		delete *trans;
	}
	buff->Release();
	delete [] outEPs;
	VERIFY(!hpsdr->Release());

	std::cout << numPackets << " entries received" << std::endl;
	Sleep(30000);
	return 0;
}
