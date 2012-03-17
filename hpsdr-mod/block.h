#pragma once
__interface IAttribute;
__interface IAttributes;
__interface IAttributeObserver;
__interface IBlock;
__interface IBlockDriver;
__interface IInEndpoint;
__interface IOutEndpoint;

enum EType : unsigned
{
	etypUnknown = 0x00,
	etypBoolean = 0x09,
	etypShort	= 0x0C,
	etypLong	= 0x0D,
	etypSingle	= 0x15,
	etypComplex	= 0x1D,
	etypString  = 0x23,
};

__interface IBlockDriver
{
	bool Name(char* buffer, unsigned* bufcount);
	bool canCreate();
	bool canDiscover();
	bool Discover(IBlock** blocks, unsigned* numBlocks);
	IBlock* Create();
};

__interface IBlock
{
	bool Name(char* buffer, unsigned* bufcount);
	IBlockDriver* Driver();
	IBlock* Parent();
	bool Destroy();
	unsigned numChildren();
	bool Children(IBlock** blocks, unsigned* numBlocks);
	unsigned numIncoming();
	bool Incoming(IInEndpoint** ep, unsigned* numEP);
	unsigned numOutgoing();
	bool Outgoing(IOutEndpoint** ep, unsigned* numEP);
	bool Start();
	bool Stop();
};

__interface IEPReceiver
{
	void Write(void* buffer, EType type, unsigned numElem);
};

__interface IEPSender
{
	void Read(EType type, void* buffer, unsigned* numElem);
};

__interface IInEndpoint
{
	EType Type();
	IAttributes* Attributes();
	bool Connect(IEPReceiver* recv);
	bool isConnected();
	bool Disconnect();
};

__interface IOutEndpoint
{
	EType Type();
	IAttributes* Attributes();
	bool Connect(IEPSender* send);
	bool isConnected();
	bool Disconnect();
};

__interface IAttributes
{
	unsigned numAttributes();
	void Itemize(IAttribute* attrs, unsigned* numElem);
	IAttribute* GetByName(char* name);
	void Attach(IAttributeObserver* obs);
	void Detach(IAttributeObserver* obs);
};

__interface IAttribute
{
	bool Name(char* buffer, unsigned* bufcount);
	EType Type();
	void Attach(IAttributeObserver* obs);
	void Detach(IAttributeObserver* obs);
	void* Value();
};

__interface IAttributeObsever
{
	void OnChanged(char* name, EType type, void* value);
};