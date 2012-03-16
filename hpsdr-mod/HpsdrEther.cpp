#include "stdafx.h"

#include "HPSDRDevice.h"
#include <process.h>
#include <WinSock2.h>

class CHpsdrEthernet : public CHpsdrDevice
{
public:
	virtual void Start();
protected:
	SOCKET buildSocket() const;
	void Metis_start_stop(bool runIQ, bool runWide);

	unsigned thread_recv();
	unsigned thread_send();

	HANDLE   m_recvThread, m_sendThread;
	SOCKET   m_sock;
	unsigned m_lastIQSeq, m_lastWideSeq, m_nextSendSeq;
	bool     m_iqStarting, m_wideStarting;
	byte     m_lastRunStatus;

private:
	static unsigned __stdcall threadbegin_recv(void *param);
	static unsigned __stdcall threadbegin_send(void *param);
};

SOCKET CHpsdrEthernet::buildSocket() const
{
	sockaddr_in iep_local;
	memset(&iep_local, 0, sizeof(iep_local));
	iep_local.sin_family = AF_INET;
	iep_local.sin_addr.S_un.S_addr = INADDR_ANY;

	// bind (open) the socket so we can use the Metis/Hermes that was found/selected
	SOCKET sock = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if(sock == INVALID_SOCKET) ThrowSocketError(WSAGetLastError());
	if(::bind(sock, (sockaddr*)&iep_local, sizeof(iep_local)) == SOCKET_ERROR) ThrowSocketError(WSAGetLastError());

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
	sockaddr_in iep_metis;
	memset(&iep_metis, 0, sizeof(iep_metis));
	iep_metis.sin_family = AF_INET;
	iep_metis.sin_addr.S_un.S_addr = Metis_IP_address;
	iep_metis.sin_port = MetisPort;
	if(::connect(sock, (sockaddr*)&iep_metis, sizeof(iep_metis)) == SOCKET_ERROR) ThrowSocketError(WSAGetLastError());

	return sock;
}

void CHpsdrEthernet::Start()
{
	/*
	// adapterSelected ranges from 1 thru number Of Adapters.  However, we need an 'index', which
	// is adapterSelected-1.
	int adapterIndex = MainForm.adapterSelected - 1;

	// get the name of this PC and, using it, the IP address of the first adapter
	IPAddress[] addr = Dns.GetHostEntry(strHostName).AddressList;

	MainForm.GetNetworkInterfaces();

	List<IPAddress> addrList = new List<IPAddress>();

	// make a list of all the adapters that we found in Dns.GetHostEntry(strHostName).AddressList
	foreach (IPAddress a in addr)
	{
		// make sure to get only IPV4 addresses!
		// test added because Erik Anderson noted an issue on Windows 7.  May have been in the socket
		// construction or binding below.
		if (a.AddressFamily == AddressFamily.InterNetwork)
		{
			addrList.Add(a);
		}
	}

	bool foundMetis = false;
	List<MetisHermesDevice> mhd = new List<MetisHermesDevice>();

	foreach (IPAddress ipa in addrList)
	{
		// configure a new socket object for each ethernet port we're scanning
		socket = new Socket(AddressFamily.InterNetwork, SocketType.Dgram, ProtocolType.Udp);

		// Listen to data on this PC's IP address. Allow the program to allocate a free port.
		iep = new IPEndPoint(ipa, LocalPort);  // was iep = new IPEndPoint(ipa, 0);

		// bind to socket and Port
		socket.Bind(iep);
		socket.ReceiveBufferSize = 0xFFFF;   // no lost frame counts at 192kHz with this setting
		socket.Blocking = true;

		IPEndPoint localEndPoint = (IPEndPoint)socket.LocalEndPoint;
		Console.WriteLine("Looking for Metis boards using host adapter IP {0}, port {1}", localEndPoint.Address, localEndPoint.Port);
                
		if (Metis_Discovery(ref mhd, iep))
		{
			foundMetis = true;
		}

		socket.Close();
		socket = null;
	}

	if (!foundMetis)
	{
		MessageBox.Show("No Metis/Hermes board found  - Check HPSDR is connected and powered");
		MainForm.OnOffButton_Click(this, EventArgs.Empty); // Toggle ON/OFF Button to OFF
		return;
	}
	int chosenDevice = 0;

	if (mhd.Count > 1)
	{
		// show selection dialog.
		DeviceChooserForm dcf = new DeviceChooserForm(mhd, MainForm.MetisMAC);
		DialogResult dr = dcf.ShowDialog();
		if (dr == DialogResult.Cancel)
		{
			MainForm.OnOffButton_Click(this, EventArgs.Empty); // Toggle ON/OFF Button to OFF
			return;
		}

		chosenDevice = dcf.GetChosenItem();
	}

	MainForm.Metis_IP_address = mhd[chosenDevice].IPAddress;
	MainForm.MetisMAC = mhd[chosenDevice].MACAddress;
*/
	m_sock = buildSocket();

	// spawn the recv + send threads
	unsigned threadId;
	m_recvThread = (HANDLE)_beginthreadex(NULL, 0, threadbegin_recv, this, CREATE_SUSPENDED, &threadId);
	if(m_recvThread == (HANDLE)-1L) ThrowErrnoError(errno);
	if(!SetThreadPriority(m_recvThread, THREAD_PRIORITY_HIGHEST)) ThrowLastError(GetLastError());

	m_sendThread = (HANDLE)_beginthreadex(NULL, 0, threadbegin_send, this, CREATE_SUSPENDED, &threadId);
	if(m_sendThread == (HANDLE)-1L) ThrowErrnoError(errno);
	if(!SetThreadPriority(m_sendThread, THREAD_PRIORITY_HIGHEST)) ThrowLastError(GetLastError());

	if(ResumeThread(m_recvThread) == -1L) ThrowLastError(GetLastError());
	if(ResumeThread(m_sendThread) == -1L) ThrowLastError(GetLastError());

	//// start thread to read Metis data from Ethernet.   DataLoop merely calls Process_Data, 
	//// which calls usb_bulk_read() and rcvr.Process(),
	//// and stuffs the demodulated audio into AudioRing buffer
	Data_thread.Name = "Ethernet Loop";
	Data_thread.IsBackground = true;  // extra insurance in case the program crashes
	Data_thread_running = true;

	Data_send(true); // send a frame to Ozy to prime the pump
	Thread.Sleep(20);
	Data_send(true); // send a frame to Ozy to prime the pump,with freq this time 

	Send_thread = new Thread(new ThreadStart(Data_send_Thread));
	Send_thread.Priority = ThreadPriority.Highest; // run USB thread at high priority
	Send_thread.Start();


	// start data from Metis
	Metis_start_stop(MainForm.Metis_IP_address, 0x03);   // bit 0 is start, bit 1 is wide spectrum

	MainForm.timer1.Enabled = true;  // start timer for bandscope update etc.

	MainForm.timer2.Enabled = true;  // used by Clip LED, fires every 100mS
	MainForm.timer2.Interval = 100;
	MainForm.timer3.Enabled = true;  // used by VOX, fires on user VOX delay setting 
	MainForm.timer3.Interval = 400;
	MainForm.timer4.Enabled = true;  // used by noise gate, fires on Gate delay setting
	MainForm.timer4.Interval = 100;
}

unsigned __stdcall CHpsdrEthernet::threadbegin_recv(void *param)
{
	return ((CHpsdrEthernet*)param)->thread_recv();
}

unsigned __stdcall CHpsdrEthernet::threadbegin_send(void *param)
{
	return ((CHpsdrEthernet*)param)->thread_send();
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
								push_wide_buffer();
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
							short* wideDest = m_wideBuff + (seq&7);
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
		lock
		{
			if(!m_lastRunStatus)
			{
				CloseHandle(m_recvThread);
				m_recvThread = INVALID_HANDLE_VALUE;
				return 0;
			}
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
	m_lastRunStatus = message[3];
	m_nextSendSeq = 0;

	if(::send(this->m_sock, (char*)message, sizeof(message), 0) == SOCKET_ERROR)
	{
		ThrowSocketError(WSAGetLastError());
	}
}

// Send two frames of 512 bytes to Metis/Hermes/Griffin
// The left & right audio to be sent comes from AudioRing buffer, which is filled by ProcessData()
unsigned CHpsdrEthernet::thread_recv()
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
