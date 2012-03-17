#include "stdafx.h"

#include "HPSDRDevice.h"
#include <list>
#include <process.h>
#include <set>
#include <WinSock2.h>

class CHpsdrEthernet : public CHpsdrDevice
{
public:
	virtual void Start();
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

void SetThreadName(char* threadName, DWORD dwThreadID = -1)
{
	#pragma pack(push,8)
	typedef struct tagTHREADNAME_INFO
	{
		DWORD dwType; // Must be 0x1000.
		LPCSTR szName; // Pointer to name (in user addr space).
		DWORD dwThreadID; // Thread ID (-1=caller thread).
		DWORD dwFlags; // Reserved for future use, must be zero.
	} THREADNAME_INFO;
	#pragma pack(pop)
	enum { MS_VC_EXCEPTION = 0x406D1388 };

	THREADNAME_INFO info;
	info.dwType = 0x1000;
	info.szName = threadName;
	info.dwThreadID = dwThreadID;
	info.dwFlags = 0;

	__try
	{
		RaiseException( MS_VC_EXCEPTION, 0, sizeof(info)/sizeof(ULONG_PTR), (ULONG_PTR*)&info );
	}
	__except(EXCEPTION_EXECUTE_HANDLER)
	{
	}
}

class socket_exception : public std::runtime_error
{
public:
	int code;
	inline socket_exception(int err, const char* str)
		:std::runtime_error(str),code(err)
	{}
	inline socket_exception(int err)
		:std::runtime_error(""),code(err)
	{}
};

__declspec(noreturn) void ThrowSocketError(int err)
{
	LPSTR lpMsgBuf = "Unknown error";
	if (FormatMessageA(
			FORMAT_MESSAGE_ALLOCATE_BUFFER |
			FORMAT_MESSAGE_FROM_SYSTEM |
			FORMAT_MESSAGE_IGNORE_INSERTS,
			NULL, err, 0,
			(LPSTR)&lpMsgBuf, 0, NULL))
	{
		std::string errStr = lpMsgBuf;
		LocalFree(lpMsgBuf);
		throw socket_exception(err, errStr.c_str());
	} else {
		throw socket_exception(err);
	}
}

class lasterr_exception : public std::runtime_error
{
public:
	DWORD code;
	inline lasterr_exception(int err, const char* str)
		:std::runtime_error(str),code(err)
	{}
	inline lasterr_exception(int err)
		:std::runtime_error(""),code(err)
	{}
};

__declspec(noreturn) void ThrowLastError(DWORD err)
{
	LPSTR lpMsgBuf = "Unknown error";
	if (FormatMessageA(
			FORMAT_MESSAGE_ALLOCATE_BUFFER |
			FORMAT_MESSAGE_FROM_SYSTEM |
			FORMAT_MESSAGE_IGNORE_INSERTS,
			NULL, err, 0,
			(LPSTR)&lpMsgBuf, 0, NULL))
	{
		std::string errStr = lpMsgBuf;
		LocalFree(lpMsgBuf);
		throw lasterr_exception(err, errStr.c_str());
	} else {
		throw lasterr_exception(err);
	}
}

class errno_exception : public std::runtime_error
{
public:
	int code;
	inline errno_exception(int err, const char* str)
		:std::runtime_error(str),code(err)
	{}
	inline errno_exception(int err)
		:std::runtime_error(""),code(err)
	{}
};

__declspec(noreturn) void ThrowErrnoError(int err)
{
	throw errno_exception(err, strerror(err));
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

		static const int SendBufferSize = 1032;
		if(::setsockopt(sock, SOL_SOCKET, SO_SNDBUF, (char*)&SendBufferSize, sizeof(SendBufferSize)) == SOCKET_ERROR)
		{
			ThrowSocketError(WSAGetLastError());
		}

		static const u_long Blocking = 1;
		if(::ioctlsocket(sock, FIONBIO, (u_long*)&Blocking) == SOCKET_ERROR) ThrowSocketError(WSAGetLastError());

		// create an endpoint for sending to Metis
		memset(&iep, 0, sizeof(iep));
		iep.sin_family = AF_INET;
		iep.sin_addr.S_un.S_addr = Metis_IP_address;
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

void CHpsdrEthernet::Metis_Discovery(std::list<CDiscoveredBoard>& discList)
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
		iep.sin_port = htons(METIS_PORT);

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
		throw;
	}
	::shutdown(sock, SD_BOTH);
}


void CHpsdrEthernet::Start()
{
	m_sock = buildSocket();

	// spawn the recv + send threads
	unsigned threadId;
	m_sendThread = (HANDLE)_beginthreadex(NULL, 0, threadbegin_send, this, CREATE_SUSPENDED, &threadId);
	if(m_sendThread == (HANDLE)-1L) ThrowErrnoError(errno);
	if(!SetThreadPriority(m_sendThread, THREAD_PRIORITY_HIGHEST)) ThrowLastError(GetLastError());

	if(ResumeThread(m_sendThread) == -1L) ThrowLastError(GetLastError());

	//// start thread to read Metis data from Ethernet.   DataLoop merely calls Process_Data, 
	//// which calls usb_bulk_read() and rcvr.Process(),
	//// and stuffs the demodulated audio into AudioRing buffer
	Data_send(true); // send a frame to Ozy to prime the pump
	Thread.Sleep(20);
	Data_send(true); // send a frame to Ozy to prime the pump,with freq this time 

	// start data from Metis
	Metis_start_stop(true, true);
}

unsigned __stdcall CHpsdrEthernet::threadbegin_recv(void *param)
{
	SetThreadName("Metis Receive Thread");
	try
	{
		return ((CHpsdrEthernet*)param)->thread_recv();
	}
	catch(...)
	{
#ifdef _DEBUG
		DebugBreak();
#endif
	}
}

unsigned __stdcall CHpsdrEthernet::threadbegin_send(void *param)
{
	SetThreadName("Metis Send Thread");
	try
	{
		return ((CHpsdrEthernet*)param)->thread_send();
	}
	catch(...)
	{
#ifdef _DEBUG
		DebugBreak();
#endif
	}
}

unsigned CHpsdrEthernet::thread_recv()
{
	byte message[1032];

	fd_set fds;
	FD_ZERO(&fds) ;
	FD_SET(this->m_sock, &fds);

	struct timeval tv;
	tv.tv_sec = 1;
	tv.tv_usec = 0;

	short wideBuff[4096];

	for(;;)
	{
		int ret = select (1, &fds, NULL, NULL, &tv);
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
					if(!m_wideStarting || !(seq&7))
					{
						if(!(seq&7))
						{
							if(m_wideStarting)
							{
								m_wideStarting = false;
							} else {
								push_wide_buffer(wideBuff, _countof(wideBuff));
							}
						}
						else if(m_lastWideSeq+1 != seq)
						{
							metis_sync(false);
							m_wideStarting = true;
						}
						else
						{
							metis_sync(true);
							m_lastWideSeq = seq;
							byte* wideSrc = message + 8;
							short* wideDest = wideBuff + (seq&7);
							for(int i=0; i < 512; i++)
							{
								*wideDest++ = (wideSrc[0] << 8) | wideSrc[1];
								wideSrc += 2;
							}
						}
					}
					break;
				case 6: // endpoint 4: IQ + mic data
					if(!m_iqStarting || !seq)
					{
						if(receive_frame(message+8) && receive_frame(message+520))
						{
							if(!m_iqStarting && m_lastWideSeq+1 != seq)
							{
								metis_sync(false);
							}
							else
							{
								metis_sync(true);
								m_iqStarting = false;
								m_lastIQSeq = seq;
							}
						}
					}
					break;
				}
			}
		}
		if(!m_lastRunStatus)
		{
			CloseHandle(m_recvThread);
			m_recvThread = INVALID_HANDLE_VALUE;
			return 0;
		}
	}
}

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
	if(!(m_lastRunStatus&1) && runIQ) m_iqStarting = true;
	if(!(m_lastRunStatus&2) && runWide) m_wideStarting = true;
	m_nextSendSeq = 0;
	if(!m_lastRunStatus && message[3] && m_recvThread == INVALID_HANDLE_VALUE)
	{
		unsigned threadId;
		m_recvThread = (HANDLE)_beginthreadex(NULL, 0, threadbegin_recv, this, CREATE_SUSPENDED, &threadId);
		if(m_recvThread == (HANDLE)-1L) ThrowErrnoError(errno);
		if(!SetThreadPriority(m_recvThread, THREAD_PRIORITY_HIGHEST)) ThrowLastError(GetLastError());
		m_lastRunStatus = message[3];
		if(ResumeThread(m_recvThread) == -1L) ThrowLastError(GetLastError());
	}
	else if(m_lastRunStatus && !message[3] && m_recvThread != INVALID_HANDLE_VALUE)
	{
		HANDLE thread = m_recvThread;
		m_lastRunStatus = message[3];
		WaitForSingleObject(thread, INFINITE);
	}

	if(::send(this->m_sock, (char*)message, sizeof(message), 0) == SOCKET_ERROR)
	{
		ThrowSocketError(WSAGetLastError());
	}
	m_lastRunStatus = message[3];
}

// Send two frames of 512 bytes to Metis/Hermes/Griffin
// The left & right audio to be sent comes from AudioRing buffer, which is filled by ProcessData()
unsigned CHpsdrEthernet::thread_send()
{
	byte message[1032];
	for(;;)
	{
		memset(message, 0, sizeof(message));
		message[0] = 0xEF;
		message[1] = 0xFE;
		message[2] = 0x01;
		message[3] = 0x02;
		*(u_long*)message[4] = ::htonl(m_nextSendSeq);

		send_frame(message + 8);
		send_frame(message + 520);

		return (::send(this->m_sock, (char*)message, sizeof(message), 0) == SOCKET_ERROR);
	}
}
