/*
	Copyright 2012 Erik Anderson

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
#include "stdafx.h"
#include "HpsdrEther.h"

#include "block.h"
#include "error.h"
#include <set>

#pragma comment(lib, "ws2_32.lib")

extern "C" unsigned QueryDrivers(signals::IBlockDriver** drivers, unsigned availDrivers)
{
	static hpsdr::CHpsdrEthernetDriver DRIVER_HpsdrEthernet;
	if(drivers && availDrivers)
	{
		drivers[0] = &DRIVER_HpsdrEthernet;
	}
	return 1;
}

namespace hpsdr {

// ------------------------------------------------------------------ class CHpsdrEthernetDriver

const char* CHpsdrEthernetDriver::NAME = "radio";
const char* CHpsdrEthernetDriver::DESCR = "OpenHPSDR Ethernet Devices";

CHpsdrEthernetDriver::CHpsdrEthernetDriver()
{
	WSADATA wsaData;
	m_wsaStartup = WSAStartup(MAKEWORD(2, 2), &wsaData);
}

CHpsdrEthernetDriver::~CHpsdrEthernetDriver()
{
	if(m_wsaStartup == 0)
	{
		WSACleanup();
		m_wsaStartup = -1;
	}
}

unsigned CHpsdrEthernetDriver::Discover(signals::IBlock** blocks, unsigned availBlocks)
{
	if(!driverGood()) return 0;

	typedef std::list<CDiscoveredBoard> TBoardList;
	TBoardList discList;
	Metis_Discovery(discList);
	if(discList.empty()) return 0;

	if(blocks && availBlocks)
	{
		unsigned i;
		TBoardList::const_iterator trans;
		for(i=0, trans=discList.begin(); i < availBlocks && trans != discList.end(); i++, trans++)
		{
			const CDiscoveredBoard& disc = *trans;
			CHpsdrEthernet* block = new CHpsdrEthernet(this, disc.ipaddr, disc.mac, disc.ver, (CHpsdrEthernet::EBoardId)disc.boardId);
			block->AddRef();
			blocks[i] = block;
		}
	}
	return discList.size();
}

#pragma warning(push)
#pragma warning(disable: 4127)

void CHpsdrEthernetDriver::Metis_Discovery(std::list<CDiscoveredBoard>& discList)
{
	discList.clear();

	// set up HPSDR Metis discovery packet
	byte message[63];
	memset(message, 0, sizeof(message));
	message[0] = 0xEF;
	message[1] = 0xFE;
	message[2] = 0x02;

	sockaddr_in iep;
	memset(&iep, 0, sizeof(iep));
	iep.sin_family = AF_INET;
	iep.sin_addr.S_un.S_addr = INADDR_ANY;

	SOCKET sock = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if(sock == INVALID_SOCKET) ThrowSocketError(WSAGetLastError());
	try
	{
		if(::bind(sock, (sockaddr*)&iep, sizeof(iep)) == SOCKET_ERROR) ThrowSocketError(WSAGetLastError());

		static const int SendBufferSize = 1032;
		if(::setsockopt(sock, SOL_SOCKET, SO_SNDBUF, (char*)&SendBufferSize, sizeof(SendBufferSize)) == SOCKET_ERROR)
		{
			ThrowSocketError(WSAGetLastError());
		}

		// set socket option so that broadcast is allowed.
		static const BOOL bBroadcast = TRUE;
		if(::setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (char*)&bBroadcast, sizeof(bBroadcast)) == SOCKET_ERROR)
		{
			ThrowSocketError(WSAGetLastError());
		}

		// need this so we can Broadcast on the socket
		memset(&iep, 0, sizeof(iep));
		iep.sin_family = AF_INET;
		iep.sin_addr.S_un.S_addr = INADDR_BROADCAST;
		iep.sin_port = htons(CHpsdrEthernet::METIS_PORT);

		// send a broadcast to the port 1024
		if(::sendto(sock, (char*)message, sizeof(message), 0, (sockaddr*)&iep, sizeof(iep)) == SOCKET_ERROR)
		{
			ThrowSocketError(WSAGetLastError());
		}

		fd_set fds;
		FD_ZERO(&fds) ;
		FD_SET(sock, &fds);

		struct timeval tv;
		tv.tv_sec = 2;
		tv.tv_usec = 0;

		std::set<__int64> foundDevices;
		int iep_size = sizeof(iep);
		for(;;)
		{
			int ret = select (1, &fds, NULL, NULL, &tv);
			if(ret == SOCKET_ERROR) ThrowSocketError(WSAGetLastError());
			if(ret == 0) break;

			ret = ::recvfrom(sock, (char*)message, sizeof(message), 0, (sockaddr*)&iep, &iep_size);
			if(ret == SOCKET_ERROR) ThrowSocketError(WSAGetLastError());

			if(ret == 60 && message[0] == 0xEF && message[1] == 0xFE && (message[2] == 0x02 || message[2] == 0x03))
			{
				__int64 mac = (__int64(message[3])<<40)|(__int64(message[4])<<32)|(message[5]<<24)
					|(message[6]<<16)|(message[7]<<8)|message[8];
				if(mac && foundDevices.find(mac) == foundDevices.end())
				{
					CDiscoveredBoard thisBoard;
					thisBoard.ipaddr = iep.sin_addr.S_un.S_addr;
					thisBoard.status = message[2];
					thisBoard.mac = mac;
					thisBoard.ver = message[9];
					thisBoard.boardId = message[10];
					foundDevices.insert(mac);
					discList.push_back(thisBoard);
				}
			}
		}
	}
	catch(const std::exception&)
	{
		::shutdown(sock, SD_BOTH);
		::closesocket(sock);
		throw;
	}
	::shutdown(sock, SD_BOTH);
	::closesocket(sock);
}

#pragma warning(pop)

// ------------------------------------------------------------------ class CHpsdrEthernet

const char* CHpsdrEthernet::NAME = "Metis (OpenHPSDR Controller)";

#pragma warning(push)
#pragma warning(disable: 4355)

CHpsdrEthernet::CHpsdrEthernet(signals::IBlockDriver* driver, unsigned long ipaddr, __int64 mac, byte ver, EBoardId boardId)
	:CHpsdrDevice(boardId), m_ipAddress(ipaddr), m_macAddress(mac), m_controllerVersion(ver),
	 m_recvThread(Thread<>::delegate_type(this, &CHpsdrEthernet::thread_recv)),
	 m_sendThread(Thread<>::delegate_type(this, &CHpsdrEthernet::thread_send)),
	 m_sock(INVALID_SOCKET), m_lastRunStatus(0), m_iqStarting(false), m_wideStarting(false),
	 m_nextSendSeq(0), m_driver(driver), m_wideSyncFault(false)
{
}

#pragma warning(pop)

unsigned CHpsdrEthernet::NodeId(char* buff, unsigned availChar)
{
	if(availChar >= 17)
	{
		sprintf_s(buff, availChar+1, "%02X:%02X:%02X:%02X:%02X:%02X", (int)((m_macAddress>>40)&0xFF), (int)((m_macAddress>>32)&0xFF),
			(int)((m_macAddress>>24)&0xFF),(int)((m_macAddress>>16)&0xFF),(int)((m_macAddress>>8)&0xFF),(int)(m_macAddress&0xFF));
	}
	return 17;
}

unsigned CHpsdrEthernet::Incoming(signals::IInEndpoint** ep, unsigned availEP)
{
	if(ep && availEP > 0) ep[0] = &m_speaker;
	if(ep && availEP > 1) ep[1] = &m_transmit;
	return 2;
}

unsigned CHpsdrEthernet::Outgoing(signals::IOutEndpoint** ep, unsigned availEP)
{
	if(ep && availEP > 0) ep[0] = &m_microphone;
	if(ep && availEP > 1) ep[1] = &m_wideRecv;
	if(ep && availEP > 2) ep[2] = &m_receiver1;
	if(ep && availEP > 3) ep[3] = &m_receiver2;
	if(ep && availEP > 4) ep[4] = &m_receiver3;
	if(ep && availEP > 5) ep[5] = &m_receiver4;
	return 6;
}

SOCKET CHpsdrEthernet::buildSocket() const
{
	sockaddr_in iep;
	memset(&iep, 0, sizeof(iep));
	iep.sin_family = AF_INET;
	iep.sin_addr.S_un.S_addr = INADDR_ANY;

	// bind (open) the socket so we can use the Metis/Hermes that was found/selected
	SOCKET sock = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if(sock == INVALID_SOCKET) ThrowSocketError(WSAGetLastError());
	try
	{
		if(::bind(sock, (sockaddr*)&iep, sizeof(iep)) == SOCKET_ERROR) ThrowSocketError(WSAGetLastError());

		static const int ReceiveBufferSize = 0xFFFF;   // no lost frame counts at 192kHz with this setting
		if(::setsockopt(sock, SOL_SOCKET, SO_RCVBUF, (char*)&ReceiveBufferSize, sizeof(ReceiveBufferSize)) == SOCKET_ERROR)
		{
			ThrowSocketError(WSAGetLastError());
		}

		static const int SendBufferSize = 2064;  // 4 outgoing packets
		if(::setsockopt(sock, SOL_SOCKET, SO_SNDBUF, (char*)&SendBufferSize, sizeof(SendBufferSize)) == SOCKET_ERROR)
		{
			ThrowSocketError(WSAGetLastError());
		}
/*
		static const u_long Blocking = 1;
		if(::ioctlsocket(sock, FIONBIO, (u_long*)&Blocking) == SOCKET_ERROR) ThrowSocketError(WSAGetLastError());
*/
		// create an endpoint for sending to Metis
		memset(&iep, 0, sizeof(iep));
		iep.sin_family = AF_INET;
		iep.sin_addr.S_un.S_addr = m_ipAddress;
		iep.sin_port = htons(METIS_PORT);
		if(::connect(sock, (sockaddr*)&iep, sizeof(iep)) == SOCKET_ERROR) ThrowSocketError(WSAGetLastError());
	}
	catch(const std::exception&)
	{
		::shutdown(sock, SD_BOTH);
		throw;
	}

	return sock;
}

void CHpsdrEthernet::Start()
{
	m_sock = buildSocket();

	// spawn the recv + send threads
	m_sendThread.launch(THREAD_PRIORITY_TIME_CRITICAL, true);
	FlushPendingChanges();
	m_sendThreadLock.open(0, MAX_SEND_LAG);
	m_sendThread.resume();

	// start data from Metis
	Metis_start_stop(true, true);
}

void CHpsdrEthernet::Stop()
{
	// shut down sending thread
	m_sendThreadLock.close();

	// shut down receive thread
	Metis_start_stop(false, false);

	// finish shutting down the sending thread
	m_sendThread.close();

	// shut down the socket
	if(m_sock != INVALID_SOCKET)
	{
		shutdown(m_sock, SD_BOTH);
		closesocket(m_sock);
		m_sock = INVALID_SOCKET;
	}
}

#pragma warning(push)
#pragma warning(disable: 4127)

void CHpsdrEthernet::thread_recv()
{
	ThreadBase::SetThreadName("Metis Receive Thread");
	byte message[1032];

	fd_set fds;

	struct timeval tv;
	tv.tv_sec = 1;
	tv.tv_usec = 0;

	while(m_lastRunStatus)
	{
		FD_ZERO(&fds) ;
		FD_SET(this->m_sock, &fds);

		int ret = select(1, &fds, NULL, NULL, &tv);
		if(ret == SOCKET_ERROR) ThrowSocketError(WSAGetLastError());
		if(ret != 0)
		{
			ret = ::recv(this->m_sock, (char*)message, sizeof(message), 0);
			if(ret == SOCKET_ERROR) ThrowSocketError(WSAGetLastError());

			if(ret == 1032 && message[0] == 0xEF && message[1] == 0xFE && message[2] == 1)
			{
				unsigned int seq = (message[4]<<24)|(message[5]<<16)|(message[6]<<8)|message[7];
				switch(message[3])
				{
				case 4: // endpoint 4: wideband data
					if(!!(seq&7) && (m_wideStarting || m_lastWideSeq+1 != seq))
					{
						m_wideStarting = true;
					}
					else
					{
						m_wideStarting = false;
						m_lastWideSeq = seq;
						byte* wideSrc = message + 8;
						float wideBuff[512];
						float* wideDest = wideBuff;
						for(int i=0; i < 512; i++)
						{
							*wideDest++ = (short(wideSrc[0] << 8) | wideSrc[1]) / SCALE_16;
							wideSrc += 2;
						}
						if(m_wideRecv.Write(signals::etypSingle, &wideBuff, _countof(wideBuff), 0))
						{
							m_wideSyncFault = false;
						}
						else if(!m_wideSyncFault && m_wideRecv.isConnected())
						{
							m_wideSyncFault = true;
							attrs.wide_sync_fault->fire();
						}
					}
					break;
				case 6: // endpoint 4: IQ + mic data
					if(!m_iqStarting || !seq)
					{
						unsigned numSamples = receive_frame(message+8);
						if(numSamples)
						{
							numSamples += receive_frame(message+520);
							if(!m_iqStarting && m_lastIQSeq+1 != seq)
							{
								attrs.sync_fault->fire();
							} else {
								m_iqStarting = false;
							}
							m_lastIQSeq = seq;
							m_recvSamples += numSamples * MIC_RATE;
							if(m_recvSamples > m_recvSpeed*128)
							{
								// release send thread
								m_sendThreadLock.wake();
							}
						}
					}
					break;
				}
			}
		}
	}
}

#pragma warning(pop)

/* 
	* Send a UDP/IP packet to the IP address and port of the Metis board
	* Payload is 
	*      0xEFFE,0x04,run, 60 nulls
	*    where run = 0x00 to stop data and 0x01 to start
	* 
	*/

void CHpsdrEthernet::Metis_start_stop(bool runIQ, bool runWide)
{
	byte message[64];
	memset(message, 0, sizeof(message));
	message[0] = 0xEF;
	message[1] = 0xFE;
	message[2] = 0x04;
	message[3] = (runIQ ? 1 : 0) | (runWide ? 2 : 0);
	if(m_lastRunStatus == message[3]) return;

	if(!(m_lastRunStatus&1) && runIQ)
	{
		m_iqStarting = true;
		m_recvSamples = 0;
		m_lastIQSeq = 0;
	}
	if(!(m_lastRunStatus&2) && runWide)
	{
		m_wideStarting = true;
		m_lastWideSeq = 0;
	}
	m_nextSendSeq = 0;
	if(!m_lastRunStatus && message[3] && !m_recvThread.running())
	{
		m_recvThread.launch(THREAD_PRIORITY_TIME_CRITICAL, true);
		m_lastRunStatus = message[3];
		m_recvThread.resume();
	}
	else if(m_lastRunStatus && !message[3] && m_recvThread.running())
	{
		m_lastRunStatus = 0;
		m_recvThread.close();
	}

	if(::send(this->m_sock, (char*)message, sizeof(message), 0) == SOCKET_ERROR)
	{
		ThrowSocketError(WSAGetLastError());
	}
	m_lastRunStatus = message[3];
}

// Send two frames of 512 bytes to Metis/Hermes/Griffin
// The left & right audio to be sent comes from AudioRing buffer, which is filled by ProcessData()
void CHpsdrEthernet::thread_send()
{
	ThreadBase::SetThreadName("Metis Send Thread");
	byte message[1032];
	while(m_sendThreadLock.sleep())
	{
		memset(message, 0, sizeof(message));
		message[0] = 0xEF;
		message[1] = 0xFE;
		message[2] = 0x01;
		message[3] = 0x02;
		*(u_long*)&message[4] = ::htonl(m_nextSendSeq++);

		send_frame(message + 8);
		send_frame(message + 520);

		int ret = ::send(this->m_sock, (char*)message, sizeof(message), 0);
		if(ret == SOCKET_ERROR) ThrowSocketError(WSAGetLastError());
	}
}

void CHpsdrEthernet::FlushPendingChanges()
{
	ASSERT(!m_sendThreadLock.isOpen());
	if(m_sendThreadLock.isOpen()) return;

	byte message[1032];
	while(outPendingExists())
	{
		memset(message, 0, sizeof(message));
		message[0] = 0xEF;
		message[1] = 0xFE;
		message[2] = 0x01;
		message[3] = 0x02;
		*(u_long*)&message[4] = ::htonl(m_nextSendSeq++);

		send_frame(message + 8, true);
		send_frame(message + 520, true);

		int ret = ::send(this->m_sock, (char*)message, sizeof(message), 0);
		if(ret == SOCKET_ERROR) ThrowSocketError(WSAGetLastError());
	}
}

}