﻿using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.IO;

namespace cppProxy
{
    static class Discover
    {
        [DllImport("kernel32.dll", SetLastError = true, CharSet = CharSet.Auto)]
        private static extern IntPtr LoadLibrary(string dllToLoad);

        [DllImport("kernel32.dll", CharSet = CharSet.Ansi, ExactSpelling = true, SetLastError = true)]
        private static extern IntPtr GetProcAddress(IntPtr hModule, string procedureName);

        [DllImport("kernel32.dll", SetLastError = true)][return: MarshalAs(UnmanagedType.Bool)]
        private static extern bool FreeLibrary(IntPtr hModule);

        private class CppModule : CppProxyBlockDriver
        {
            private IntPtr m_hModule;

            public CppModule(IntPtr hModule, IntPtr native)
                :base(native)
            {
                m_hModule = hModule;
            }

            ~CppModule()
            {
                if (m_hModule != IntPtr.Zero)
                {
                    FreeLibrary(m_hModule);
                    m_hModule = IntPtr.Zero;
                }
            }
        };

        public static signals.IBlockDriver[] DoDiscovery(string path)
        {
            string[] dllList = Directory.GetFiles(path, "*.dll", SearchOption.TopDirectoryOnly);
            List<signals.IBlockDriver> found = new List<signals.IBlockDriver>();
            for (int idx = 0; idx < dllList.Length; idx++)
            {
                IntPtr hModule = LoadLibrary(dllList[idx]);
                if (hModule != IntPtr.Zero)
                {
                    try
                    {
                        IntPtr driverAddr = GetProcAddress(hModule, "QueryDrivers");
                        found.Add(new CppModule(hModule, driverAddr));
                        hModule = IntPtr.Zero;
                    }
                    finally
                    {
                        if (hModule != IntPtr.Zero) FreeLibrary(hModule);
                    }
                }
            }
            signals.IBlockDriver[] outList = new signals.IBlockDriver[found.Count];
            found.CopyTo(outList);
            return outList;
        }
    }

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
            if (strz == IntPtr.Zero) throw new ArgumentNullException("strz");
            int strlen = getStringLength(strz);
            byte[] strArray = new byte[strlen+1];
            Marshal.Copy(strz, strArray, 0, strlen+1);
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
            public int size(object val) { return sizeof(float); }
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
            public int size(object val) { return sizeof(double); }
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
            }
        };

        public static ITypeMarshaller getTypeInfo(signals.EType typ)
        {
            switch (typ)
            {
                case signals.EType.etypString:
                    return new String();
                case signals.EType.etypBoolean:
                    return new Boolean();
                case signals.EType.etypByte:
                    return new Byte();
                case signals.EType.etypShort:
                    return new Short();
                case signals.EType.etypLong:
                    return new Long();
                case signals.EType.etypSingle:
                    return new Single();
                case signals.EType.etypDouble:
                    return new Double();
                case signals.EType.etypComplex:
                case signals.EType.etypLRSingle:
                    return new Complex();
                case signals.EType.etypCmplDbl:
                    return new ComplexDouble();
                default:
                    return null;
            }
        }
    }

    class CppProxyBlockDriver : signals.IBlockDriver
    {
        private interface ICppBlockDriverInterface
        {
            IntPtr Name();
            bool canCreate();
            bool canDiscover();
            uint Discover(IntPtr blocks, uint availBlocks);
            IntPtr Create();
        };

        private ICppBlockDriverInterface m_native;
        private IntPtr m_nativeRef;
        private bool m_canCreate;
        private bool m_canDiscover;
        private string m_name;
        public const int DEFAULT_BUFFER = 100;
        public int BufferSize = DEFAULT_BUFFER;

        public CppProxyBlockDriver(IntPtr native)
        {
            if (native == IntPtr.Zero) throw new ArgumentNullException("native");
            m_nativeRef = native;
            Registration.storeObject(native, this);
            m_native = (ICppBlockDriverInterface)Marshal.PtrToStructure(native, typeof(ICppBlockDriverInterface));
            interrogate();
        }

        private void interrogate()
        {
            m_name = Utilities.getString(m_native.Name());
            m_canCreate = m_native.canCreate();
            m_canDiscover = m_native.canDiscover();
        }

        ~CppProxyBlockDriver()
        {
            if (m_nativeRef != IntPtr.Zero)
            {
                Registration.removeObject(m_nativeRef);
                m_nativeRef = IntPtr.Zero;
            }
        }

        public string Name { get { return m_name; } }
        public bool canCreate { get { return m_canCreate; } }
        public bool canDiscover { get { return m_canDiscover; } }

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

    class CppProxyBlock : signals.IBlock
    {
        private interface ICppBlockInterface
        {
            uint AddRef();
            uint Release();
            IntPtr Name();
            IntPtr Driver();
            IntPtr Parent();
            uint Children(IntPtr blocks, uint availBlocks);
            uint Incoming(IntPtr ep, uint availEP);
            uint Outgoing(IntPtr ep, uint availEP);
            IntPtr Attributes();
            void Start();
            void Stop();
        };

        private ICppBlockInterface m_native;
        private IntPtr m_nativeRef;
        private readonly signals.IBlockDriver m_driver;
        private readonly signals.IBlock m_parent;
        private string m_name;
        private signals.IAttributes m_attrs = null;
        private signals.IBlock[] m_children = null;
        private signals.IInEndpoint[] m_incoming = null;
        private signals.IOutEndpoint[] m_outgoing = null;

        public CppProxyBlock(signals.IBlockDriver driver, signals.IBlock parent, IntPtr native)
        {
            if (driver == null) throw new ArgumentNullException("driver");
            if (native == IntPtr.Zero) throw new ArgumentNullException("native");
            m_nativeRef = native;
            m_driver = driver;
            m_parent = parent;
            Registration.storeObject(native, this);
            m_native = (ICppBlockInterface)Marshal.PtrToStructure(native, typeof(ICppBlockInterface));
            interrogate();
        }

        ~CppProxyBlock()
        {
            Dispose(false);
        }

        private void interrogate()
        {
            m_name = Utilities.getString(m_native.Name());
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
                ICppBlockInterface localNative = m_native;
                m_native = null;
                localNative.Release();
            }
            if (m_nativeRef != IntPtr.Zero)
            {
                Registration.removeObject(m_nativeRef);
                m_nativeRef = IntPtr.Zero;
            }
        }

        public string Name { get { return m_name; } }
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
                    if (m_attrs == null) m_attrs = new CppProxyAttributes(m_driver, this, attrs);
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
                                if (newObj == null) newObj = new CppProxyInEndpoint(m_driver, this, ptrArr[idx]);
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
                                if (newObj == null) newObj = new CppProxyOutEndpoint(m_driver, this, ptrArr[idx]);
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
    }

    class CppProxyAttributes : signals.IAttributes
    {
        private interface ICppAttributesInterface
        {
            uint Itemize(IntPtr attrs, uint availElem);
            IntPtr GetByName(IntPtr name);
            void Observe(IntPtr obs);
            void Unobserve(IntPtr obs);
            IntPtr Block();
        };

        private ICppAttributesInterface m_native;
        private IntPtr m_nativeRef;
        private readonly signals.IBlock m_block;
        private readonly signals.IBlockDriver m_driver;

        public CppProxyAttributes(signals.IBlockDriver driver, signals.IBlock block, IntPtr native)
        {
            if (driver == null) throw new ArgumentNullException("driver");
            if (native == IntPtr.Zero) throw new ArgumentNullException("native");
            m_driver = driver;
            m_block = block;
            m_nativeRef = native;
            Registration.storeObject(native, this);
            m_native = (ICppAttributesInterface)Marshal.PtrToStructure(native, typeof(ICppAttributesInterface));
        }

        ~CppProxyAttributes()
        {
            if (m_nativeRef != IntPtr.Zero)
            {
                Registration.removeObject(m_nativeRef);
                m_nativeRef = IntPtr.Zero;
            }
        }

        public signals.IBlock Block { get { return m_block; } }

        public signals.IAttribute GetByName(string name)
        {
            if (name == null) throw new ArgumentNullException("name");
            byte[] strName = System.Text.Encoding.UTF8.GetBytes(name);
            IntPtr nameBuff = Marshal.AllocHGlobal(strName.Length);
            try
            {
                Marshal.Copy(strName, 0, nameBuff, strName.Length);
                IntPtr attrRef = m_native.GetByName(nameBuff);
                if (attrRef == IntPtr.Zero) return null;
                signals.IAttribute newObj = (signals.IAttribute)Registration.retrieveObject(attrRef);
                if (newObj == null) newObj = new CppProxyAttribute(m_driver, attrRef);
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
                        if (newObj == null) newObj = new CppProxyAttribute(m_driver, ptrArr[idx]);
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
        private interface ICppAttributeInterface
	    {
		    IntPtr Name();
		    IntPtr Description();
		    signals.EType Type();
		    void Observe(IntPtr obs);
		    void Unobserve(IntPtr obs);
		    bool isReadOnly();
		    IntPtr getValue();
		    bool setValue(IntPtr newVal);
		    uint options(IntPtr values, IntPtr opts, uint availElem);
	    };

        [UnmanagedFunctionPointer(CallingConvention.StdCall)]
        private delegate void ChangedCatcherDelegate(IntPtr nativeThis, IntPtr name, signals.EType type, IntPtr value);

        private class ChangedCatcher
        {
            private readonly CppProxyAttribute parent;
            public ChangedCatcherDelegate myDelegate;

            public ChangedCatcher(CppProxyAttribute parent)
            {
                this.parent = parent;
                this.myDelegate = new ChangedCatcherDelegate(OnChanged);
            }

            public void OnChanged(IntPtr nativeThis, IntPtr name, signals.EType type, IntPtr value)
            {
                string strName = Utilities.getString(name);
                object newVal = null;
                if (value != IntPtr.Zero)
                {
                    if (parent.m_typeInfo != null)
                    {
                        newVal = parent.m_typeInfo.fromNative(value);
                    }
                    else if (type == signals.EType.etypString)
                    {
                        newVal = Utilities.getString(value);
                    }
                }
                parent.changed(strName, type, newVal);
            }
        }

        public event signals.OnChanged changed;
        private readonly signals.IBlockDriver m_driver;
        private ICppAttributeInterface m_native;
        private IntPtr m_nativeRef;
        private string m_name;
        private string m_descr;
        private signals.EType m_type;
        private ProxyTypes.ITypeMarshaller m_typeInfo;
        private bool m_readOnly;
        private ChangedCatcher m_catcher;
        private IntPtr m_catcherIface;

        public CppProxyAttribute(signals.IBlockDriver driver, IntPtr native)
        {
            if (driver == null) throw new ArgumentNullException("driver");
            if (native == IntPtr.Zero) throw new ArgumentNullException("native");
            m_driver = driver;
            m_nativeRef = native;
            Registration.storeObject(native, this);
            m_native = (ICppAttributeInterface)Marshal.PtrToStructure(native, typeof(ICppAttributeInterface));
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
            if (m_catcherIface != IntPtr.Zero)
            {
                m_native.Unobserve(m_catcherIface);
                Marshal.FreeHGlobal(m_catcherIface);
                m_catcherIface = IntPtr.Zero;
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
            m_catcherIface = buildCatcherIface();
            m_native.Observe(m_catcherIface);
        }

        private IntPtr buildCatcherIface()
        { // technique from http://code4k.blogspot.com/2010/10/implementing-unmanaged-c-interface.html
            IntPtr iface = Marshal.AllocHGlobal(IntPtr.Size * 2);
            IntPtr vtblPtr = new IntPtr(iface.ToInt64() + IntPtr.Size);   // Write pointer to vtbl
            Marshal.WriteIntPtr(iface, vtblPtr);
            Marshal.WriteIntPtr(vtblPtr, Marshal.GetFunctionPointerForDelegate(m_catcher.myDelegate));
            return iface;
        }

        public string Name { get { return m_name; } }
        public string Description { get { return m_descr; } }
        public signals.EType Type { get { return m_type; } }
        public bool isReadOnly { get { return m_readOnly; } }

        public object Value
        {
            get
            {
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
        private interface IInEndpoint
	    {
		    uint AddRef();
		    uint Release();
		    IntPtr Block();
		    IntPtr EPName();
		    signals.EType Type();
		    IntPtr Attributes();
		    bool Connect(IntPtr recv);
		    bool isConnected();
		    bool Disconnect();
		    IntPtr CreateBuffer();
	    }

        private IInEndpoint m_native;
        private readonly signals.IBlockDriver m_driver;
        private readonly signals.IBlock m_block;
        private IntPtr m_nativeRef;
        private string m_name;
        private signals.EType m_type;
        private signals.IAttributes m_attrs = null;

        public CppProxyInEndpoint(signals.IBlockDriver driver, signals.IBlock block, IntPtr native)
        {
            if (driver == null) throw new ArgumentNullException("driver");
            if (block == null) throw new ArgumentNullException("block");
            if (native == IntPtr.Zero) throw new ArgumentNullException("native");
            m_driver = driver;
            m_nativeRef = native;
            m_block = block;
            Registration.storeObject(native, this);
            m_native = (IInEndpoint)Marshal.PtrToStructure(native, typeof(IInEndpoint));
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
                    if (m_attrs == null) m_attrs = new CppProxyAttributes(m_driver, m_block, attrs);
                }
                return m_attrs;
            }
        }

        public signals.IBlock Block { get { return m_block; } }
        public string EPName { get { return m_name; } }
        public signals.EType Type { get { return m_type; } }
        public bool isConnected { get { return m_native.isConnected(); } }
        public bool Disconnect() { return m_native.Disconnect(); }

        public signals.IEPBuffer CreateBuffer()
        {
            IntPtr pBuf = m_native.CreateBuffer();
            if (pBuf == IntPtr.Zero) return null;
            signals.IEPBuffer buff = (signals.IEPBuffer)Registration.retrieveObject(pBuf);
            if (m_attrs == null) buff = new CppProxyBuffer(m_driver, pBuf);
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
                    return m_native.Connect(proxy.Native);
                }
            }
            throw new NotSupportedException("Native connections not yet implemented.");
        }
    }

    class CppProxyOutEndpoint : signals.IOutEndpoint
    {
	    private interface IOutEndpoint
	    {
		    IntPtr Block();
		    IntPtr EPName();
		    signals.EType Type();
		    IntPtr Attributes();
		    bool Connect(IntPtr send);
		    bool isConnected();
		    bool Disconnect();
		    IntPtr CreateBuffer();
	    };

        private IOutEndpoint m_native;
        private readonly signals.IBlockDriver m_driver;
        private readonly signals.IBlock m_block;
        private IntPtr m_nativeRef;
        private string m_name;
        private signals.EType m_type;
        private signals.IAttributes m_attrs = null;

        public CppProxyOutEndpoint(signals.IBlockDriver driver, signals.IBlock block, IntPtr native)
        {
            if (driver == null) throw new ArgumentNullException("driver");
            if (block == null) throw new ArgumentNullException("block");
            if (native == IntPtr.Zero) throw new ArgumentNullException("native");
            m_driver = driver;
            m_nativeRef = native;
            m_block = block;
            Registration.storeObject(native, this);
            m_native = (IOutEndpoint)Marshal.PtrToStructure(native, typeof(IOutEndpoint));
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
                    if (m_attrs == null) m_attrs = new CppProxyAttributes(m_driver, m_block, attrs);
                }
                return m_attrs;
            }
        }

        public void Dispose() { }
        public signals.IBlock Block { get { return m_block; } }
        public string EPName { get { return m_name; } }
        public signals.EType Type { get { return m_type; } }
        public bool isConnected { get { return m_native.isConnected(); } }
        public bool Disconnect() { return m_native.Disconnect(); }

        public signals.IEPBuffer CreateBuffer()
        {
            IntPtr pBuf = m_native.CreateBuffer();
            if (pBuf == IntPtr.Zero) return null;
            signals.IEPBuffer buff = (signals.IEPBuffer)Registration.retrieveObject(pBuf);
            if (m_attrs == null) buff = new CppProxyBuffer(m_driver, pBuf);
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
                    return m_native.Connect(proxy.Native);
                }
            }
            throw new NotSupportedException("Native connections not yet implemented.");
        }
    }

    class CppProxyEPSender : signals.IEPSender
    {
        private interface IEPSender
        {
            uint Write(signals.EType type, IntPtr buffer, uint numElem, uint msTimeout);
            uint AddRef();
            uint Release();
        };

        private readonly signals.IBlockDriver m_driver;
        private IEPSender m_native;
        private IntPtr m_nativeRef;
        private signals.EType m_type = signals.EType.etypNone;
        private ProxyTypes.ITypeMarshaller m_typeInfo = null;

        public CppProxyEPSender(signals.IBlockDriver driver, IntPtr native)
        {
            if (driver == null) throw new ArgumentNullException("driver");
            if (native == IntPtr.Zero) throw new ArgumentNullException("native");
            m_driver = driver;
            m_nativeRef = native;
            Registration.storeObject(native, this);
            m_native = (IEPSender)Marshal.PtrToStructure(native, typeof(IEPSender));
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
                m_native.Release();
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
        private interface IEPReceiver
        {       
            uint Read(signals.EType type, IntPtr buffer, uint numAvail, uint msTimeout);
            void onSinkConnected(IntPtr src);
            void onSinkDisconnected(IntPtr src);
        };

        private readonly signals.IBlockDriver m_driver;
        private IEPReceiver m_native;
        private IntPtr m_nativeRef;
        private signals.EType m_type = signals.EType.etypNone;
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
            m_native = (IEPReceiver)Marshal.PtrToStructure(native, typeof(IEPReceiver));
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

        public void Read(signals.EType type, out object[] values, int msTimeout)
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
                uint read = m_native.Read(type, buff, (uint)BufferSize, (uint)msTimeout);
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
        private interface IEPBuffer
        {
            uint Write(signals.EType type, IntPtr buffer, uint numElem, uint msTimeout);
            uint AddRef();
            uint Release();
            uint Read(signals.EType type, IntPtr buffer, uint numAvail, uint msTimeout);
            void onSinkConnected(IntPtr src);
            void onSinkDisconnected(IntPtr src);
            signals.EType Type();
            uint Capacity();
            uint Used();
        };

        private readonly signals.IBlockDriver m_driver;
        private IEPBuffer m_native;
        private IntPtr m_nativeRef;
        private signals.EType m_type;
        private ProxyTypes.ITypeMarshaller m_typeInfo = null;
        public const int DEFAULT_BUFFER = 100;
        public int BufferSize = DEFAULT_BUFFER;

        public CppProxyBuffer(signals.IBlockDriver driver, IntPtr native)
        {
            if (driver == null) throw new ArgumentNullException("driver");
            if (native == IntPtr.Zero) throw new ArgumentNullException("native");
            m_driver = driver;
            m_nativeRef = native;
            Registration.storeObject(native, this);
            m_native = (IEPBuffer)Marshal.PtrToStructure(native, typeof(IEPBuffer));
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
                m_native.Release();
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
        public IntPtr Native { get { return m_nativeRef; } }

        public void Read(signals.EType type, out object[] values, int msTimeout)
        {
            if (m_typeInfo == null) throw new NotSupportedException("Cannot retrieve value for this type.");
            int elemSize = m_typeInfo.size(null);
            IntPtr buff = Marshal.AllocHGlobal(elemSize * BufferSize);
            try
            {
                uint read = m_native.Read(m_type, buff, (uint)BufferSize, (uint)msTimeout);
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
}
