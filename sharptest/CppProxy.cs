using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;

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
            const int BUF_SIZE = 100;
            if (!m_canDiscover) throw new NotSupportedException("Driver cannot discover objects.");
            IntPtr discoverBuff = Marshal.AllocHGlobal(IntPtr.Size * BUF_SIZE);
            try
            {
                uint numObj = m_native.Discover(discoverBuff, BUF_SIZE);
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
                                if (newObj == null) newObj = new CppProxyInEndpoint(this, ptrArr[idx]);
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
                                if (newObj == null) newObj = new CppProxyOutEndpoint(this, ptrArr[idx]);
                                m_incoming[idx] = newObj;
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
        /*
                public signals.IOutEndpoint[] Outgoing
                {
                    get
                    {
                    }
                }
                */
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

        public CppProxyAttributes(signals.IBlock block, IntPtr native)
        {
            m_block = block;
            if (native == IntPtr.Zero) throw new ArgumentNullException("native");
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
                if (newObj == null) newObj = new CppProxyAttribute(attrRef);
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
                        if (newObj == null) newObj = new CppProxyAttribute(ptrArr[idx]);
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

        private interface ITypeMarshaller
        {
            int size();
            object fromNative(IntPtr val);
            void toNative(object src, IntPtr dest);
        };

        private class BooleanType : ITypeMarshaller
        {
            public int size() { return sizeof(byte); }
            public object fromNative(IntPtr val) { return Marshal.ReadByte(val) != 0; }
            public void toNative(object src, IntPtr dest) { Marshal.WriteByte(dest, (byte)((bool)src ? 1 : 0)); }
        };

        private class ByteType : ITypeMarshaller
        {
            public int size() { return sizeof(byte); }
            public object fromNative(IntPtr val) { return Marshal.ReadByte(val); }
            public void toNative(object src, IntPtr dest) { Marshal.WriteByte(dest, (byte)src); }
        };

        private class ShortType : ITypeMarshaller
        {
            public int size() { return sizeof(short); }
            public object fromNative(IntPtr val) { return Marshal.ReadInt16(val); }
            public void toNative(object src, IntPtr dest) { Marshal.WriteInt16(dest, (short)src); }
        };

        private class LongType : ITypeMarshaller
        {
            public int size() { return sizeof(int); }
            public object fromNative(IntPtr val) { return Marshal.ReadInt32(val); }
            public void toNative(object src, IntPtr dest) { Marshal.WriteInt32(dest, (int)src); }
        };

        private class SingleType : ITypeMarshaller
        {
            public int size() { return sizeof(float); }
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

        private class DoubleType : ITypeMarshaller
        {
            public int size() { return sizeof(double); }
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

        private class ComplexType : ITypeMarshaller
        {
            public int size() { return sizeof(float); }
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

        private class ComplexDoubleType : ITypeMarshaller
        {
            public int size() { return sizeof(double); }
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
        private ICppAttributeInterface m_native;
        private IntPtr m_nativeRef;
        private string m_name;
        private string m_descr;
        private signals.EType m_type;
        private ITypeMarshaller m_typeInfo;
        private bool m_readOnly;
        private ChangedCatcher m_catcher;
        private IntPtr m_catcherIface;

        public CppProxyAttribute(IntPtr native)
        {
            if (native == IntPtr.Zero) throw new ArgumentNullException("native");
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
            m_typeInfo = getTypeInfo(m_type);
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

        private static ITypeMarshaller getTypeInfo(signals.EType typ)
        {
            switch (typ)
            {
                case signals.EType.etypBoolean:
                    return new BooleanType();
                case signals.EType.etypByte:
                    return new ByteType();
                case signals.EType.etypShort:
                    return new ShortType();
                case signals.EType.etypLong:
                    return new LongType();
                case signals.EType.etypSingle:
                    return new SingleType();
                case signals.EType.etypDouble:
                    return new DoubleType();
                case signals.EType.etypComplex:
                case signals.EType.etypLRSingle:
                    return new ComplexType();
                case signals.EType.etypCmplDbl:
                    return new ComplexDoubleType();
                default:
                    return null;
            }
        }

        public string Name { get { return m_name; } }
        public string Description { get { return m_descr; } }
        public signals.EType Type { get { return m_type; } }
        public bool isReadOnly { get { return m_readOnly; } }

        public object Value
        {
            get
            {
                IntPtr rawVal = m_native.getValue();
                if (rawVal == IntPtr.Zero) return null;
                if (m_typeInfo != null)
                {
                    return m_typeInfo.fromNative(rawVal);
                }
                else if (m_type == signals.EType.etypString)
                {
                    return Utilities.getString(rawVal);
                }
                else
                {
                    throw new NotSupportedException("Cannot retrieve value for this type.");
                }
            }
            set
            {
                if (value == null) throw new ArgumentNullException("value");
                IntPtr buff = IntPtr.Zero;
                try
                {
                    if (m_typeInfo != null)
                    {
                        buff = Marshal.AllocHGlobal(m_typeInfo.size());
                        m_typeInfo.toNative(value, buff);
                    }
                    else if (m_type == signals.EType.etypString)
                    {
                        byte[] strBytes = System.Text.Encoding.UTF8.GetBytes((string)value);
                        buff = Marshal.AllocHGlobal(sizeof(byte) * (strBytes.Length + 1));
                        Marshal.Copy(strBytes, 0, buff, strBytes.Length);
                    }
                    else
                    {
                        throw new NotSupportedException("Cannot store value for this type.");
                    }
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
                        if (m_typeInfo != null)
                        {
                            values[idx] = m_typeInfo.fromNative(valArr[idx]);
                        }
                        else if (m_type == signals.EType.etypString)
                        {
                            values[idx] = Utilities.getString(valArr[idx]);
                        }
                        else
                        {
                            throw new NotSupportedException("Cannot retrieve value for this type.");
                        }
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
        private signals.IBlock m_block;
        private IntPtr m_nativeRef;
        private string m_name;
        private signals.EType m_type;
        private signals.IAttributes m_attrs = null;

        public CppProxyInEndpoint(signals.IBlock block, IntPtr native)
        {
            if (block == null) throw new ArgumentNullException("block");
            if (native == IntPtr.Zero) throw new ArgumentNullException("native");
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
                    if (m_attrs == null) m_attrs = new CppProxyAttributes(m_block, attrs);
                }
                return m_attrs;
            }
        }

        public signals.IBlock Block { get { return m_block; } }
        public string EPName { get { return m_name; } }
        public signals.EType Type { get { return m_type; } }
        public bool isConnected { get { return m_native.isConnected(); } }
        public bool Disconnect() { return m_native.Disconnect(); }
//        bool Connect(IEPReceiver recv);
//		  IEPBuffer CreateBuffer();
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
        private signals.IBlock m_block;
        private IntPtr m_nativeRef;
        private string m_name;
        private signals.EType m_type;
        private signals.IAttributes m_attrs = null;

        public CppProxyOutEndpoint(signals.IBlock block, IntPtr native)
        {
            if (block == null) throw new ArgumentNullException("block");
            if (native == IntPtr.Zero) throw new ArgumentNullException("native");
            m_nativeRef = native;
            m_block = block;
            Registration.storeObject(native, this);
            m_native = (IOutEndpoint)Marshal.PtrToStructure(native, typeof(IOutEndpoint));
            interrogate();
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
                    if (m_attrs == null) m_attrs = new CppProxyAttributes(m_block, attrs);
                }
                return m_attrs;
            }
        }

        public signals.IBlock Block { get { return m_block; } }
        public string EPName { get { return m_name; } }
        public signals.EType Type { get { return m_type; } }
        public bool isConnected { get { return m_native.isConnected(); } }
        public bool Disconnect() { return m_native.Disconnect(); }
//        bool Connect(IEPSender send);
//        IEPBuffer CreateBuffer();
    }

    class CppProxyEPSender : signals.IEPSender
    {
        private interface IEPSender
        {
            uint Write(signals.EType type, IntPtr buffer, uint numElem, uint msTimeout);
            uint AddRef();
            uint Release();
        };

        private IEPSender m_native;
        private IntPtr m_nativeRef;

        public CppProxyEPSender(IntPtr native)
        {
            if (native == IntPtr.Zero) throw new ArgumentNullException("native");
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

//        int Write(EType type, object[] buffer, int msTimeout);
    }
/*
        __interface IEPReceiver
        {       
            unsigned Read(EType type, void* buffer, unsigned numAvail, unsigned msTimeout);
            void onSinkConnected(IInEndpoint* src);
            void onSinkDisconnected(IInEndpoint* src);
        };

        __interface IEPBuffer : public IEPSender, public IEPReceiver
        {
            EType Type();
            unsigned Capacity();
            unsigned Used();
        };
    */
}
