﻿using System;

namespace signals
{
    enum EModType
    {
        Unknown,
        CSharp,
        CPlusPlus,
        OpenCL
    };

	enum EType : int
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

    interface IModule
    {
        string File { get; }
        EModType Type { get; }
        IBlockDriver[] Drivers { get; }
    };

	interface IBlockDriver
	{
        IModule Module { get; }
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