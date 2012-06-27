using System;

namespace signals
{
	enum EType : int
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
        etypCmplDbl = 0x1E,
        etypString  = 0x23,
		etypLRSingle = 0x2D
	};

	interface IBlockDriver
	{
		string Name { get; }
		bool canCreate { get; }
		bool canDiscover { get; }
		IBlock[] Discover();
		IBlock Create();
	};

	interface IBlock : IDisposable
	{
        string Name { get; }
        IBlockDriver Driver { get; }
        IBlock Parent { get; }
        IBlock[] Children { get; }
        IInEndpoint[] Incoming { get; }
        IOutEndpoint[] Outgoing { get; }
        IAttributes Attributes { get; }
		void Start();
		void Stop();
	};

	interface IEPSender : IDisposable
	{
		int Write(EType type, object[] buffer, int msTimeout);
	};

	interface IEPReceiver : IDisposable
	{
		void Read(EType type, out object[] buffer, int msTimeout);
	};

	interface IEPBuffer : IEPSender, IEPReceiver
	{
        EType Type { get; }
        int Capacity { get; }
        int Used { get; }
	};

	interface IInEndpoint : IDisposable
	{
        IBlock Block { get; }
        string EPName { get; }
        EType Type { get; }
        IAttributes Attributes { get; }
		bool Connect(IEPReceiver recv);
        bool isConnected { get; }
		bool Disconnect();
		IEPBuffer CreateBuffer();
	};

	interface IOutEndpoint : IDisposable
	{
        IBlock Block { get; }
        string EPName { get; }
        EType Type { get; }
        IAttributes Attributes { get; }
		bool Connect(IEPSender send);
        bool isConnected { get; }
		bool Disconnect();
		IEPBuffer CreateBuffer();
	};

	interface IAttributes
	{
		IAttribute[] Itemize();
		IAttribute GetByName(string name);
        IBlock Block { get; }
    };

	interface IAttribute : IDisposable
	{
        string Name { get; }
        string Description { get; }
        EType Type { get; }
        bool isReadOnly { get; }
        object Value { get; set; }
		void options(out object[] values, out string[] opts);
        event OnChanged changed;
	};

	delegate void OnChanged(string name, EType type, object value);
}
