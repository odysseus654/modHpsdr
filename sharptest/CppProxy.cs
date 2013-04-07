using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.IO;

namespace cppProxy
{
    static class Utilities
    {
        private static unsafe int getStringLength(IntPtr strz)
        {
            if(strz == IntPtr.Zero) throw new ArgumentNullException("strz");
            int strlen = 0;
            for (byte* native = (byte*)strz.ToPointer(); *native != 0; native++, strlen++) ;
            return strlen;
        }

        public static string getString(IntPtr strz)
        {
            if (strz == IntPtr.Zero) return null;
            return getString(strz, getStringLength(strz));
        }

        public static string getString(IntPtr strz, int strlen)
        {
            if (strz == IntPtr.Zero) return null;
            byte[] strArray = new byte[strlen];
            Marshal.Copy(strz, strArray, 0, strlen);
            return System.Text.Encoding.UTF8.GetString(strArray);
        }
    }

    class Registration
    {
        private static sharptest.WeakValuedDictionary<IntPtr, object> m_universe = new sharptest.WeakValuedDictionary<IntPtr, object>();

        public static object retrieveObject(IntPtr key)
        {
            if (key == IntPtr.Zero) throw new ArgumentNullException("key");
            object value;
            if (m_universe.TryGetValue(key, out value))
            {
                return value;
            }
            else
            {
                return null;
            }
        }

        public static void storeObject(IntPtr key, object value)
        {
            if (key == IntPtr.Zero) throw new ArgumentNullException("key");
            if (value == null) throw new ArgumentNullException("value");
            m_universe.Add(key, value);
        }

        public static void removeObject(IntPtr key)
        {
            if (key == IntPtr.Zero) throw new ArgumentNullException("key");
            m_universe.Remove(key);
        }
    }

    static class ProxyTypes
    {
        public interface ITypeMarshaller
        {
            int size(object val);
            object fromNative(IntPtr val);
            void toNative(object src, IntPtr dest);
        };

        private class Boolean : ITypeMarshaller
        {
            public int size(object val) { return sizeof(byte); }
            public object fromNative(IntPtr val) { return Marshal.ReadByte(val) != 0; }
            public void toNative(object src, IntPtr dest) { Marshal.WriteByte(dest, (byte)((bool)src ? 1 : 0)); }
        };

        private class Byte : ITypeMarshaller
        {
            public int size(object val) { return sizeof(byte); }
            public object fromNative(IntPtr val) { return Marshal.ReadByte(val); }
            public void toNative(object src, IntPtr dest) { Marshal.WriteByte(dest, (byte)src); }
        };

        private class Short : ITypeMarshaller
        {
            public int size(object val) { return sizeof(short); }
            public object fromNative(IntPtr val) { return Marshal.ReadInt16(val); }
            public void toNative(object src, IntPtr dest) { Marshal.WriteInt16(dest, (short)src); }
        };

        private class Long : ITypeMarshaller
        {
            public int size(object val) { return sizeof(int); }
            public object fromNative(IntPtr val) { return Marshal.ReadInt32(val); }
            public void toNative(object src, IntPtr dest) { Marshal.WriteInt32(dest, (int)src); }
        };

        private class Single : ITypeMarshaller
        {
            public int size(object val) { return sizeof(float); }
            public object fromNative(IntPtr val)
            {
                float[] buff = new float[1];
                Marshal.Copy(val, buff, 0, 1);
                return buff[0];
            }
            public void toNative(object src, IntPtr dest)
            {
                Marshal.Copy(new float[] { (float)src }, 0, dest, 1);
            }
        };

        private class Double : ITypeMarshaller
        {
            public int size(object val) { return sizeof(double); }
            public object fromNative(IntPtr val)
            {
                double[] buff = new double[1];
                Marshal.Copy(val, buff, 0, 1);
                return buff[0];
            }
            public void toNative(object src, IntPtr dest)
            {
                Marshal.Copy(new double[] { (double)src }, 0, dest, 1);
            }
        };

        private class Complex : ITypeMarshaller
        {
            public int size(object val) { return 2*sizeof(float); }
            public object fromNative(IntPtr val)
            {
                float[] buff = new float[2];
                Marshal.Copy(val, buff, 0, 2);
                return buff;
            }
            public void toNative(object src, IntPtr dest)
            {
                Marshal.Copy((float[])src, 0, dest, 2);
            }
        };

        private class ComplexDouble : ITypeMarshaller
        {
            public int size(object val) { return 2*sizeof(double); }
            public object fromNative(IntPtr val)
            {
                double[] buff = new double[2];
                Marshal.Copy(val, buff, 0, 2);
                return buff;
            }
            public void toNative(object src, IntPtr dest)
            {
                Marshal.Copy((double[])src, 0, dest, 2);
            }
        };

        private class String : ITypeMarshaller
        {
            public int size(object val)
            {
                return sizeof(byte) * (System.Text.Encoding.UTF8.GetByteCount((string)val) + 1);
            }
            public object fromNative(IntPtr val)
            {
                return Utilities.getString(val);
            }
            public void toNative(object src, IntPtr dest)
            {
                byte[] strBytes = System.Text.Encoding.UTF8.GetBytes((string)src);
                Marshal.Copy(strBytes, 0, dest, strBytes.Length);
                Marshal.WriteByte(dest, strBytes.Length, 0);
            }
        };

        public static ITypeMarshaller getTypeInfo(signals.EType typ)
        {
            switch (typ)
            {
                case signals.EType.String:
                    return new String();
                case signals.EType.Boolean:
                case signals.EType.VecBoolean:
                    return new Boolean();
                case signals.EType.Byte:
                case signals.EType.VecByte:
                    return new Byte();
                case signals.EType.Short:
                case signals.EType.VecShort:
                    return new Short();
                case signals.EType.Long:
                case signals.EType.VecLong:
                    return new Long();
                case signals.EType.Single:
                case signals.EType.VecSingle:
                    return new Single();
                case signals.EType.Double:
                case signals.EType.VecDouble:
                    return new Double();
                case signals.EType.Complex:
                case signals.EType.VecComplex:
                case signals.EType.LRSingle:
                case signals.EType.VecLRSingle:
                    return new Complex();
                case signals.EType.CmplDbl:
                case signals.EType.VecCmplDbl:
                    return new ComplexDouble();
                default:
                    return null;
            }
        }
    }

    public static class Native
    {
        public interface IBlockDriver
        {
            IntPtr Name();
            IntPtr Description();
            [return:MarshalAs(UnmanagedType.Bool)]bool canCreate();
            [return:MarshalAs(UnmanagedType.Bool)]bool canDiscover();
            uint Discover(IntPtr blocks, uint availBlocks);
            IntPtr Create();
            IntPtr Fingerprint();
        };

        public interface IBlock
        {
            uint AddRef();
            uint Release();
            IntPtr Name();
            uint NodeId(IntPtr buff, uint availChar);
            IntPtr Driver();
            IntPtr Parent();
            uint Children(IntPtr blocks, uint availBlocks);
            uint Incoming(IntPtr ep, uint availEP);
            uint Outgoing(IntPtr ep, uint availEP);
            IntPtr Attributes();
            void Start();
            void Stop();
        };

        public interface IAttributes
        {
            uint Itemize(IntPtr attrs, uint availElem);
            IntPtr GetByName(IntPtr name);
            void Observe(IntPtr obs);
            void Unobserve(IntPtr obs);
        };

        public interface IAttribute
        {
            IntPtr Name();
            IntPtr Description();
            signals.EType Type();
            void Observe(IntPtr obs);
            void Unobserve(IntPtr obs);
            [return: MarshalAs(UnmanagedType.Bool)] bool isReadOnly();
            IntPtr getValue();
            [return: MarshalAs(UnmanagedType.Bool)] bool setValue(IntPtr newVal);
            uint options(IntPtr values, IntPtr opts, uint availElem);
        };

        public interface IInEndpoint
        {
            uint AddRef();
            uint Release();
            IntPtr EPName();
            signals.EType Type();
            IntPtr Attributes();
            [return: MarshalAs(UnmanagedType.Bool)] bool Connect(IntPtr recv);
            [return: MarshalAs(UnmanagedType.Bool)] bool isConnected();
            [return: MarshalAs(UnmanagedType.Bool)] bool Disconnect();
            IntPtr CreateBuffer();
        }

        public interface IOutEndpoint
        {
            IntPtr EPName();
            signals.EType Type();
            IntPtr Attributes();
            [return: MarshalAs(UnmanagedType.Bool)] bool Connect(IntPtr send);
            [return: MarshalAs(UnmanagedType.Bool)] bool isConnected();
            [return: MarshalAs(UnmanagedType.Bool)] bool Disconnect();
            IntPtr CreateBuffer();
        };

        public interface IEPSender
        {
            uint Write(signals.EType type, IntPtr buffer, uint numElem, uint msTimeout);
            uint AddRef(IntPtr iep);
            uint Release(IntPtr iep);
        };

        public interface IEPReceiver
        {
            uint Read(signals.EType type, IntPtr buffer, uint numAvail, [MarshalAs(UnmanagedType.Bool)]bool bReadAll, uint msTimeout);
            void onSinkConnected(IntPtr src);
            void onSinkDisconnected(IntPtr src);
        };

        public interface IEPBuffer : IEPSender, IEPReceiver
        {
            signals.EType Type();
            uint Capacity();
            uint Used();
        };

        public interface IAttributeObserver
        {
            void OnChanged(IntPtr name, signals.EType type, IntPtr value);
        };

	    public interface IFunctionSpec
	    {
		    IntPtr Name();
		    IntPtr Description();
		    IntPtr Create();
            IntPtr Fingerprint();
        };

	    public interface IFunction
	    {
		    IntPtr Spec();
		    uint AddRef();
		    uint Release();
		    IntPtr Input();
		    IntPtr Output();
	    };

	    public interface IInputFunction : IInEndpoint, IEPReceiver
	    {
	    };

	    public interface IOutputFunction : IOutEndpoint, IEPSender
	    {
	    };
    }

    class CppProxyModuleDriver : signals.IModule
    {
        [DllImport("kernel32.dll", SetLastError = true, CharSet = CharSet.Auto)]
        private static extern IntPtr LoadLibrary(string dllToLoad);

        [DllImport("kernel32.dll", CharSet = CharSet.Ansi, ExactSpelling = true, SetLastError = true)]
        private static extern IntPtr GetProcAddress(IntPtr hModule, string procedureName);

        [DllImport("kernel32.dll", SetLastError = true)][return: MarshalAs(UnmanagedType.Bool)]
        private static extern bool FreeLibrary(IntPtr hModule);

        private string m_file;
        private IntPtr m_hModule;
        private signals.IBlockDriver[] m_drivers = null;
        private signals.IFunctionSpec[] m_functions = null;
        private IntPtr m_queryDrivers;
        private IntPtr m_queryFunctions;

        public CppProxyModuleDriver(string file, IntPtr hModule)
        {
            if (file == null) throw new ArgumentNullException("file");
            if (hModule == IntPtr.Zero) throw new ArgumentNullException("hModule");
            m_file = file;
            m_hModule = hModule;
            m_queryDrivers = GetProcAddress(hModule, "QueryDrivers");
            m_queryFunctions = GetProcAddress(hModule, "QueryFunctions");
            if (m_queryDrivers == IntPtr.Zero && m_queryFunctions == IntPtr.Zero)
            {
                throw new NotImplementedException("module does not contain any known c++ plugin entrypoints");
            }
        }

        ~CppProxyModuleDriver()
        {
            if (m_hModule != IntPtr.Zero)
            {
                FreeLibrary(m_hModule);
                m_hModule = IntPtr.Zero;
            }
        }

        public signals.IBlockDriver[] Drivers
        {
            get
            {
                if (m_drivers == null && m_queryDrivers != IntPtr.Zero)
                {
                    Delegate queryDrivers = Marshal.GetDelegateForFunctionPointer(m_queryDrivers, typeof(QueryObjectsDelegate));
                    uint numDrivers = (uint)queryDrivers.DynamicInvoke(new object[] { null, 0u });
                    IntPtr driverBuff = Marshal.AllocHGlobal(IntPtr.Size * (int)numDrivers);
                    try
                    {
                        queryDrivers.DynamicInvoke(new object[] { driverBuff, numDrivers });
                        IntPtr[] ptrArr = new IntPtr[numDrivers];
                        Marshal.Copy(driverBuff, ptrArr, 0, (int)numDrivers);
                        m_drivers = new CppProxyBlockDriver[numDrivers];
                        for (int idx = 0; idx < numDrivers; idx++)
                        {
                            if (ptrArr[idx] != IntPtr.Zero)
                            {
                                signals.IBlockDriver newObj = (signals.IBlockDriver)Registration.retrieveObject(ptrArr[idx]);
                                if (newObj == null) newObj = new CppProxyBlockDriver(this, ptrArr[idx]);
                                m_drivers[idx] = newObj;
                            }
                        }
                    }
                    finally
                    {
                        Marshal.FreeHGlobal(driverBuff);
                    }
                }
                return m_drivers;
            }
        }

        public signals.IFunctionSpec[] Functions
        {
            get
            {
                if (m_functions == null && m_queryFunctions != IntPtr.Zero)
                {
                    Delegate queryFunctions = Marshal.GetDelegateForFunctionPointer(m_queryFunctions, typeof(QueryObjectsDelegate));
                    uint numFunctions = (uint)queryFunctions.DynamicInvoke(new object[] { null, 0u });
                    IntPtr functionBuff = Marshal.AllocHGlobal(IntPtr.Size * (int)numFunctions);
                    try
                    {
                        queryFunctions.DynamicInvoke(new object[] { functionBuff, numFunctions });
                        IntPtr[] ptrArr = new IntPtr[numFunctions];
                        Marshal.Copy(functionBuff, ptrArr, 0, (int)numFunctions);
                        m_functions = new CppProxyFunctionSpec[numFunctions];
                        for (int idx = 0; idx < numFunctions; idx++)
                        {
                            if (ptrArr[idx] != IntPtr.Zero)
                            {
                                signals.IFunctionSpec newObj = (signals.IFunctionSpec)Registration.retrieveObject(ptrArr[idx]);
                                if (newObj == null) newObj = new CppProxyFunctionSpec(this, ptrArr[idx]);
                                m_functions[idx] = newObj;
                            }
                        }
                    }
                    finally
                    {
                        Marshal.FreeHGlobal(functionBuff);
                    }
                }
                return m_functions;
            }
        }

        public signals.EModType Type { get { return signals.EModType.CPlusPlus; } }
        public string File { get { return m_file; } }

        [UnmanagedFunctionPointer(CallingConvention.StdCall)]
        private delegate uint QueryObjectsDelegate(IntPtr objects, uint availObjects);

        public static void DoDiscovery(string path, sharptest.ModLibrary library)
        {
            string[] dllList = Directory.GetFiles(path, "*.dll", SearchOption.TopDirectoryOnly);
            for (int idx = 0; idx < dllList.Length; idx++)
            {
                IntPtr hModule = LoadLibrary(dllList[idx]);
                if (hModule != IntPtr.Zero)
                {
                    CppProxyModuleDriver module = null;
                    try
                    {
                        module = new CppProxyModuleDriver(dllList[idx], hModule);
                    }
                    catch (Exception)
                    {
                        FreeLibrary(hModule);
                        continue;
                    }

                    signals.IBlockDriver[] drivers = module.Drivers;
                    if (drivers != null)
                    {
                        for (int didx = 0; didx < drivers.Length; didx++)
                        {
                            library.add(drivers[didx]);
                        }
                    }

                    signals.IFunctionSpec[] funcs = module.Functions;
                    if (funcs != null)
                    {
                        for (int fidx = 0; fidx < funcs.Length; fidx++)
                        {
                            library.add(funcs[fidx]);
                        }
                    }
                }
            }
        }
    }

    class CppProxyBlockDriver : signals.IBlockDriver
    {
        private CppProxyModuleDriver m_parent;
        private Native.IBlockDriver m_native;
        private IntPtr m_nativeRef;
        private bool m_canCreate;
        private bool m_canDiscover;
        private string m_name;
        private string m_descr;
        private signals.Fingerprint m_fingerprint;
        public const int DEFAULT_BUFFER = 100;
        public int BufferSize = DEFAULT_BUFFER;

        public CppProxyBlockDriver(CppProxyModuleDriver parent, IntPtr native)
        {
            if (parent == null) throw new ArgumentNullException("parent");
            if (native == IntPtr.Zero) throw new ArgumentNullException("native");
            m_parent = parent;
            m_nativeRef = native;
            Registration.storeObject(native, this);
            m_native = (Native.IBlockDriver)CppNativeProxy.CreateCallout(native, typeof(Native.IBlockDriver));
            interrogate();
        }

        private void interrogate()
        {
            m_name = Utilities.getString(m_native.Name());
            m_descr = Utilities.getString(m_native.Description());
            m_canCreate = m_native.canCreate();
            m_canDiscover = m_native.canDiscover();
            m_fingerprint = decodeFingerprint(m_native.Fingerprint());
        }

        private static unsafe signals.Fingerprint decodeFingerprint(IntPtr fgr)
        {
            if (fgr == IntPtr.Zero) return null;
            signals.Fingerprint val = new signals.Fingerprint();
            byte* ptr = (byte*)fgr.ToPointer();
            int len = *ptr++;
            val.inputs = new signals.EType[len];
            for (int idx = 0; idx < len; idx++) val.inputs[idx] = (signals.EType)(int) *ptr++;
            len = *ptr++;
            val.outputs = new signals.EType[len];
            for (int idx = 0; idx < len; idx++) val.outputs[idx] = (signals.EType)(int) *ptr++;
            return val;
        }

        ~CppProxyBlockDriver()
        {
            if (m_nativeRef != IntPtr.Zero)
            {
                Registration.removeObject(m_nativeRef);
                m_nativeRef = IntPtr.Zero;
            }
        }

        public signals.IModule Module { get { return m_parent; } }
        public string Name { get { return m_name; } }
        public string Description { get { return m_descr; } }
        public bool canCreate { get { return m_canCreate; } }
        public bool canDiscover { get { return m_canDiscover; } }
        public signals.Fingerprint Fingerprint { get { return m_fingerprint; } }

        public signals.IBlock Create()
        {
            if (!m_canCreate) throw new NotSupportedException("Driver cannot create objects.");
            IntPtr newObjRef = m_native.Create();
            if (newObjRef == IntPtr.Zero) return null;
            signals.IBlock newObj = (signals.IBlock)Registration.retrieveObject(newObjRef);
            if (newObj == null) newObj = new CppProxyBlock(this, null, newObjRef);
            return newObj;
        }

        public signals.IBlock[] Discover()
        {   // discovery can be expensive, so we just use a large buffer rather than call things multiple times
            if (!m_canDiscover) throw new NotSupportedException("Driver cannot discover objects.");
            IntPtr discoverBuff = Marshal.AllocHGlobal(IntPtr.Size * BufferSize);
            try
            {
                uint numObj = m_native.Discover(discoverBuff, (uint)BufferSize);
                IntPtr[] ptrArr = new IntPtr[numObj];
                Marshal.Copy(discoverBuff, ptrArr, 0, (int)numObj);
                signals.IBlock[] objArr = new signals.IBlock[numObj];
                for (int idx = 0; idx < numObj; idx++)
                {
                    if (ptrArr[idx] != IntPtr.Zero)
                    {
                        signals.IBlock newObj = (signals.IBlock)Registration.retrieveObject(ptrArr[idx]);
                        if (newObj == null) newObj = new CppProxyBlock(this, null, ptrArr[idx]);
                        objArr[idx] = newObj;
                    }
                }
                return objArr;
            }
            finally
            {
                Marshal.FreeHGlobal(discoverBuff);
            }
        }
    };

    interface ICollectible
    {
        bool isValid { get; }
    };

    class CppProxyBlock : signals.IBlock, ICollectible
    {
        private Native.IBlock m_native;
        private IntPtr m_nativeRef;
        private readonly signals.IBlockDriver m_driver;
        private readonly signals.IBlock m_parent;
        private string m_name;
        private string m_nodeId;
        private signals.IAttributes m_attrs = null;
        private signals.IBlock[] m_children = null;
        private signals.IInEndpoint[] m_incoming = null;
        private signals.IOutEndpoint[] m_outgoing = null;
        private signals.Fingerprint m_print = null;

        public CppProxyBlock(signals.IBlockDriver driver, signals.IBlock parent, IntPtr native)
        {
            if (driver == null) throw new ArgumentNullException("driver");
            if (native == IntPtr.Zero) throw new ArgumentNullException("native");
            m_nativeRef = native;
            m_driver = driver;
            m_parent = parent;
            Registration.storeObject(native, this);
            m_native = (Native.IBlock)CppNativeProxy.CreateCallout(native, typeof(Native.IBlock));
            interrogate();
        }

        ~CppProxyBlock()
        {
            Dispose(false);
        }

        private void interrogate()
        {
            m_name = Utilities.getString(m_native.Name());

            m_nodeId = null;
            uint nodeIdSize = m_native.NodeId(IntPtr.Zero, 0u);
            if (nodeIdSize > 0)
            {
                IntPtr nodeIdBuff = Marshal.AllocHGlobal((int)nodeIdSize + 1);
                try
                {
                    m_native.NodeId(nodeIdBuff, nodeIdSize);
                    m_nodeId = Utilities.getString(nodeIdBuff, (int)nodeIdSize);
                }
                finally
                {
                    Marshal.FreeHGlobal(nodeIdBuff);
                }
            }
        }

        public void Dispose()
        {
            Dispose(true);
            GC.SuppressFinalize(this);
        }

        protected virtual void Dispose(bool disposing)
        {
            if (m_native != null)
            {
                Native.IBlock localNative = m_native;
                m_native = null;
                localNative.Release();
            }
            if (m_nativeRef != IntPtr.Zero)
            {
                Registration.removeObject(m_nativeRef);
                m_nativeRef = IntPtr.Zero;
            }
        }

        public bool isValid { get { return m_native != null; } }

        public string Name { get { return m_name; } }
        public string NodeId { get { return m_nodeId; } }
        public signals.IBlockDriver Driver { get { return m_driver; } }
        public signals.IBlock Parent { get { return m_parent; } }
        public void Start() { m_native.Start(); }
        public void Stop() { m_native.Stop(); }

        public signals.IAttributes Attributes
        {
            get
            {
                if (m_attrs == null)
                {
                    IntPtr attrs = m_native.Attributes();
                    if (attrs == IntPtr.Zero) return null;
                    m_attrs = (signals.IAttributes)Registration.retrieveObject(attrs);
                    if (m_attrs == null) m_attrs = new CppProxyAttributes(this, attrs);
                }
                return m_attrs;
            }
        }

        public signals.IBlock[] Children
        {
            get
            {
                if (m_children == null)
                {
                    uint numChildren = m_native.Children(IntPtr.Zero, 0);
                    IntPtr childBuff = Marshal.AllocHGlobal(IntPtr.Size * (int)numChildren);
                    try
                    {
                        m_native.Children(childBuff, numChildren);
                        IntPtr[] ptrArr = new IntPtr[numChildren];
                        Marshal.Copy(childBuff, ptrArr, 0, (int)numChildren);
                        m_children = new CppProxyBlock[numChildren];
                        for (int idx = 0; idx < numChildren; idx++)
                        {

                            if (ptrArr[idx] != IntPtr.Zero)
                            {
                                signals.IBlock newObj = (signals.IBlock)Registration.retrieveObject(ptrArr[idx]);
                                if (newObj == null) newObj = new CppProxyBlock(m_driver, this, ptrArr[idx]);
                                m_children[idx] = newObj;
                            }
                        }
                    }
                    finally
                    {
                        Marshal.FreeHGlobal(childBuff);
                    }
                }
                return m_children;
            }
        }

        public signals.IInEndpoint[] Incoming
        {
            get
            {
                if (m_incoming == null)
                {
                    uint numEP = m_native.Incoming(IntPtr.Zero, 0);
                    IntPtr epBuff = Marshal.AllocHGlobal(IntPtr.Size * (int)numEP);
                    try
                    {
                        m_native.Incoming(epBuff, numEP);
                        IntPtr[] ptrArr = new IntPtr[numEP];
                        Marshal.Copy(epBuff, ptrArr, 0, (int)numEP);
                        m_incoming = new CppProxyInEndpoint[numEP];
                        for (int idx = 0; idx < numEP; idx++)
                        {

                            if (ptrArr[idx] != IntPtr.Zero)
                            {
                                signals.IInEndpoint newObj = (signals.IInEndpoint)Registration.retrieveObject(ptrArr[idx]);
                                if (newObj == null) newObj = new CppProxyInEndpoint(m_driver.Module, this, ptrArr[idx]);
                                m_incoming[idx] = newObj;
                            }
                        }
                    }
                    finally
                    {
                        Marshal.FreeHGlobal(epBuff);
                    }
                }
                return m_incoming;
            }
        }

        public signals.IOutEndpoint[] Outgoing
        {
            get
            {
                if (m_outgoing == null)
                {
                    uint numEP = m_native.Outgoing(IntPtr.Zero, 0);
                    IntPtr epBuff = Marshal.AllocHGlobal(IntPtr.Size * (int)numEP);
                    try
                    {
                        m_native.Outgoing(epBuff, numEP);
                        IntPtr[] ptrArr = new IntPtr[numEP];
                        Marshal.Copy(epBuff, ptrArr, 0, (int)numEP);
                        m_outgoing = new CppProxyOutEndpoint[numEP];
                        for (int idx = 0; idx < numEP; idx++)
                        {

                            if (ptrArr[idx] != IntPtr.Zero)
                            {
                                signals.IOutEndpoint newObj = (signals.IOutEndpoint)Registration.retrieveObject(ptrArr[idx]);
                                if (newObj == null) newObj = new CppProxyOutEndpoint(m_driver.Module, this, ptrArr[idx]);
                                m_outgoing[idx] = newObj;
                            }
                        }
                    }
                    finally
                    {
                        Marshal.FreeHGlobal(epBuff);
                    }
                }
                return m_outgoing;
            }
        }

        public signals.Fingerprint Fingerprint
        {
            get
            {
                if (m_print == null)
                {
                    signals.IInEndpoint[] incom = this.Incoming;
                    signals.IOutEndpoint[] outgo = this.Outgoing;
                    m_print = new signals.Fingerprint();
                    m_print.inputs = new signals.EType[incom.Length];
                    m_print.inputNames = new string[incom.Length];
                    for (int idx = 0; idx < incom.Length; idx++)
                    {
                        m_print.inputs[idx] = incom[idx].Type;
                        m_print.inputNames[idx] = incom[idx].EPName;
                    }
                    m_print.outputs = new signals.EType[outgo.Length];
                    m_print.outputNames = new string[outgo.Length];
                    for (int idx = 0; idx < outgo.Length; idx++)
                    {
                        m_print.outputs[idx] = outgo[idx].Type;
                        m_print.outputNames[idx] = outgo[idx].EPName;
                    }
                }
                return m_print;
            }
        }
    }

    class CppProxyAttributes : signals.IAttributes
    {
        private Native.IAttributes m_native;
        private IntPtr m_nativeRef;
        private readonly ICollectible m_owner;

        public CppProxyAttributes(ICollectible owner, IntPtr native)
        {
            if (owner == null) throw new ArgumentNullException("owner");
            if (native == IntPtr.Zero) throw new ArgumentNullException("native");
            m_owner = owner;
            m_nativeRef = native;
            Registration.storeObject(native, this);
            m_native = (Native.IAttributes)CppNativeProxy.CreateCallout(native, typeof(Native.IAttributes));
        }

        ~CppProxyAttributes()
        {
            if (m_nativeRef != IntPtr.Zero)
            {
                Registration.removeObject(m_nativeRef);
                m_nativeRef = IntPtr.Zero;
            }
        }

        public signals.IAttribute GetByName(string name)
        {
            if (name == null) throw new ArgumentNullException("name");
            byte[] strName = System.Text.Encoding.UTF8.GetBytes(name);
            IntPtr nameBuff = Marshal.AllocHGlobal(strName.Length+1);
            try
            {
                Marshal.Copy(strName, 0, nameBuff, strName.Length);
                Marshal.WriteByte(nameBuff, strName.Length, 0);
                IntPtr attrRef = m_native.GetByName(nameBuff);
                if (attrRef == IntPtr.Zero) return null;
                signals.IAttribute newObj = (signals.IAttribute)Registration.retrieveObject(attrRef);
                if (newObj == null) newObj = new CppProxyAttribute(m_owner, attrRef);
                return newObj;
            }
            finally
            {
                Marshal.FreeHGlobal(nameBuff);
            }
        }

        public signals.IAttribute[] Itemize()
        {
            uint numAttr = m_native.Itemize(IntPtr.Zero, 0);
            IntPtr attrBuff = Marshal.AllocHGlobal(IntPtr.Size * (int)numAttr);
            try
            {
                uint numObj = m_native.Itemize(attrBuff, numAttr);
                IntPtr[] ptrArr = new IntPtr[numObj];
                Marshal.Copy(attrBuff, ptrArr, 0, (int)numObj);
                signals.IAttribute[] attrArray = new signals.IAttribute[numObj];
                for (int idx = 0; idx < numObj; idx++)
                {
                    if (ptrArr[idx] != IntPtr.Zero)
                    {
                        signals.IAttribute newObj = (signals.IAttribute)Registration.retrieveObject(ptrArr[idx]);
                        if (newObj == null) newObj = new CppProxyAttribute(m_owner, ptrArr[idx]);
                        attrArray[idx] = newObj;
                    }
                }
                return attrArray;
            }
            finally
            {
                Marshal.FreeHGlobal(attrBuff);
            }
        }
    }

    class CppProxyAttribute : signals.IAttribute
    {
        private class ChangedCatcher : Native.IAttributeObserver
        {
            private readonly CppProxyAttribute parent;

            public ChangedCatcher(CppProxyAttribute parent)
            {
                this.parent = parent;
            }

            public void OnChanged(IntPtr name, signals.EType type, IntPtr value)
            {
                string strName = Utilities.getString(name);
                object newVal = null;
                if (value != IntPtr.Zero && parent.m_typeInfo != null)
                {
                    newVal = parent.m_typeInfo.fromNative(value);
                }
                if(parent.changed != null) parent.changed(strName, type, newVal);
            }
        }

        public event signals.OnChanged changed;
        private readonly ICollectible m_owner;
        private Native.IAttribute m_native;
        private IntPtr m_nativeRef;
        private string m_name;
        private string m_descr;
        private signals.EType m_type;
        private ProxyTypes.ITypeMarshaller m_typeInfo;
        private bool m_readOnly;
        private ChangedCatcher m_catcher;
        private CppNativeProxy.Callback m_callback;

        public CppProxyAttribute(ICollectible owner, IntPtr native)
        {
            if (owner == null) throw new ArgumentNullException("driver");
            if (native == IntPtr.Zero) throw new ArgumentNullException("native");
            m_owner = owner;
            m_nativeRef = native;
            Registration.storeObject(native, this);
            m_native = (Native.IAttribute)CppNativeProxy.CreateCallout(native, typeof(Native.IAttribute));
            m_catcher = new ChangedCatcher(this);
            interrogate();
        }

        ~CppProxyAttribute()
        {
            Dispose(false);
        }

        public void Dispose()
        {
            Dispose(true);
            GC.SuppressFinalize(this);
        }

        protected virtual void Dispose(bool disposing)
        {
            if (m_callback != null && m_owner.isValid)
            {
                m_native.Unobserve(m_callback.Pointer);
                m_callback = null;
            }
            if (m_nativeRef != IntPtr.Zero)
            {
                Registration.removeObject(m_nativeRef);
                m_nativeRef = IntPtr.Zero;
            }
        }

        private void interrogate()
        {
            m_name = Utilities.getString(m_native.Name());
            m_descr = Utilities.getString(m_native.Description());
            m_type = m_native.Type();
            m_typeInfo = ProxyTypes.getTypeInfo(m_type);
            m_readOnly = m_native.isReadOnly();
            m_callback = CppNativeProxy.CreateCallin(m_catcher, typeof(Native.IAttributeObserver));
            m_native.Observe(m_callback.Pointer);
        }

        public string Name { get { return m_name; } }
        public string Description { get { return m_descr; } }
        public signals.EType Type { get { return m_type; } }
        public bool isReadOnly { get { return m_readOnly; } }

        public object Value
        {
            get
            {
                if (m_type == signals.EType.None) return null;
                if (m_typeInfo == null) throw new NotSupportedException("Cannot retrieve value for this type.");
                IntPtr rawVal = m_native.getValue();
                if (rawVal == IntPtr.Zero) return null;
                return m_typeInfo.fromNative(rawVal);
            }
            set
            {
                if (m_typeInfo == null) throw new NotSupportedException("Cannot store value for this type.");
                if (value == null) throw new ArgumentNullException("value");
                IntPtr buff = IntPtr.Zero;
                try
                {
                    buff = Marshal.AllocHGlobal(m_typeInfo.size(value));
                    m_typeInfo.toNative(value, buff);
                    m_native.setValue(buff);
                }
                finally
                {
                    if (buff != IntPtr.Zero)
                    {
                        Marshal.FreeHGlobal(buff);
                    }
                }
            }
        }

        public void options(out object[] values, out string[] opts)
        {
            if (m_typeInfo == null) throw new NotSupportedException("Cannot store value for this type.");
            uint numOpt = m_native.options(IntPtr.Zero, IntPtr.Zero, 0);
            if (numOpt == 0)
            {
                values = null;
                opts = null;
                return;
            }
            IntPtr valList = Marshal.AllocHGlobal(IntPtr.Size * (int)numOpt);
            IntPtr optList = Marshal.AllocHGlobal(IntPtr.Size * (int)numOpt);
            try
            {
                m_native.options(valList, optList, numOpt);
                IntPtr[] valArr = new IntPtr[numOpt];
                Marshal.Copy(valList, valArr, 0, (int)numOpt);
                IntPtr[] optArr = new IntPtr[numOpt];
                Marshal.Copy(optList, optArr, 0, (int)numOpt);
                values = new object[numOpt];
                opts = new string[numOpt];
                for (uint idx = 0; idx < numOpt; idx++)
                {
                    if (valArr[idx] != null)
                    {
                        values[idx] = m_typeInfo.fromNative(valArr[idx]);
                    }
                    if (optArr[idx] != null)
                    {
                        opts[idx] = Utilities.getString(optArr[idx]);
                    }
                }
            }
            finally
            {
                Marshal.FreeHGlobal(valList);
                Marshal.FreeHGlobal(optList);
            }
        }
    }

    class CppProxyInEndpoint : signals.IInEndpoint
    {
        private Native.IInEndpoint m_native;
        private readonly signals.IModule m_module;
        private readonly ICollectible m_owner;
        private IntPtr m_nativeRef;
        private string m_name;
        private signals.EType m_type;
        private signals.IAttributes m_attrs = null;

        public CppProxyInEndpoint(signals.IModule module, ICollectible owner, IntPtr native)
        {
            if (module == null) throw new ArgumentNullException("module");
            if (owner == null) throw new ArgumentNullException("owner");
            if (native == IntPtr.Zero) throw new ArgumentNullException("native");
            m_module = module;
            m_nativeRef = native;
            m_owner = owner;
            Registration.storeObject(native, this);
            m_native = (Native.IInEndpoint)CppNativeProxy.CreateCallout(native, typeof(Native.IInEndpoint));
            interrogate();
        }

        private void interrogate()
        {
            m_name = Utilities.getString(m_native.EPName());
            m_type = m_native.Type();
        }

        ~CppProxyInEndpoint()
        {
            Dispose(false);
        }

        public void Dispose()
        {
            Dispose(true);
            GC.SuppressFinalize(this);
        }

        protected virtual void Dispose(bool disposing)
        {
            if (m_native != null)
            {
                m_native.Release();
                m_native = null;
            }
            if (m_nativeRef != IntPtr.Zero)
            {
                Registration.removeObject(m_nativeRef);
                m_nativeRef = IntPtr.Zero;
            }
        }

        public signals.IAttributes Attributes
        {
            get
            {
                if (m_attrs == null)
                {
                    IntPtr attrs = m_native.Attributes();
                    if (attrs == IntPtr.Zero) return null;
                    m_attrs = (signals.IAttributes)Registration.retrieveObject(attrs);
                    if (m_attrs == null) m_attrs = new CppProxyAttributes(m_owner, attrs);
                }
                return m_attrs;
            }
        }

        public string EPName { get { return m_name; } }
        public signals.EType Type { get { return m_type; } }
        public bool isConnected { get { return m_native.isConnected(); } }
        public bool Disconnect() { return m_native.Disconnect(); }

        public signals.IEPBuffer CreateBuffer()
        {
            IntPtr pBuf = m_native.CreateBuffer();
            if (pBuf == IntPtr.Zero) return null;
            signals.IEPBuffer buff = (signals.IEPBuffer)Registration.retrieveObject(pBuf);
            if (m_attrs == null) buff = new CppProxyBuffer(m_module, pBuf);
            return buff;
        }

        public bool Connect(signals.IEPReceiver recv)
        {
            {
                CppProxyEPReceiver proxy = recv as CppProxyEPReceiver;
                if (proxy != null)
                {
                    return m_native.Connect(proxy.Native);
                }
            }
            {
                CppProxyBuffer proxy = recv as CppProxyBuffer;
                if (proxy != null)
                {
                    return m_native.Connect(proxy.NativeReceiver);
                }
            }
            {
                CppProxyInputFunction proxy = recv as CppProxyInputFunction;
                if (proxy != null)
                {
                    return m_native.Connect(proxy.NativeReceiver);
                }
            }
            throw new NotSupportedException("Native connections not yet implemented.");
        }
    }

    class CppProxyOutEndpoint : signals.IOutEndpoint
    {
        private Native.IOutEndpoint m_native;
        private readonly signals.IModule m_module;
        private readonly ICollectible m_owner;
        private IntPtr m_nativeRef;
        private string m_name;
        private signals.EType m_type;
        private signals.IAttributes m_attrs = null;

        public CppProxyOutEndpoint(signals.IModule module, ICollectible owner, IntPtr native)
        {
            if (module == null) throw new ArgumentNullException("module");
            if (native == IntPtr.Zero) throw new ArgumentNullException("native");
            m_module = module;
            m_nativeRef = native;
            m_owner = owner;
            Registration.storeObject(native, this);
            m_native = (Native.IOutEndpoint)CppNativeProxy.CreateCallout(native, typeof(Native.IOutEndpoint));
            interrogate();
        }

        ~CppProxyOutEndpoint()
        {
            if (m_nativeRef != IntPtr.Zero)
            {
                Registration.removeObject(m_nativeRef);
                m_nativeRef = IntPtr.Zero;
            }
        }

        private void interrogate()
        {
            m_name = Utilities.getString(m_native.EPName());
            m_type = m_native.Type();
        }

        public signals.IAttributes Attributes
        {
            get
            {
                if (m_attrs == null)
                {
                    IntPtr attrs = m_native.Attributes();
                    if (attrs == IntPtr.Zero) return null;
                    m_attrs = (signals.IAttributes)Registration.retrieveObject(attrs);
                    if (m_attrs == null) m_attrs = new CppProxyAttributes(m_owner, attrs);
                }
                return m_attrs;
            }
        }

        public virtual void Dispose() { }
        public string EPName { get { return m_name; } }
        public signals.EType Type { get { return m_type; } }
        public bool isConnected { get { return m_native.isConnected(); } }
        public bool Disconnect() { return m_native.Disconnect(); }

        public signals.IEPBuffer CreateBuffer()
        {
            IntPtr pBuf = m_native.CreateBuffer();
            if (pBuf == IntPtr.Zero) return null;
            signals.IEPBuffer buff = (signals.IEPBuffer)Registration.retrieveObject(pBuf);
            if (m_attrs == null) buff = new CppProxyBuffer(m_module, pBuf);
            return buff;
        }

        public bool Connect(signals.IEPSender recv)
        {
            {
                CppProxyEPSender proxy = recv as CppProxyEPSender;
                if (proxy != null)
                {
                    return m_native.Connect(proxy.Native);
                }
            }
            {
                CppProxyBuffer proxy = recv as CppProxyBuffer;
                if (proxy != null)
                {
                    return m_native.Connect(proxy.NativeSender);
                }
            }
            {
                CppProxyOutputFunction proxy = recv as CppProxyOutputFunction;
                if (proxy != null)
                {
                    return m_native.Connect(proxy.NativeSender);
                }
            }
            throw new NotSupportedException("Native connections not yet implemented.");
        }
    }

    class CppProxyEPSender : signals.IEPSender
    {
        private readonly signals.IBlockDriver m_driver;
        private Native.IEPSender m_native;
        private IntPtr m_nativeRef;
        private signals.EType m_type = signals.EType.None;
        private ProxyTypes.ITypeMarshaller m_typeInfo = null;

        public CppProxyEPSender(signals.IBlockDriver driver, IntPtr native)
        {
            if (driver == null) throw new ArgumentNullException("driver");
            if (native == IntPtr.Zero) throw new ArgumentNullException("native");
            m_driver = driver;
            m_nativeRef = native;
            Registration.storeObject(native, this);
            m_native = (Native.IEPSender)CppNativeProxy.CreateCallout(native, typeof(Native.IEPSender));
        }

        ~CppProxyEPSender()
        {
            Dispose(false);
        }

        public void Dispose()
        {
            Dispose(true);
            GC.SuppressFinalize(this);
        }

        protected virtual void Dispose(bool disposing)
        {
            if (m_native != null)
            {
                m_native.Release(IntPtr.Zero);
                m_native = null;
            }
            if (m_nativeRef != IntPtr.Zero)
            {
                Registration.removeObject(m_nativeRef);
                m_nativeRef = IntPtr.Zero;
            }
        }

        public IntPtr Native { get { return m_nativeRef; } }

        public int Write(signals.EType type, object[] values, int msTimeout)
        {
            if (values == null) throw new ArgumentNullException("values");
            if (type != m_type || m_typeInfo == null)
            {
                m_type = type;
                m_typeInfo = ProxyTypes.getTypeInfo(type);
            }
            if (m_typeInfo == null) throw new NotSupportedException("Cannot retrieve value for this type.");
            int elemSize = m_typeInfo.size(null);
            IntPtr buff = Marshal.AllocHGlobal(elemSize * values.Length);
            try
            {
                for (int idx = 0; idx < values.Length; idx++)
                {
                    IntPtr here = new IntPtr(buff.ToInt64() + idx * elemSize);
                    m_typeInfo.toNative(values[idx], here);
                }
                return (int)m_native.Write(type, buff, (uint)values.Length, (uint)msTimeout);
            }
            finally
            {
                Marshal.FreeHGlobal(buff);
            }
        }
    }

    class CppProxyEPReceiver : signals.IEPReceiver
    {
        private readonly signals.IBlockDriver m_driver;
        private Native.IEPReceiver m_native;
        private IntPtr m_nativeRef;
        private signals.EType m_type = signals.EType.None;
        private ProxyTypes.ITypeMarshaller m_typeInfo = null;
        public const int DEFAULT_BUFFER = 100;
        public int BufferSize = DEFAULT_BUFFER;

        public CppProxyEPReceiver(signals.IBlockDriver driver, IntPtr native)
        {
            if (driver == null) throw new ArgumentNullException("driver");
            if (native == IntPtr.Zero) throw new ArgumentNullException("native");
            m_driver = driver;
            m_nativeRef = native;
            Registration.storeObject(native, this);
            m_native = (Native.IEPReceiver)CppNativeProxy.CreateCallout(native, typeof(Native.IEPReceiver));
        }

        ~CppProxyEPReceiver()
        {
            if (m_nativeRef != IntPtr.Zero)
            {
                Registration.removeObject(m_nativeRef);
                m_nativeRef = IntPtr.Zero;
            }
        }

        public void Dispose() { }
        public IntPtr Native { get { return m_nativeRef; } }

        public void Read(signals.EType type, out object[] values, bool bReadAll, int msTimeout)
        {
            if (type != m_type || m_typeInfo == null)
            {
                m_type = type;
                m_typeInfo = ProxyTypes.getTypeInfo(type);
            }
            if (m_typeInfo == null) throw new NotSupportedException("Cannot retrieve value for this type.");
            int elemSize = m_typeInfo.size(null);
            IntPtr buff = Marshal.AllocHGlobal(elemSize * BufferSize);
            try
            {
                uint read = m_native.Read(type, buff, (uint)BufferSize, bReadAll, (uint)msTimeout);
                values = new object[read];
                for (uint idx = 0; idx < read; idx++)
                {
                    IntPtr here = new IntPtr(buff.ToInt64() + idx * elemSize);
                    values[idx] = m_typeInfo.fromNative(here);
                }
            }
            finally
            {
                Marshal.FreeHGlobal(buff);
            }
        }
    }

    class CppProxyBuffer : signals.IEPBuffer
    {
        private readonly signals.IModule m_module;
        private Native.IEPBuffer m_native;
        private IntPtr m_nativeRef;
        private signals.EType m_type;
        private ProxyTypes.ITypeMarshaller m_typeInfo = null;
        public const int DEFAULT_BUFFER = 5000;
        public int BufferSize = DEFAULT_BUFFER;

        public CppProxyBuffer(signals.IModule module, IntPtr native)
        {
            if (module == null) throw new ArgumentNullException("module");
            if (native == IntPtr.Zero) throw new ArgumentNullException("native");
            m_module = module;
            m_nativeRef = native;
            Registration.storeObject(native, this);
            m_native = (Native.IEPBuffer)CppNativeProxy.CreateCallout(native, typeof(Native.IEPBuffer));
            interrogate();
        }

        ~CppProxyBuffer()
        {
            Dispose(false);
        }

        private void interrogate()
        {
            m_type = m_native.Type();
            m_typeInfo = ProxyTypes.getTypeInfo(m_type);
        }

        public void Dispose()
        {
            Dispose(true);
            GC.SuppressFinalize(this);
        }

        protected virtual void Dispose(bool disposing)
        {
            if (m_native != null)
            {
                m_native.Release(IntPtr.Zero);
                m_native = null;
            }
            if (m_nativeRef != IntPtr.Zero)
            {
                Registration.removeObject(m_nativeRef);
                m_nativeRef = IntPtr.Zero;
            }
        }

        public signals.EType Type { get { return m_type; } }
        public int Capacity { get { return (int)m_native.Capacity(); } }
        public int Used { get { return (int)m_native.Used(); } }
        public IntPtr NativeSender { get { return m_nativeRef; } }
        public IntPtr NativeReceiver { get { return new IntPtr(m_nativeRef.ToInt64() + IntPtr.Size); } }

        public void Read(signals.EType type, out object[] values, bool bReadAll, int msTimeout)
        {
            if (m_typeInfo == null) throw new NotSupportedException("Cannot retrieve value for this type.");
            int elemSize = m_typeInfo.size(null);
            IntPtr buff = Marshal.AllocHGlobal(elemSize * BufferSize);
            try
            {
                uint read = m_native.Read(m_type, buff, (uint)BufferSize, bReadAll, (uint)msTimeout);
                values = new object[read];
                for (uint idx = 0; idx < read; idx++)
                {
                    IntPtr here = new IntPtr(buff.ToInt64() + idx * elemSize);
                    values[idx] = m_typeInfo.fromNative(here);
                }
            }
            finally
            {
                Marshal.FreeHGlobal(buff);
            }
        }

        public int Write(signals.EType type, object[] values, int msTimeout)
        {
            if (values == null) throw new ArgumentNullException("values");
            if (m_typeInfo == null) throw new NotSupportedException("Cannot retrieve value for this type.");
            int elemSize = m_typeInfo.size(null);
            IntPtr buff = Marshal.AllocHGlobal(elemSize * values.Length);
            try
            {
                for (int idx = 0; idx < values.Length; idx++)
                {
                    IntPtr here = new IntPtr(buff.ToInt64() + idx * elemSize);
                    m_typeInfo.toNative(values[idx], here);
                }
                return (int)m_native.Write(type, buff, (uint)values.Length, (uint)msTimeout);
            }
            finally
            {
                Marshal.FreeHGlobal(buff);
            }
        }
    }

    class CppProxyFunctionSpec : signals.IFunctionSpec
    {
        private CppProxyModuleDriver m_parent;
        private Native.IFunctionSpec m_native;
        private IntPtr m_nativeRef;
        private string m_name;
        private string m_descr;
        private signals.Fingerprint m_fingerprint;

        public CppProxyFunctionSpec(CppProxyModuleDriver parent, IntPtr native)
        {
            if (parent == null) throw new ArgumentNullException("parent");
            if (native == IntPtr.Zero) throw new ArgumentNullException("native");
            m_parent = parent;
            m_nativeRef = native;
            Registration.storeObject(native, this);
            m_native = (Native.IFunctionSpec)CppNativeProxy.CreateCallout(native, typeof(Native.IFunctionSpec));
            interrogate();
        }

        private void interrogate()
        {
            m_name = Utilities.getString(m_native.Name());
            m_descr = Utilities.getString(m_native.Description());
            m_fingerprint = decodeFingerprint(m_native.Fingerprint());
        }

        private static signals.Fingerprint decodeFingerprint(IntPtr fgr)
        {
            if (fgr == IntPtr.Zero) return null;
            signals.Fingerprint val = new signals.Fingerprint();
            val.inputs = new signals.EType[1];
            val.inputs[0] = (signals.EType)(int)Marshal.ReadByte(fgr, 0);
            val.outputs = new signals.EType[1];
            val.outputs[0] = (signals.EType)(int)Marshal.ReadByte(fgr, 1);
            return val;
        }

        ~CppProxyFunctionSpec()
        {
            if (m_nativeRef != IntPtr.Zero)
            {
                Registration.removeObject(m_nativeRef);
                m_nativeRef = IntPtr.Zero;
            }
        }

        public signals.IModule Module { get { return m_parent; } }
        public string Name { get { return m_name; } }
        public string Description { get { return m_descr; } }
        public signals.Fingerprint Fingerprint { get { return m_fingerprint; } }

        public signals.IFunction Create()
        {
            IntPtr newObjRef = m_native.Create();
            if (newObjRef == IntPtr.Zero) return null;
            signals.IFunction newObj = (signals.IFunction)Registration.retrieveObject(newObjRef);
            if (newObj == null) newObj = new CppProxyFunction(this, newObjRef);
            return newObj;
        }
    }

    class CppProxyFunction : signals.IFunction, ICollectible
    {
        private Native.IFunction m_native;
        private IntPtr m_nativeRef;
        private readonly signals.IFunctionSpec m_spec;
        private signals.IInputFunction m_incoming = null;
        private signals.IOutputFunction m_outgoing = null;

        public CppProxyFunction(signals.IFunctionSpec spec, IntPtr native)
        {
            if (spec == null) throw new ArgumentNullException("spec");
            if (native == IntPtr.Zero) throw new ArgumentNullException("native");
            m_nativeRef = native;
            m_spec = spec;
            Registration.storeObject(native, this);
            m_native = (Native.IFunction)CppNativeProxy.CreateCallout(native, typeof(Native.IFunction));
        }

        ~CppProxyFunction()
        {
            Dispose(false);
        }

        public void Dispose()
        {
            Dispose(true);
            GC.SuppressFinalize(this);
        }

        protected virtual void Dispose(bool disposing)
        {
            if (m_native != null)
            {
                Native.IFunction localNative = m_native;
                m_native = null;
                localNative.Release();
            }
            if (m_nativeRef != IntPtr.Zero)
            {
                Registration.removeObject(m_nativeRef);
                m_nativeRef = IntPtr.Zero;
            }
        }

        public bool isValid { get { return m_native != null; } }
        public signals.IFunctionSpec Spec { get { return m_spec; } }

        public signals.IInputFunction Input
        {
            get
            {
                if (m_incoming == null)
                {
                    IntPtr newObjRef = m_native.Input();
                    if (newObjRef == IntPtr.Zero) return null;
                    m_incoming = (signals.IInputFunction)Registration.retrieveObject(newObjRef);
                    if (m_incoming == null) m_incoming = new CppProxyInputFunction(m_spec.Module, this, newObjRef);
                }
                return m_incoming;
            }
        }

        public signals.IOutputFunction Output
        {
            get
            {
                if (m_outgoing == null)
                {
                    IntPtr newObjRef = m_native.Output();
                    if (newObjRef == IntPtr.Zero) return null;
                    m_outgoing = (signals.IOutputFunction)Registration.retrieveObject(newObjRef);
                    if (m_outgoing == null) m_outgoing = new CppProxyOutputFunction(m_spec.Module, this, newObjRef);
                }
                return m_outgoing;
            }
        }
    }

    class CppProxyInputFunction : CppProxyInEndpoint, signals.IInputFunction
	{
        private Native.IEPReceiver m_nativeRecv;
        private IntPtr m_nativeRecvRef;
        private signals.EType m_recvType = signals.EType.None;
        private ProxyTypes.ITypeMarshaller m_recvTypeInfo = null;
        public const int DEFAULT_BUFFER = 100;
        public int BufferSize = DEFAULT_BUFFER;

        public CppProxyInputFunction(signals.IModule module, ICollectible owner, IntPtr native)
            :base(module, owner, native)
        {
            m_nativeRecvRef = new IntPtr(native.ToInt64() + IntPtr.Size);
            m_nativeRecv = (Native.IEPReceiver)CppNativeProxy.CreateCallout(m_nativeRecvRef, typeof(Native.IEPReceiver));
        }

        public IntPtr NativeReceiver { get { return m_nativeRecvRef; } }

        public void Read(signals.EType type, out object[] values, bool bReadAll, int msTimeout)
        {
            if (type != m_recvType || m_recvTypeInfo == null)
            {
                m_recvType = type;
                m_recvTypeInfo = ProxyTypes.getTypeInfo(type);
            }
            if (m_recvTypeInfo == null) throw new NotSupportedException("Cannot retrieve value for this type.");
            int elemSize = m_recvTypeInfo.size(null);
            IntPtr buff = Marshal.AllocHGlobal(elemSize * BufferSize);
            try
            {
                uint read = m_nativeRecv.Read(type, buff, (uint)BufferSize, bReadAll, (uint)msTimeout);
                values = new object[read];
                for (uint idx = 0; idx < read; idx++)
                {
                    IntPtr here = new IntPtr(buff.ToInt64() + idx * elemSize);
                    values[idx] = m_recvTypeInfo.fromNative(here);
                }
            }
            finally
            {
                Marshal.FreeHGlobal(buff);
            }
        }
    };

    class CppProxyOutputFunction : CppProxyOutEndpoint, signals.IOutputFunction
	{
        private Native.IEPSender m_nativeSend;
        private IntPtr m_nativeSendRef;
        private signals.EType m_sendType = signals.EType.None;
        private ProxyTypes.ITypeMarshaller m_sendTypeInfo = null;

        public CppProxyOutputFunction(signals.IModule module, ICollectible owner, IntPtr native)
            :base(module, owner, native)
        {
            m_nativeSendRef = new IntPtr(native.ToInt64() + IntPtr.Size);
            m_nativeSend = (Native.IEPSender)CppNativeProxy.CreateCallout(m_nativeSendRef, typeof(Native.IEPSender));
        }

        ~CppProxyOutputFunction()
        {
            Dispose(false);
        }

        override public void Dispose()
        {
            Dispose(true);
            GC.SuppressFinalize(this);
        }

        protected virtual void Dispose(bool disposing)
        {
            if (m_nativeSend != null)
            {
                m_nativeSend.Release(IntPtr.Zero);
                m_nativeSend = null;
            }
        }

        public IntPtr NativeSender { get { return m_nativeSendRef; } }

        public int Write(signals.EType type, object[] values, int msTimeout)
        {
            if (values == null) throw new ArgumentNullException("values");
            if (type != m_sendType || m_sendTypeInfo == null)
            {
                m_sendType = type;
                m_sendTypeInfo = ProxyTypes.getTypeInfo(type);
            }
            if (m_sendTypeInfo == null) throw new NotSupportedException("Cannot retrieve value for this type.");
            int elemSize = m_sendTypeInfo.size(null);
            IntPtr buff = Marshal.AllocHGlobal(elemSize * values.Length);
            try
            {
                for (int idx = 0; idx < values.Length; idx++)
                {
                    IntPtr here = new IntPtr(buff.ToInt64() + idx * elemSize);
                    m_sendTypeInfo.toNative(values[idx], here);
                }
                return (int)m_nativeSend.Write(type, buff, (uint)values.Length, (uint)msTimeout);
            }
            finally
            {
                Marshal.FreeHGlobal(buff);
            }
        }
    };
}
