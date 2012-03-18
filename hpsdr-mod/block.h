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
		etypNone    = 0x00,
		etypBoolean = 0x08,
		etypByte    = 0x0B,
		etypShort	= 0x0C,
		etypLong	= 0x0D,
		etypSingle	= 0x15,
		etypDouble  = 0x16,
		etypComplex	= 0x1D,
		etypString  = 0x23,
	};

	__interface IBlockDriver
	{
		const char* Name();
		bool canCreate();
		bool canDiscover();
		bool Discover(IBlock** blocks, unsigned* numBlocks);
		IBlock* Create();
	};

	__interface IBlock
	{
		unsigned AddRef();
		unsigned Release();
		const char* Name();
		IBlockDriver* Driver();
		IBlock* Parent();
		unsigned numChildren();
		bool Children(IBlock** blocks, unsigned* numBlocks);
		unsigned numIncoming();
		bool Incoming(IInEndpoint** ep, unsigned* numEP);
		unsigned numOutgoing();
		bool Outgoing(IOutEndpoint** ep, unsigned* numEP);
		bool Start();
		bool Stop();
	};

	__interface IEPSender
	{
		unsigned Write(void* buffer, EType type, unsigned numElem, unsigned msTimeout);
		void onSourceConnected(IOutEndpoint* src);
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
		EType Type();
		IAttributes* Attributes();
		bool Connect(IEPSender* send);
		bool isConnected();
		bool Disconnect();
		IEPBuffer* CreateBuffer();
	};

	__interface IAttributes
	{
		unsigned numAttributes();
		void Itemize(IAttribute* attrs, unsigned* numElem);
		IAttribute* GetByName(char* name);
		void Attach(IAttributeObserver* obs);
		void Detach(IAttributeObserver* obs);
		IBlock* Block();
	};

	__interface IAttribute
	{
		const char* Name();
		const char* Description();
		IAttributes* Attributes();
		EType Type();
		void Attach(IAttributeObserver* obs);
		void Detach(IAttributeObserver* obs);
		void* Value();
	};

	__interface IAttributeObserver
	{
		void OnChanged(char* name, EType type, void* value);
	};
}