#pragma once
namespace signals
{
	__interface IAttribute;
	__interface IAttributes;
	__interface IAttributeObserver;
	__interface IBlock;
	__interface IBlockDriver;
	__interface IEPBuffer;
	__interface IEPReceiver;
	__interface IEPSender;
	__interface IInEndpoint;
	__interface IOutEndpoint;

	enum EType : unsigned
	{
		etypNone	= 0x00,
		etypEvent	= 0x01,
		etypBoolean	= 0x08,
		etypByte	= 0x0B,
		etypShort	= 0x0C,
		etypLong	= 0x0D,
		etypSingle	= 0x15,
		etypDouble	= 0x16,
		etypComplex	= 0x1D,
		etypString	= 0x23,
		etypLRSingle = 0x2D
	};

	__interface IBlockDriver
	{
		const char* Name();
		bool canCreate();
		bool canDiscover();
		unsigned Discover(IBlock** blocks, unsigned availBlocks);
		IBlock* Create();
	};

	__interface IBlock
	{
		unsigned AddRef();
		unsigned Release();
		const char* Name();
		IBlockDriver* Driver();
		IBlock* Parent();
		unsigned Children(IBlock** blocks, unsigned availBlocks);
		unsigned Incoming(IInEndpoint** ep, unsigned availEP);
		unsigned Outgoing(IOutEndpoint** ep, unsigned availEP);
		IAttributes* Attributes();
		bool Start();
		bool Stop();
	};

	__interface IEPSender
	{
		unsigned Write(EType type, const void* buffer, unsigned numElem, unsigned msTimeout);
		void onSourceConnected(IOutEndpoint* src);
		void onSourceDisconnected(IOutEndpoint* src);
	};

	__interface IEPReceiver
	{
		unsigned Read(EType type, void* buffer, unsigned numAvail, unsigned msTimeout);
		unsigned AddRef();
		unsigned Release();
	};

	__interface IEPBuffer : public IEPSender, public IEPReceiver
	{
		EType Type();
		unsigned Capacity();
		unsigned Used();
	};

	__interface IInEndpoint
	{
		unsigned AddRef();
		unsigned Release();
		IBlock* Block();
		const char* EPName();
		EType Type();
		IAttributes* Attributes();
		bool Connect(IEPReceiver* recv);
		bool isConnected();
		bool Disconnect();
		IEPBuffer* CreateBuffer();
	};

	__interface IOutEndpoint
	{
		IBlock* Block();
		const char* EPName();
		EType Type();
		IAttributes* Attributes();
		bool Connect(IEPSender* send);
		bool isConnected();
		bool Disconnect();
		IEPBuffer* CreateBuffer();
	};

	__interface IAttributes
	{
		unsigned Itemize(IAttribute** attrs, unsigned availElem);
		IAttribute* GetByName(const char* name);
		void Observe(IAttributeObserver* obs);
		void Unobserve(IAttributeObserver* obs);
		IBlock* Block();
	};

	__interface IAttribute
	{
		const char* Name();
		const char* Description();
		EType Type();
		void Observe(IAttributeObserver* obs);
		void Unobserve(IAttributeObserver* obs);
		bool isReadOnly();
		const void* getValue();
		bool setValue(const void* newVal);
		unsigned options(const char** opts, unsigned availElem);
	};

	__interface IAttributeObserver
	{
		void OnChanged(const char* name, EType type, const void* value);
	};
}