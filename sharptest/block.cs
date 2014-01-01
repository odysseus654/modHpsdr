/*
	Copyright 2012-2013 Erik Anderson

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
        WinHdl  = 0x03,
        
        Boolean = 0x10,
		Byte	= 0x11,
		Short	= 0x12,
		Long	= 0x13,
        Int64	= 0x14,
		Single	= 0x23,
		Double	= 0x24,
		Complex	= 0x34,
        CmplDbl = 0x35,
		LRSingle = 0x44,

        VecBoolean  = 0x18,
        VecByte     = 0x19,
        VecShort    = 0x1A,
        VecLong     = 0x1B,
        VecInt64    = 0x1C,
        VecSingle   = 0x2B,
        VecDouble   = 0x2C,
        VecComplex  = 0x3C,
        VecCmplDbl  = 0x3D,
        VecLRSingle = 0x4C
    };

    [Flags]
    public enum EAttrEnumFlags : int
    {
        flgLocalOnly = 1,
        flgIncludeHidden = 2
    };

    public class Fingerprint
    {
        public EType[] inputs;
        public string[] inputNames;
        public EType[] outputs;
        public string[] outputNames;
    };

    public interface ICircuitConnectible
    {   // Indicates this element can be part of a circuit diagram
        Fingerprint Fingerprint { get; }
    };

    public interface ICircuitElement : IDisposable
    {   // Indicates this element can be part of a running circuit
    };

    public interface IModule
    {
        string File { get; }
        EModType Type { get; }
        IBlockDriver[] Drivers { get; }
        IFunctionSpec[] Functions { get; }
    };

    public interface IBlockDriver : ICircuitConnectible
	{
        IModule Module { get; }
		string Name { get; }
        string Description { get; }
        bool canCreate { get; }
		bool canDiscover { get; }
		IBlock[] Discover();
		IBlock Create();
	};

    public interface IBlock : IDisposable, ICircuitConnectible, ICircuitElement
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

    public interface IEPSendTo : IDisposable
	{
		int Write(EType type, object[] buffer, int msTimeout);
        IAttributes InputAttributes { get; }
    };

    public interface IEPRecvFrom : IDisposable
	{
		void Read(EType type, out object[] buffer, bool bReadAll, int msTimeout);
        IAttributes OutputAttributes { get; }
    };

    public interface IEPBuffer : IEPSendTo, IEPRecvFrom
	{
        EType Type { get; }
        int Capacity { get; }
        int Used { get; }
	};

    public interface IInEndpoint : IDisposable
	{
        string EPName { get; }
        string EPDescr { get; }
        EType Type { get; }
        IAttributes Attributes { get; }
		bool Connect(IEPRecvFrom recv);
        bool isConnected { get; }
		bool Disconnect();
		IEPBuffer CreateBuffer();
	};

    public interface IOutEndpoint : IDisposable
	{
        string EPName { get; }
        string EPDescr { get; }
        EType Type { get; }
        IAttributes Attributes { get; }
		bool Connect(IEPSendTo send);
        bool isConnected { get; }
		bool Disconnect();
		IEPBuffer CreateBuffer();
	};

    public interface IAttributes : System.Collections.Generic.IEnumerable<IAttribute>
	{
        System.Collections.Generic.IEnumerator<IAttribute> Itemize(EAttrEnumFlags flags);
        IAttribute this[string name] { get; }
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

    public delegate void OnChanged(IAttribute attr, object value);

    public interface IFunctionSpec : ICircuitConnectible
	{
        IModule Module { get; }
        string Name { get; }
		string Description { get; }
		IFunction Create();
    };

    public interface IFunction : IDisposable, ICircuitElement
	{
		IFunctionSpec Spec { get; }
		IInputFunction Input { get; }
		IOutputFunction Output { get; }
    };

	public interface IInputFunction : IInEndpoint, IEPRecvFrom
	{
	};

	public interface IOutputFunction : IOutEndpoint, IEPSendTo
	{
	};
}
