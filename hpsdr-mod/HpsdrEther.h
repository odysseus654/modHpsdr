#pragma once

#include "HPSDRDevice.h"
#include <list>
#include <WinSock2.h>

class CHpsdrEthernet : public CHpsdrDevice, public signals::IBlock
{
public:
//		unsigned AddRef();
//		unsigned Release();
//		const char* Name();
//		IBlockDriver* Driver();
//		IBlock* Parent();
//		unsigned numChildren();
//		bool Children(IBlock** blocks, unsigned* numBlocks);
//		unsigned numIncoming();
//		bool Incoming(IInEndpoint** ep, unsigned* numEP);
//		unsigned numOutgoing();
//		bool Outgoing(IOutEndpoint** ep, unsigned* numEP);
//		bool Stop();
	virtual bool Start();
protected:
	struct CDiscoveredBoard
	{
		unsigned long ipaddr;
		byte status;
		__int64 mac;
		byte ver;
		byte boardId;
	};

	SOCKET buildSocket() const;
	void Metis_start_stop(bool runIQ, bool runWide);
	static void Metis_Discovery(std::list<CDiscoveredBoard>& discList);

	enum {
		METIS_PORT = 1024
	};

	unsigned thread_recv();
	unsigned thread_send();

	HANDLE   m_recvThread, m_sendThread;
	SOCKET   m_sock;
	unsigned m_lastIQSeq, m_lastWideSeq, m_nextSendSeq;
	bool     m_iqStarting, m_wideStarting;
	volatile byte m_lastRunStatus;

private:
	static unsigned __stdcall threadbegin_recv(void *param);
	static unsigned __stdcall threadbegin_send(void *param);
};

