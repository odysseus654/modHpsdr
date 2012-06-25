using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;

namespace cppProxy
{
    static class Utilities
    {
        private static unsafe int getStringLength(IntPtr strz)
        {
            if(strz == IntPtr.Zero) return 0;
            int strlen = 0;
            for (byte* native = (byte*)strz.ToPointer(); *native != 0; native++, strlen++) ;
            return strlen;
        }

        public static string getString(IntPtr strz)
        {
            if(strz == IntPtr.Zero) return null;
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
            m_universe.Add(key, value);
        }
    }

    class CppProxyBlockDriver : signals.IBlockDriver
    {
        [StructLayout(LayoutKind.Sequential)]
        private interface ICppBlockDriverInterface
        {
            IntPtr Name();
            bool canCreate();
            bool canDiscover();
            uint Discover(IntPtr blocks, uint availBlocks);
            IntPtr Create();
        };

        private ICppBlockDriverInterface m_native;
        private bool m_canCreate;
        private bool m_canDiscover;
        private string m_name;

        public CppProxyBlockDriver(IntPtr native)
        {
            if (native == IntPtr.Zero)
            {
                m_native = null;
            } else { 
                m_native = (ICppBlockDriverInterface)Marshal.PtrToStructure(native, typeof(ICppBlockDriverInterface));
                interrogate();
            }
        }

        private void interrogate()
        {
            m_name = Utilities.getString(m_native.Name());
            m_canCreate = m_native.canCreate();
            m_canDiscover = m_native.canDiscover();
        }

        public string Name { get { return m_name; } }
        public bool canCreate { get { return m_canCreate; } }
        public bool canDiscover { get { return m_canDiscover; } }

        public signals.IBlock Create()
        {
            IntPtr newObjRef = m_native.Create();
            if (newObjRef == IntPtr.Zero) return null;
            signals.IBlock newObj = (signals.IBlock)Registration.retrieveObject(newObjRef);
            if (newObj == null)
            {
                newObj = new CppProxyBlock(this, null, newObjRef);
                Registration.storeObject(newObjRef, newObj);
            }
            return newObj;
        }

        public signals.IBlock[] Discover()
        {   // discovery can be expensive, so we just use a large buffer rather than call things multiple times
            const int BUF_SIZE = 100;
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
                        if (newObj == null)
                        {
                            newObj = new CppProxyBlock(this, null, ptrArr[idx]);
                            Registration.storeObject(ptrArr[idx], newObj);
                        }
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
        [StructLayout(LayoutKind.Sequential)]
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
        private readonly signals.IBlockDriver m_driver;
        private readonly signals.IBlock m_parent;
        private string m_name;
        private signals.IAttributes m_attrs = null;
        private signals.IBlock[] m_children = null;

        public CppProxyBlock(signals.IBlockDriver driver, signals.IBlock parent, IntPtr native)
        {
            m_driver = driver;
            m_parent = parent;
            if (native == IntPtr.Zero)
            {
                m_native = null;
            } else { 
                m_native = (ICppBlockInterface)Marshal.PtrToStructure(native, typeof(ICppBlockInterface));
                interrogate();
            }
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
                    if (m_attrs == null)
                    {
                        m_attrs = new CppProxyAttributes(this, attrs);
                        Registration.storeObject(attrs, m_attrs);
                    }
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
                        uint numObj = m_native.Children(childBuff, numChildren);
                        IntPtr[] ptrArr = new IntPtr[numObj];
                        Marshal.Copy(childBuff, ptrArr, 0, (int)numObj);
                        m_children = new CppProxyBlock[numObj];
                        for (int idx = 0; idx < numObj; idx++)
                        {

                            if (ptrArr[idx] != IntPtr.Zero)
                            {
                                signals.IBlock newObj = (signals.IBlock)Registration.retrieveObject(ptrArr[idx]);
                                if (newObj == null)
                                {
                                    newObj = new CppProxyBlock(m_driver, this, ptrArr[idx]);
                                    Registration.storeObject(ptrArr[idx], newObj);
                                }
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
//        signals.IInEndpoint[] Incoming { get; }
//        signals.IOutEndpoint[] Outgoing { get; }
    }

    class CppProxyAttributes : signals.IAttributes
    {
        [StructLayout(LayoutKind.Sequential)]
        private interface ICppAttributesInterface
        {
            uint Itemize(IntPtr attrs, uint availElem);
            IntPtr GetByName(IntPtr name);
            void Observe(IntPtr obs);
            void Unobserve(IntPtr obs);
            IntPtr Block();
        };

        private ICppAttributesInterface m_native;
        private readonly signals.IBlock m_block;

        public CppProxyAttributes(signals.IBlock block, IntPtr native)
        {
            m_block = block;
            if (native == IntPtr.Zero)
            {
                m_native = null;
            }
            else
            {
                m_native = (ICppAttributesInterface)Marshal.PtrToStructure(native, typeof(ICppAttributesInterface));
            }
        }

        public signals.IBlock Block { get { return m_block; } }

        public signals.IAttribute GetByName(string name)
        {
            byte[] strName = System.Text.Encoding.UTF8.GetBytes(name);
            IntPtr nameBuff = Marshal.AllocHGlobal(strName.Length);
            try
            {
                Marshal.Copy(strName, 0, nameBuff, strName.Length);
                IntPtr attrRef = m_native.GetByName(strName);
                if (attrRef == IntPtr.Zero) return null;
                signals.IAttribute newObj = (signals.IAttribute)Registration.retrieveObject(attrRef);
                if (newObj == null)
                {
                    newObj = new CppProxyAttribute(attrRef);
                    Registration.storeObject(attrRef, newObj);
                }
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
                        if (newObj == null)
                        {
                            newObj = new CppProxyAttribute(ptrArr[idx]);
                            Registration.storeObject(ptrArr[idx], newObj);
                        }
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
        [StructLayout(LayoutKind.Sequential)]
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

        private ICppAttributeInterface m_native;
        private string m_name;
        private string m_descr;
        private signals.EType m_type;
        private bool m_readOnly;

        public CppProxyAttribute(IntPtr native)
        {
            if (native == IntPtr.Zero)
            {
                m_native = null;
            }
            else
            {
                m_native = (ICppAttributeInterface)Marshal.PtrToStructure(native, typeof(ICppAttributeInterface));
            }
            interrogate();
        }

        private void interrogate()
        {
            m_name = Utilities.getString(m_native.Name());
            m_descr = Utilities.getString(m_native.Description());
            m_type = m_native.Type();
            m_readOnly = m_native.isReadOnly();
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
                switch (m_type)
                {
                    case signals.EType.etypBoolean:
                        return Marshal.ReadByte(rawVal) != 0;
                    case signals.EType.etypByte:
                        return Marshal.ReadByte(rawVal);
                    case signals.EType.etypShort:
                        return Marshal.ReadInt16(rawVal);
                    case signals.EType.etypLong:
                        return Marshal.ReadInt32(rawVal);
                    case signals.EType.etypSingle:
                        {
                            float[] buff = new float[1];
                            Marshal.Copy(rawVal, buff, 0, 1);
                            return buff[0];
                        }
                    case signals.EType.etypDouble:
                        {
                            double[] buff = new double[1];
                            Marshal.Copy(rawVal, buff, 0, 1);
                            return buff[0];
                        }
                    case signals.EType.etypComplex:
                        {
                            float[] buff = new float[2];
                            Marshal.Copy(rawVal, buff, 0, 2);
                            return new Complex(buff[0], buff[1]);
                        }
                    case signals.EType.etypCmplDbl:
                        {
                            double[] buff = new double[2];
                            Marshal.Copy(rawVal, buff, 0, 2);
                            return new Complex(buff[0], buff[1]);
                        }
                    case signals.EType.etypLRSingle:
                        {
                            float[] buff = new float[2];
                            Marshal.Copy(rawVal, buff, 0, 2);
                            return buff;
                        }
                    case signals.EType.etypString:
                        return Utilities.getString(rawVal);
                }
                return null;
            }
//            set;
        }

//        void options(out object[] values, out string[] opts);
//        event OnChanged changed;
    }
/*
	__interface IEPSender
	{
		unsigned Write(EType type, const void* buffer, unsigned numElem, unsigned msTimeout);
		unsigned AddRef();
		unsigned Release();
	};

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

	__interface IInEndpoint
	{
		unsigned AddRef();
		unsigned Release();
		IBlock* Block();
		const char* EPName();
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
		const char* EPName();
		EType Type();
		IAttributes* Attributes();
		bool Connect(IEPSender* send);
		bool isConnected();
		bool Disconnect();
		IEPBuffer* CreateBuffer();
	};

	__interface IAttributeObserver
	{
		void OnChanged(const char* name, EType type, const void* value);
	};
*/
}
