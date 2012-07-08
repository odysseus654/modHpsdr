using System;

namespace signals
{
    public enum EModType
    {
        Unknown,
        CSharp,
        CPlusPlus,
        OpenCL
    };

	public enum EType : int
	{
		None	= 0x00,
		Event	= 0x01,
		Boolean	= 0x08,
		Byte	= 0x0B,
		Short	= 0x0C,
		Long	= 0x0D,
		Single	= 0x15,
		Double	= 0x16,
		Complex	= 0x1D,
        CmplDbl = 0x1E,
        String  = 0x23,
		LRSingle = 0x2D
	};

    public interface IModule
    {
        string File { get; }
        EModType Type { get; }
        IBlockDriver[] Drivers { get; }
    };

    public interface IBlockDriver
	{
        IModule Module { get; }
		string Name { get; }
		bool canCreate { get; }
		bool canDiscover { get; }
		IBlock[] Discover();
		IBlock Create();
	};

    public interface IBlock : IDisposable
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

    public interface IEPSender : IDisposable
	{
		int Write(EType type, object[] buffer, int msTimeout);
	};

    public interface IEPReceiver : IDisposable
	{
		void Read(EType type, out object[] buffer, int msTimeout);
	};

    public interface IEPBuffer : IEPSender, IEPReceiver
	{
        EType Type { get; }
        int Capacity { get; }
        int Used { get; }
	};

    public interface IInEndpoint : IDisposable
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

    public interface IOutEndpoint : IDisposable
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

    public interface IAttributes
	{
		IAttribute[] Itemize();
		IAttribute GetByName(string name);
        IBlock Block { get; }
    };

	public interface IAttribute : IDisposable
	{
        string Name { get; }
        string Description { get; }
        EType Type { get; }
        bool isReadOnly { get; }
        object Value { get; set; }
		void options(out object[] values, out string[] opts);
        event OnChanged changed;
	};

    public delegate void OnChanged(string name, EType type, object value);
}
