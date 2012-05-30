#pragma once

#include "HPSDRDevice.h"
#include <list>
#include <WinSock2.h>

class CHpsdrEthernetDriver : public signals::IBlockDriver
{
public:
	CHpsdrEthernetDriver();
	virtual ~CHpsdrEthernetDriver();
	inline bool driverGood()		{ return m_wsaStartup == 0; }

private:
	CHpsdrEthernetDriver(const CHpsdrEthernetDriver& other);
	CHpsdrEthernetDriver operator=(const CHpsdrEthernetDriver& other);

public:
	virtual const char* Name()		{ return NAME; }
	virtual bool canCreate()		{ return false; }
	virtual bool canDiscover()		{ return true; }
	virtual unsigned Discover(signals::IBlock** blocks, unsigned availBlocks);
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

class CHpsdrEthernet : public CHpsdrDevice, protected CRefcountObject
{
public:
	CHpsdrEthernet(unsigned long ipaddr, __int64 mac, byte ver, EBoardId boardId);
	virtual ~CHpsdrEthernet() { Stop(); }

	enum { METIS_PORT = 1024 };

public: // IBlock implementation
	virtual unsigned AddRef()				{ return CRefcountObject::AddRef(); }
	virtual unsigned Release()				{ return CRefcountObject::Release(); }
	virtual const char* Name()				{ return NAME; }
	virtual signals::IBlockDriver* Driver()	{ return &DRIVER_HpsdrEthernet; }
	virtual signals::IBlock* Parent()		{ return NULL; }
	virtual signals::IAttributes* Attributes() { return this; }
	virtual unsigned Children(signals::IBlock** blocks, unsigned availBlocks) { return 0; }
	virtual unsigned Incoming(signals::IInEndpoint** ep, unsigned availEP);
	virtual unsigned Outgoing(signals::IOutEndpoint** ep, unsigned availEP);
	virtual bool Start();
	virtual bool Stop();

private:
	static const char* NAME;

	enum
	{
		MAX_SEND_LAG = 10		// how many frames behind are we permitting the send thread to catch up with?
	};

	SOCKET buildSocket() const;
	void Metis_start_stop(bool runIQ, bool runWide);

	unsigned thread_recv();
	unsigned thread_send();
	void FlushPendingChanges();

	const unsigned long	m_ipAddress;
	const __int64		m_macAddress;
	const byte			m_controllerVersion;

	HANDLE   m_recvThread, m_sendThread;
	SOCKET   m_sock;
	unsigned m_lastIQSeq, m_lastWideSeq, m_nextSendSeq;
	bool     m_iqStarting, m_wideStarting;
	volatile byte m_lastRunStatus;
	Semaphore m_sendThreadLock;
	unsigned m_recvSamples;		// private to thread_recv

private:
	static unsigned __stdcall threadbegin_recv(void *param);
	static unsigned __stdcall threadbegin_send(void *param);
};

