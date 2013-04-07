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
        String  = 0x02,
        
        Boolean = 0x10,
		Byte	= 0x11,
		Short	= 0x12,
		Long	= 0x13,
		Single	= 0x23,
		Double	= 0x24,
		Complex	= 0x34,
        CmplDbl = 0x35,
		LRSingle = 0x44,

        VecBoolean  = 0x18,
        VecByte     = 0x19,
        VecShort    = 0x1A,
        VecLong     = 0x1B,
        VecSingle   = 0x2B,
        VecDouble   = 0x2C,
        VecComplex  = 0x3C,
        VecCmplDbl  = 0x3D,
        VecLRSingle = 0x4C
    };

    public class Fingerprint
    {
        public EType[] inputs;
        public string[] inputNames;
        public EType[] outputs;
        public string[] outputNames;
    };

    public interface IConnectible
    {
        Fingerprint Fingerprint { get; }
    };

    public interface IModule
    {
        string File { get; }
        EModType Type { get; }
        IBlockDriver[] Drivers { get; }
        IFunctionSpec[] Functions { get; }
    };

    public interface IBlockDriver : IConnectible
	{
        IModule Module { get; }
		string Name { get; }
        string Description { get; }
        bool canCreate { get; }
		bool canDiscover { get; }
		IBlock[] Discover();
		IBlock Create();
	};

    public interface IBlock : IDisposable, IConnectible
	{
        string Name { get; }
        string NodeId { get; }
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
		void Read(EType type, out object[] buffer, bool bReadAll, int msTimeout);
	};

    public interface IEPBuffer : IEPSender, IEPReceiver
	{
        EType Type { get; }
        int Capacity { get; }
        int Used { get; }
	};

    public interface IInEndpoint : IDisposable
	{
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

    public interface IFunctionSpec : IConnectible
	{
        IModule Module { get; }
        string Name { get; }
		string Description { get; }
		IFunction Create();
    };

	public interface IFunction : IDisposable
	{
		IFunctionSpec Spec { get; }
		IInputFunction Input { get; }
		IOutputFunction Output { get; }
    };

	public interface IInputFunction : IInEndpoint, IEPReceiver
	{
	};

	public interface IOutputFunction : IOutEndpoint, IEPSender
	{
	};
}
