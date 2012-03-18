#pragma once

#include "BlockImpl.h"
#include "HPSDRDevice.h"
#include <list>
#include <WinSock2.h>

class CHpsdrEthernetDriver : public signals::IBlockDriver
{
public:
	CHpsdrEthernetDriver();
	virtual ~CHpsdrEthernetDriver();
	inline bool driverGood()		{ return m_wsaStartup == 0; }

public:
	virtual const char* Name()		{ return NAME; }
	virtual bool canCreate()		{ return false; }
	virtual bool canDiscover()		{ return true; }
	virtual bool Discover(signals::IBlock** blocks, unsigned* numBlocks);
	virtual signals::IBlock* Create()	{ return NULL; }

protected:
	static const char* NAME;
	int m_wsaStartup;

	struct CDiscoveredBoard
	{
		unsigned long ipaddr;
		byte status;
		__int64 mac;
		byte ver;
		byte boardId;
	};

	static void Metis_Discovery(std::list<CDiscoveredBoard>& discList);
};
extern CHpsdrEthernetDriver DRIVER_HpsdrEthernet;

class CHpsdrEthernet : public CHpsdrDevice, public signals::IBlock, public CRefcountObject
{
public:
	CHpsdrEthernet(unsigned long ipaddr, __int64 mac, byte ver, byte boardId);
	virtual ~CHpsdrEthernet();

	enum { METIS_PORT = 1024 };

public: // IBlock implementation
	virtual unsigned AddRef()				{ return CRefcountObject::AddRef(); }
	virtual unsigned Release()				{ return CRefcountObject::Release(); }
	virtual const char* Name()				{ return NAME; }
	virtual signals::IBlockDriver* Driver()	{ return &DRIVER_HpsdrEthernet; }
	virtual signals::IBlock* Parent()		{ return NULL; }
//	virtual unsigned numChildren();
//	virtual bool Children(signals::IBlock** blocks, unsigned* numBlocks);
//	virtual unsigned numIncoming();
//	virtual bool Incoming(signals::IInEndpoint** ep, unsigned* numEP);
//	virtual unsigned numOutgoing();
//	virtual bool Outgoing(signals::IOutEndpoint** ep, unsigned* numEP);
	virtual bool Start();
//	virtual bool Stop();

protected:
	static const char* NAME;

	SOCKET buildSocket() const;
	void Metis_start_stop(bool runIQ, bool runWide);

	unsigned thread_recv();
	unsigned thread_send();

	const unsigned long	m_ipAddress;
	const __int64		m_macAddress;
	const byte			m_controllerVersion;
	const byte			m_controllerType;

	HANDLE   m_recvThread, m_sendThread;
	SOCKET   m_sock;
	unsigned m_lastIQSeq, m_lastWideSeq, m_nextSendSeq;
	bool     m_iqStarting, m_wideStarting;
	volatile byte m_lastRunStatus;

private:
	static unsigned __stdcall threadbegin_recv(void *param);
	static unsigned __stdcall threadbegin_send(void *param);
};

