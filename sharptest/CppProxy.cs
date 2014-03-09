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
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.IO;

namespace cppProxy
{
    class Registration
    {
        private static WeakValuedDictionary<IntPtr, object> m_universe = new WeakValuedDictionary<IntPtr, object>();

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
        #region Proxy type conversion
        public interface ITypeMarshaller
        {
            int size(object val);
            object fromNative(IntPtr val);
            void toNative(object src, IntPtr dest);
            Array makePinnableArray(int cnt);
            Array toPinnableArray(Array src);
            Array fromPinnableArray(Array pin, int cnt);
        };

        private class Handle : ITypeMarshaller
        {
            public int size(object val) { return IntPtr.Size; }
            public object fromNative(IntPtr val) { return Marshal.ReadIntPtr(val); }
            public void toNative(object src, IntPtr dest) { Marshal.WriteIntPtr(dest, (IntPtr)src); }
            public Array makePinnableArray(int cnt) { return new IntPtr[cnt]; }

            public Array toPinnableArray(Array src)
            {
                if (src is IntPtr[]) return src;
                int len = src.Length;
                IntPtr[] outBuff = new IntPtr[len];
                for (int idx = 0; idx < len; idx++)
                {
                    outBuff[idx] = (IntPtr)src.GetValue(idx);
                }
                return outBuff;
            }

            public Array fromPinnableArray(Array pin, int cnt)
            {
                if (cnt == pin.Length) return pin;
                IntPtr[] outBuff = new IntPtr[cnt];
                Array.Copy(pin, outBuff, cnt);
                return outBuff;
            }
        };

        private class Boolean : Byte
        {
            public new object fromNative(IntPtr val) { return Marshal.ReadByte(val) != 0; }
            public new void toNative(object src, IntPtr dest) { Marshal.WriteByte(dest, (byte)(Convert.ToBoolean(src) ? 1 : 0)); }

            public new Array toPinnableArray(Array src)
            {
                int len = src.Length;
                byte[] outBuff = new byte[len];
                for (int idx = 0; idx < len; idx++)
                {
                    outBuff[idx] = (byte)(Convert.ToBoolean(src.GetValue(idx)) ? 1 : 0);
                }
                return outBuff;
            }

            public new Array fromPinnableArray(Array pin, int cnt)
            {
                byte[] byteBuff = (byte[])pin;
                bool[] buff = new bool[cnt];
                for (int idx = 0; idx < cnt; idx++)
                {
                    buff[idx] = byteBuff[idx] != 0;
                }
                return buff;
            }
        };

        private class Byte : ITypeMarshaller
        {
            public int size(object val) { return sizeof(byte); }
            public object fromNative(IntPtr val) { return Marshal.ReadByte(val); }
            public void toNative(object src, IntPtr dest) { Marshal.WriteByte(dest, Convert.ToByte(src)); }
            public Array makePinnableArray(int cnt) { return new byte[cnt]; }

            public Array toPinnableArray(Array src)
            {
                if (src is byte[]) return src;
                int len = src.Length;
                byte[] outBuff = new byte[len];
                for (int idx = 0; idx < len; idx++)
                {
                    outBuff[idx] = Convert.ToByte(src.GetValue(idx));
                }
                return outBuff;
            }

            public Array fromPinnableArray(Array pin, int cnt)
            {
                if (cnt == pin.Length) return pin;
                byte[] outBuff = new byte[cnt];
                Array.Copy(pin, outBuff, cnt);
                return outBuff;
            }
        };

        private class Short : ITypeMarshaller
        {
            public int size(object val) { return sizeof(short); }
            public object fromNative(IntPtr val) { return Marshal.ReadInt16(val); }
            public void toNative(object src, IntPtr dest) { Marshal.WriteInt16(dest, Convert.ToInt16(src)); }
            public Array makePinnableArray(int cnt) { return new short[cnt]; }

            public Array toPinnableArray(Array src)
            {
                if (src is short[]) return src;
                int len = src.Length;
                short[] outBuff = new short[len];
                for (int idx = 0; idx < len; idx++)
                {
                    outBuff[idx] = Convert.ToInt16(src.GetValue(idx));
                }
                return outBuff;
            }

            public Array fromPinnableArray(Array pin, int cnt)
            {
                if (cnt == pin.Length) return pin;
                short[] outBuff = new short[cnt];
                Array.Copy(pin, outBuff, cnt);
                return outBuff;
            }
        };

        private class Long : ITypeMarshaller
        {
            public int size(object val) { return sizeof(int); }
            public object fromNative(IntPtr val) { return Marshal.ReadInt32(val); }
            public void toNative(object src, IntPtr dest) { Marshal.WriteInt32(dest, Convert.ToInt32(src)); }
            public Array makePinnableArray(int cnt) { return new int[cnt]; }

            public Array toPinnableArray(Array src)
            {
                if (src is int[]) return src;
                int len = src.Length;
                int[] outBuff = new int[len];
                for (int idx = 0; idx < len; idx++)
                {
                    outBuff[idx] = Convert.ToInt32(src.GetValue(idx));
                }
                return outBuff;
            }

            public Array fromPinnableArray(Array pin, int cnt)
            {
                if (cnt == pin.Length) return pin;
                int[] outBuff = new int[cnt];
                Array.Copy(pin, outBuff, cnt);
                return outBuff;
            }
        };

        private class Int64 : ITypeMarshaller
        {
            public int size(object val) { return sizeof(long); }
            public object fromNative(IntPtr val) { return Marshal.ReadInt64(val); }
            public void toNative(object src, IntPtr dest) { Marshal.WriteInt64(dest, Convert.ToInt64(src)); }
            public Array makePinnableArray(int cnt) { return new long[cnt]; }

            public Array toPinnableArray(Array src)
            {
                if (src is long[]) return src;
                int len = src.Length;
                long[] outBuff = new long[len];
                for (int idx = 0; idx < len; idx++)
                {
                    outBuff[idx] = Convert.ToInt64(src.GetValue(idx));
                }
                return outBuff;
            }

            public Array fromPinnableArray(Array pin, int cnt)
            {
                if (cnt == pin.Length) return pin;
                long[] outBuff = new long[cnt];
                Array.Copy(pin, outBuff, cnt);
                return outBuff;
            }
        };

        private class Single : ITypeMarshaller
        {
            public int size(object val) { return sizeof(float); }
            public Array makePinnableArray(int cnt) { return new float[cnt]; }

            public void toNative(object src, IntPtr dest)
            {
                Marshal.Copy(new float[] { Convert.ToSingle(src) }, 0, dest, 1);
            }

            public object fromNative(IntPtr val)
            {
                float[] buff = new float[1];
                Marshal.Copy(val, buff, 0, 1);
                return buff[0];
            }

            public Array toPinnableArray(Array src)
            {
                if (src is float[]) return src;
                int len = src.Length;
                float[] outBuff = new float[len];
                for (int idx = 0; idx < len; idx++)
                {
                    outBuff[idx] = Convert.ToSingle(src.GetValue(idx));
                }
                return outBuff;
            }

            public Array fromPinnableArray(Array pin, int cnt)
            {
                if (cnt == pin.Length) return pin;
                float[] outBuff = new float[cnt];
                Array.Copy(pin, outBuff, cnt);
                return outBuff;
            }
        };

        private class Double : ITypeMarshaller
        {
            public int size(object val) { return sizeof(double); }
            public Array makePinnableArray(int cnt) { return new double[cnt]; }

            public void toNative(object src, IntPtr dest)
            {
                Marshal.Copy(new double[] { Convert.ToDouble(src) }, 0, dest, 1);
            }

            public object fromNative(IntPtr val)
            {
                double[] buff = new double[1];
                Marshal.Copy(val, buff, 0, 1);
                return buff[0];
            }

            public Array toPinnableArray(Array src)
            {
                if (src is double[]) return src;
                int len = src.Length;
                double[] outBuff = new double[len];
                for (int idx = 0; idx < len; idx++)
                {
                    outBuff[idx] = Convert.ToDouble(src.GetValue(idx));
                }
                return outBuff;
            }

            public Array fromPinnableArray(Array pin, int cnt)
            {
                if (cnt == pin.Length) return pin;
                double[] outBuff = new double[cnt];
                Array.Copy(pin, outBuff, cnt);
                return outBuff;
            }
        };

        private class Complex : ITypeMarshaller
        {
            public int size(object val) { return 2*sizeof(float); }
            public Array makePinnableArray(int cnt) { return new float[cnt * 2]; }

            public object fromNative(IntPtr val)
            {
                float[] buff = new float[2];
                Marshal.Copy(val, buff, 0, 2);
                return buff;
            }
            public void toNative(object src, IntPtr dest)
            {
                float[] val = new float[2] {
                    Convert.ToSingle(((Array)src).GetValue(0)),
                    Convert.ToSingle(((Array)src).GetValue(1))
                };
                Marshal.Copy(val, 0, dest, 2);
            }

            public Array toPinnableArray(Array src)
            {
                int len = src.Length;
                float[] outBuff = new float[len * 2];
                for (int inRef = 0, outRef = 0; inRef < len; inRef++, outRef += 2)
                {
                    Array inVal = (Array)src.GetValue(inRef);
                    outBuff[outRef] = Convert.ToSingle(inVal.GetValue(0));
                    outBuff[outRef + 1] = Convert.ToSingle(inVal.GetValue(1));
                }
                return outBuff;
            }

            public Array fromPinnableArray(Array pin, int cnt)
            {
                if (cnt == 0) return new Array[0];
                float[] nativeBuff = (float[])pin;
                Array[] outBuff = new Array[cnt];
                for (int inRef = 0, outRef = 0; outRef < cnt; inRef += 2, outRef++)
                {
                    outBuff[outRef] = new float[2] { nativeBuff[inRef], nativeBuff[inRef + 1] };
                }
                return outBuff;
            }
        };

        private class ComplexDouble : ITypeMarshaller
        {
            public int size(object val) { return 2*sizeof(double); }
            public Array makePinnableArray(int cnt) { return new double[cnt * 2]; }

            public object fromNative(IntPtr val)
            {
                double[] buff = new double[2];
                Marshal.Copy(val, buff, 0, 2);
                return buff;
            }
            public void toNative(object src, IntPtr dest)
            {
                double[] val = new double[2] {
                    Convert.ToDouble(((Array)src).GetValue(0)),
                    Convert.ToDouble(((Array)src).GetValue(1))
                };
                Marshal.Copy(val, 0, dest, 2);
            }

            public Array toPinnableArray(Array src)
            {
                int len = src.Length;
                double[] outBuff = new double[len * 2];
                for (int inRef = 0, outRef = 0; inRef < len; inRef++, outRef += 2)
                {
                    Array inVal = (Array)src.GetValue(inRef);
                    outBuff[outRef] = Convert.ToDouble(inVal.GetValue(0));
                    outBuff[outRef + 1] = Convert.ToDouble(inVal.GetValue(1));
                }
                return outBuff;
            }

            public Array fromPinnableArray(Array pin, int cnt)
            {
                if (cnt == 0) return new Array[0];
                double[] nativeBuff = (double[])pin;
                Array[] outBuff = new Array[cnt];
                for (int inRef = 0, outRef = 0; outRef < cnt; inRef += 2, outRef++)
                {
                    outBuff[outRef] = new double[2] { nativeBuff[inRef], nativeBuff[inRef + 1] };
                }
                return outBuff;
            }
        };

        private class String : ITypeMarshaller
        {
            public Array makePinnableArray(int cnt) { throw new NotSupportedException("String arrays are not supported."); }
            public Array toPinnableArray(Array src) { throw new NotSupportedException("String arrays are not supported."); }
            public Array fromPinnableArray(Array pin, int cnt) { throw new NotSupportedException("String arrays are not supported."); }

            public int size(object val)
            {
                return sizeof(byte) * (System.Text.Encoding.UTF8.GetByteCount(Convert.ToString(val)) + 1);
            }
            public object fromNative(IntPtr val)
            {
                return Utilities.getString(val);
            }
            public void toNative(object src, IntPtr dest)
            {
                byte[] strBytes = System.Text.Encoding.UTF8.GetBytes(Convert.ToString(src));
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
                case signals.EType.WinHdl:
                    return new Handle();
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
                case signals.EType.Int64:
                case signals.EType.VecInt64:
                    return new Int64();
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
        #endregion
    }

    public static class Native
    {
        public interface IBlockDriver // holds a ref to IModule
        {
            IntPtr Name();
            IntPtr Description();
            [return:MarshalAs(UnmanagedType.Bool)]bool canCreate();
            [return:MarshalAs(UnmanagedType.Bool)]bool canDiscover();
            uint Discover(IntPtr blocks, uint availBlocks);
            IntPtr Create();
            IntPtr Fingerprint();
        };

        public interface IBlock // refcounted, also holds a ref to IBlockDriver
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

        public interface IAttributes // no references?!?
        {
            uint Itemize(IntPtr attrs, uint availElem, uint flags);
            IntPtr GetByName(IntPtr name);
            void Observe(IntPtr obs);
            void Unobserve(IntPtr obs);
        };

        public interface IAttribute // no references?!?
        {
            IntPtr Name();
            IntPtr Description();
            [return: MarshalAs(UnmanagedType.I4)] signals.EType Type();
            void Observe(IntPtr obs);
            void Unobserve(IntPtr obs);
            [return: MarshalAs(UnmanagedType.Bool)] bool isReadOnly();
            IntPtr getValue();
            [return: MarshalAs(UnmanagedType.Bool)] bool setValue(IntPtr newVal);
            uint options(IntPtr values, IntPtr opts, uint availElem);
        };

        public interface IInEndpoint // refcounted
        {
            uint AddRef();
            uint Release();
            IntPtr EPName();
            IntPtr EPDescr();
            [return: MarshalAs(UnmanagedType.I4)] signals.EType Type();
            IntPtr Attributes();
            [return: MarshalAs(UnmanagedType.Bool)] bool Connect(IntPtr recv);
            [return: MarshalAs(UnmanagedType.Bool)] bool isConnected();
            [return: MarshalAs(UnmanagedType.Bool)] bool Disconnect();
        }

        public interface IOutEndpoint // reference to IModule
        {
            IntPtr EPName();
            IntPtr EPDescr();
            [return: MarshalAs(UnmanagedType.I4)] signals.EType Type();
            IntPtr Attributes();
            [return: MarshalAs(UnmanagedType.Bool)] bool Connect(IntPtr send);
            [return: MarshalAs(UnmanagedType.Bool)] bool isConnected();
            [return: MarshalAs(UnmanagedType.Bool)] bool Disconnect();
            IntPtr CreateBuffer();
        };

        public interface IEPSendTo // refcounted
        {
            uint Write([MarshalAs(UnmanagedType.I4)] signals.EType type, IntPtr buffer, uint numElem, uint msTimeout);
            uint AddRef(IntPtr iep);
            uint Release(IntPtr iep);
            IntPtr InputAttributes();
        };

        public interface IEPRecvFrom // reference to IBlockDriver
        {
            uint Read([MarshalAs(UnmanagedType.I4)] signals.EType type, IntPtr buffer, uint numAvail,
                [MarshalAs(UnmanagedType.Bool)]bool bReadAll, uint msTimeout);
            void onSinkConnected(IntPtr src);
            void onSinkDisconnected(IntPtr src);
            IntPtr OutputAttributes();
            IntPtr CreateBuffer();
        };

        public interface IEPBuffer : IEPSendTo, IEPRecvFrom // refcounted, reference to IModule
        {
            [return: MarshalAs(UnmanagedType.I4)] signals.EType Type();
            uint Capacity();
            uint Used();
        };

        public interface IAttributeObserver
        {
            void OnChanged(IntPtr attr, IntPtr value);
            void OnDetached(IntPtr attr);
        };

        public interface IFunctionSpec // reference to IModule
	    {
		    IntPtr Name();
		    IntPtr Description();
		    IntPtr Create();
            IntPtr Fingerprint();
        };

	    public interface IFunction // refcounted, reference to IFunctionSpec
	    {
		    IntPtr Spec();
		    uint AddRef();
		    uint Release();
		    IntPtr Input();
		    IntPtr Output();
	    };

        public interface IInputFunction : IInEndpoint, IEPRecvFrom // CppProxyInEndpoint, reference to IModule
	    {
	    };

	    public interface IOutputFunction : IOutEndpoint, IEPSendTo // refcounted, reference to IModule
	    {
	    };
    }

    class CppProxyModuleDriver : System.Runtime.ConstrainedExecution.CriticalFinalizerObject, signals.IModule
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
                    if (numDrivers == 0)
                    {
                        m_drivers = new CppProxyBlockDriver[0];
                    }
                    else
                    {
                        IntPtr[] ptrArr = new IntPtr[numDrivers];
                        GCHandle pin = GCHandle.Alloc(ptrArr, GCHandleType.Pinned);
                        try
                        {
                            queryDrivers.DynamicInvoke(new object[] { pin.AddrOfPinnedObject(), numDrivers });
                        }
                        finally
                        {
                            pin.Free();
                        }
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
                    if (numFunctions == 0)
                    {
                        m_functions = new CppProxyFunctionSpec[0];
                    }
                    else
                    {
                        IntPtr[] ptrArr = new IntPtr[numFunctions];
                        GCHandle pin = GCHandle.Alloc(ptrArr, GCHandleType.Pinned);
                        try
                        {
                            queryFunctions.DynamicInvoke(new object[] { pin.AddrOfPinnedObject(), numFunctions });
                        }
                        finally
                        {
                            pin.Free();
                        }
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
            IntPtr[] ptrArr = new IntPtr[BufferSize];
            GCHandle pin = GCHandle.Alloc(ptrArr, GCHandleType.Pinned);
            uint numObj;
            try
            {
                numObj = m_native.Discover(pin.AddrOfPinnedObject(), (uint)BufferSize);
            }
            finally
            {
                pin.Free();
            }
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
    };

    class CppProxyBlock : signals.IBlock
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
                    if (m_attrs == null) m_attrs = new CppProxyAttributes(attrs);
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
                    if (numChildren == 0)
                    {
                        m_children = new CppProxyBlock[0];
                    }
                    else
                    {
                        IntPtr[] ptrArr = new IntPtr[numChildren];
                        GCHandle pin = GCHandle.Alloc(ptrArr, GCHandleType.Pinned);
                        try
                        {
                            m_native.Children(pin.AddrOfPinnedObject(), numChildren);
                        }
                        finally
                        {
                            pin.Free();
                        }
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
                    if (numEP == 0)
                    {
                        m_incoming = new CppProxyInEndpoint[0];
                    }
                    else
                    {
                        IntPtr[] ptrArr = new IntPtr[numEP];
                        GCHandle pin = GCHandle.Alloc(ptrArr, GCHandleType.Pinned);
                        try
                        {
                            m_native.Incoming(pin.AddrOfPinnedObject(), numEP);
                        }
                        finally
                        {
                            pin.Free();
                        }
                        m_incoming = new CppProxyInEndpoint[numEP];
                        for (int idx = 0; idx < numEP; idx++)
                        {

                            if (ptrArr[idx] != IntPtr.Zero)
                            {
                                signals.IInEndpoint newObj = (signals.IInEndpoint)Registration.retrieveObject(ptrArr[idx]);
                                if (newObj == null) newObj = new CppProxyInEndpoint(ptrArr[idx]);
                                m_incoming[idx] = newObj;
                            }
                        }
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
                    if (numEP == 0)
                    {
                        m_outgoing = new CppProxyOutEndpoint[0];
                    }
                    else
                    {
                        IntPtr[] ptrArr = new IntPtr[numEP];
                        GCHandle pin = GCHandle.Alloc(ptrArr, GCHandleType.Pinned);
                        try
                        {
                            m_native.Outgoing(pin.AddrOfPinnedObject(), numEP);
                        }
                        finally
                        {
                            pin.Free();
                        }
                        m_outgoing = new CppProxyOutEndpoint[numEP];
                        for (int idx = 0; idx < numEP; idx++)
                        {

                            if (ptrArr[idx] != IntPtr.Zero)
                            {
                                signals.IOutEndpoint newObj = (signals.IOutEndpoint)Registration.retrieveObject(ptrArr[idx]);
                                if (newObj == null) newObj = new CppProxyOutEndpoint(m_driver.Module, ptrArr[idx]);
                                m_outgoing[idx] = newObj;
                            }
                        }
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

        public CppProxyAttributes(IntPtr native)
        {
            if (native == IntPtr.Zero) throw new ArgumentNullException("native");
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

        public signals.IAttribute this[string name]
        {
            get
            {
                if (name == null) throw new ArgumentNullException("name");
                byte[] strName = System.Text.Encoding.UTF8.GetBytes(name);
                IntPtr nameBuff = Marshal.AllocHGlobal(strName.Length + 1);
                try
                {
                    Marshal.Copy(strName, 0, nameBuff, strName.Length);
                    Marshal.WriteByte(nameBuff, strName.Length, 0);
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
        }

        public IEnumerator<signals.IAttribute> GetEnumerator()
        {
            uint numAttr = m_native.Itemize(IntPtr.Zero, 0, 0);
            if (numAttr == 0) return ((IList<signals.IAttribute>)new signals.IAttribute[0]).GetEnumerator();
            IntPtr[] ptrArr = new IntPtr[numAttr];
            GCHandle pin = GCHandle.Alloc(ptrArr, GCHandleType.Pinned);
            uint numObj;
            try
            {
                numObj = m_native.Itemize(pin.AddrOfPinnedObject(), numAttr, 0);
            }
            finally
            {
                pin.Free();
            }
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
            return ((IList<signals.IAttribute>)attrArray).GetEnumerator();
        }

        public IEnumerator<signals.IAttribute> Itemize(signals.EAttrEnumFlags flags)
        {
            uint numAttr = m_native.Itemize(IntPtr.Zero, 0, (uint)flags);
            if (numAttr == 0) return ((IList<signals.IAttribute>)new signals.IAttribute[0]).GetEnumerator();
            IntPtr[] ptrArr = new IntPtr[numAttr];
            GCHandle pin = GCHandle.Alloc(ptrArr, GCHandleType.Pinned);
            uint numObj;
            try
            {
                numObj = m_native.Itemize(pin.AddrOfPinnedObject(), numAttr, (uint)flags);
            }
            finally
            {
                pin.Free();
            }
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
            return ((IList<signals.IAttribute>)attrArray).GetEnumerator();
        }

        System.Collections.IEnumerator System.Collections.IEnumerable.GetEnumerator()
        {
            return GetEnumerator();
        }
    }

    class CppProxyAttribute : signals.IAttribute
    {
        private class ChangedCatcher : Native.IAttributeObserver
        {
            private readonly WeakReference parent;

            public ChangedCatcher(CppProxyAttribute parent)
            {
                this.parent = new WeakReference(parent);
            }

            public void OnChanged(IntPtr attr, IntPtr value)
            {
                CppProxyAttribute p = (CppProxyAttribute)parent.Target;
                if (p == null) return;
                object newVal = null;
                if (value != IntPtr.Zero && p.m_typeInfo != null)
                {
                    newVal = p.m_typeInfo.fromNative(value);
                }
                if(p.changed != null) p.changed(p, newVal);
            }

            public void OnDetached(IntPtr attr)
            {
                CppProxyAttribute p = (CppProxyAttribute)parent.Target;
                if (p == null) return;
                p.detached();
            }
        }

        public event signals.OnChanged changed;
        private Native.IAttribute m_native;
        private IntPtr m_nativeRef;
        private string m_name;
        private string m_descr;
        private signals.EType m_type;
        private ProxyTypes.ITypeMarshaller m_typeInfo;
        private bool m_readOnly;
        private ChangedCatcher m_catcher;
        private CppNativeProxy.Callback m_callback;
        private bool m_callbackRef;

        public CppProxyAttribute(IntPtr native)
        {
            if (native == IntPtr.Zero) throw new ArgumentNullException("native");
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
            if (m_callback != null)
            {
                CppNativeProxy.Callback callback = m_callback;
                System.Runtime.CompilerServices.RuntimeHelpers.PrepareConstrainedRegions();
                try { }
                finally
                {
                    m_native.Unobserve(m_callback.DangerousGetHandle());
                    if (m_callbackRef) m_callback.DangerousRelease();
                    m_callback = null;
                }
                if (disposing) callback.Dispose();
            }
            if (m_nativeRef != IntPtr.Zero)
            {
                Registration.removeObject(m_nativeRef);
                m_nativeRef = IntPtr.Zero;
            }
        }

        void detached()
        {
            if (m_callback != null)
            {
                System.Runtime.CompilerServices.RuntimeHelpers.PrepareConstrainedRegions();
                try { }
                finally
                {
                    if (m_callbackRef) m_callback.DangerousRelease();
                    m_callback = null;
                }
            }
        }

        private void interrogate()
        {
            m_name = Utilities.getString(m_native.Name());
            m_descr = Utilities.getString(m_native.Description());
            m_type = m_native.Type();
            m_typeInfo = ProxyTypes.getTypeInfo(m_type);
            m_readOnly = m_native.isReadOnly();

            m_callbackRef = false;
            CppNativeProxy.Callback callback = CppNativeProxy.CreateCallin(m_catcher, typeof(Native.IAttributeObserver));
            System.Runtime.CompilerServices.RuntimeHelpers.PrepareConstrainedRegions();
            try { }
            finally
            {
                m_callback = callback;
                m_callback.DangerousAddRef(ref m_callbackRef);
                m_native.Observe(m_callback.DangerousGetHandle());
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
                IntPtr buff = Marshal.AllocHGlobal(m_typeInfo.size(value));
                try
                {
                    m_typeInfo.toNative(value, buff);
                    if (!m_native.setValue(buff)) throw new ArgumentOutOfRangeException("value");
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
            IntPtr[] valArr = new IntPtr[numOpt];
            IntPtr[] optArr = new IntPtr[numOpt];
            GCHandle valPin = GCHandle.Alloc(valArr, GCHandleType.Pinned);
            GCHandle optPin = GCHandle.Alloc(optArr, GCHandleType.Pinned);
            try
            {
                m_native.options(valPin.AddrOfPinnedObject(), optPin.AddrOfPinnedObject(), numOpt);
            }
            finally
            {
                valPin.Free();
                optPin.Free();
            }
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
    }

    class CppProxyInEndpoint : signals.IInEndpoint
    {
        private Native.IInEndpoint m_native;
        private IntPtr m_nativeRef;
        private string m_name;
        private string m_descr;
        private signals.EType m_type;
        private signals.IAttributes m_attrs = null;

        public CppProxyInEndpoint(IntPtr native)
        {
            if (native == IntPtr.Zero) throw new ArgumentNullException("native");
            m_nativeRef = native;
            Registration.storeObject(native, this);
            m_native = (Native.IInEndpoint)CppNativeProxy.CreateCallout(native, typeof(Native.IInEndpoint));
            m_native.AddRef();
            interrogate();
        }

        private void interrogate()
        {
            m_name = Utilities.getString(m_native.EPName());
            m_descr = Utilities.getString(m_native.EPDescr());
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
                    if (m_attrs == null) m_attrs = new CppProxyAttributes(attrs);
                }
                return m_attrs;
            }
        }

        public string EPName { get { return m_name; } }
        public string EPDescr { get { return m_descr; } }
        public signals.EType Type { get { return m_type; } }
        public bool isConnected { get { return m_native.isConnected(); } }
        public bool Disconnect() { return m_native.Disconnect(); }

        public bool Connect(signals.IEPRecvFrom recv)
        {
            {
                CppProxyEPRecvFrom proxy = recv as CppProxyEPRecvFrom;
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
        private IntPtr m_nativeRef;
        private string m_name;
        private string m_descr;
        private signals.EType m_type;
        private signals.IAttributes m_attrs = null;

        public CppProxyOutEndpoint(signals.IModule module, IntPtr native)
        {
            if (module == null) throw new ArgumentNullException("module");
            if (native == IntPtr.Zero) throw new ArgumentNullException("native");
            m_module = module;
            m_nativeRef = native;
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
            m_descr = Utilities.getString(m_native.EPDescr());
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
                    if (m_attrs == null) m_attrs = new CppProxyAttributes(attrs);
                }
                return m_attrs;
            }
        }

        public virtual void Dispose() { }
        public string EPName { get { return m_name; } }
        public string EPDescr { get { return m_descr; } }
        public signals.EType Type { get { return m_type; } }
        public bool isConnected { get { return m_native.isConnected(); } }
        public bool Disconnect() { return m_native.Disconnect(); }

        public signals.IEPBuffer CreateBuffer()
        {
            IntPtr pBuf = m_native.CreateBuffer();
            if (pBuf == IntPtr.Zero) return null;
            signals.IEPBuffer buff = (signals.IEPBuffer)Registration.retrieveObject(pBuf);
            if (buff == null) buff = new CppProxyBuffer(m_module, pBuf);
            return buff;
        }

        public bool Connect(signals.IEPSendTo recv)
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

    class CppProxyEPSender : signals.IEPSendTo
    {
        private readonly signals.IBlockDriver m_driver;
        private Native.IEPSendTo m_native;
        private IntPtr m_nativeRef;
        private signals.EType m_type = signals.EType.None;
        private ProxyTypes.ITypeMarshaller m_typeInfo = null;
        private signals.IAttributes m_attrs = null;

        public CppProxyEPSender(signals.IBlockDriver driver, IntPtr native)
        {
            if (driver == null) throw new ArgumentNullException("driver");
            if (native == IntPtr.Zero) throw new ArgumentNullException("native");
            m_driver = driver;
            m_nativeRef = native;
            Registration.storeObject(native, this);
            m_native = (Native.IEPSendTo)CppNativeProxy.CreateCallout(native, typeof(Native.IEPSendTo));
            m_native.AddRef(IntPtr.Zero);
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

        public signals.IAttributes InputAttributes
        {
            get
            {
                if (m_attrs == null)
                {
                    IntPtr attrs = m_native.InputAttributes();
                    if (attrs == IntPtr.Zero) return null;
                    m_attrs = (signals.IAttributes)Registration.retrieveObject(attrs);
                    if (m_attrs == null) m_attrs = new CppProxyAttributes(attrs);
                }
                return m_attrs;
            }
        }

        public int Write(signals.EType type, Array values, int msTimeout)
        {
            if (values == null) throw new ArgumentNullException("values");
            if (type != m_type || m_typeInfo == null)
            {
                m_type = type;
                m_typeInfo = ProxyTypes.getTypeInfo(type);
                if (m_typeInfo == null) throw new NotSupportedException("Cannot retrieve value for this type.");
            }

            Array pinArray = m_typeInfo.toPinnableArray(values);
            GCHandle pin = GCHandle.Alloc(pinArray, GCHandleType.Pinned);
            try
            {
                return (int)m_native.Write(type, pin.AddrOfPinnedObject(), (uint)values.Length, (uint)msTimeout);
            }
            finally
            {
                pin.Free();
            }
        }
    }

    class CppProxyEPRecvFrom : signals.IEPRecvFrom
    {
        private readonly signals.IBlockDriver m_driver;
        private Native.IEPRecvFrom m_native;
        private IntPtr m_nativeRef;
        private signals.EType m_type = signals.EType.None;
        private ProxyTypes.ITypeMarshaller m_typeInfo = null;
        public const int DEFAULT_BUFFER = 100;
        public int BufferSize = DEFAULT_BUFFER;
        private signals.IAttributes m_attrs = null;

        public CppProxyEPRecvFrom(signals.IBlockDriver driver, IntPtr native)
        {
            if (driver == null) throw new ArgumentNullException("driver");
            if (native == IntPtr.Zero) throw new ArgumentNullException("native");
            m_driver = driver;
            m_nativeRef = native;
            Registration.storeObject(native, this);
            m_native = (Native.IEPRecvFrom)CppNativeProxy.CreateCallout(native, typeof(Native.IEPRecvFrom));
        }

        ~CppProxyEPRecvFrom()
        {
            if (m_nativeRef != IntPtr.Zero)
            {
                Registration.removeObject(m_nativeRef);
                m_nativeRef = IntPtr.Zero;
            }
        }

        public void Dispose() { }
        public IntPtr Native { get { return m_nativeRef; } }

        public void Read(signals.EType type, out Array values, bool bReadAll, int msTimeout)
        {
            if (type != m_type || m_typeInfo == null)
            {
                m_type = type;
                m_typeInfo = ProxyTypes.getTypeInfo(type);
                if (m_typeInfo == null) throw new NotSupportedException("Cannot retrieve value for this type.");
            }
            Array pinArray = m_typeInfo.makePinnableArray(BufferSize);
            GCHandle pin = GCHandle.Alloc(pinArray, GCHandleType.Pinned);
            uint read;
            try
            {
                read = m_native.Read(type, pin.AddrOfPinnedObject(), (uint)BufferSize, bReadAll, (uint)msTimeout);
            }
            finally
            {
                pin.Free();
            }
            values = m_typeInfo.fromPinnableArray(pinArray, (int)read);
        }

        public signals.IEPBuffer CreateBuffer()
        {
            IntPtr pBuf = m_native.CreateBuffer();
            if (pBuf == IntPtr.Zero) return null;
            signals.IEPBuffer buff = (signals.IEPBuffer)Registration.retrieveObject(pBuf);
            if (buff == null) buff = new CppProxyBuffer(m_driver.Module, pBuf);
            return buff;
        }

        public signals.IAttributes OutputAttributes
        {
            get
            {
                if (m_attrs == null)
                {
                    IntPtr attrs = m_native.OutputAttributes();
                    if (attrs == IntPtr.Zero) return null;
                    m_attrs = (signals.IAttributes)Registration.retrieveObject(attrs);
                    if (m_attrs == null) m_attrs = new CppProxyAttributes(attrs);
                }
                return m_attrs;
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
        private signals.IAttributes m_outAttrs = null;
        private signals.IAttributes m_inAttrs = null;

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

        public void Read(signals.EType type, out Array values, bool bReadAll, int msTimeout)
        {
            if (m_typeInfo == null) throw new NotSupportedException("Cannot retrieve value for this type.");

            Array pinArray = m_typeInfo.makePinnableArray(BufferSize);
            GCHandle pin = GCHandle.Alloc(pinArray, GCHandleType.Pinned);
            uint read;
            try
            {
                read = m_native.Read(m_type, pin.AddrOfPinnedObject(), (uint)BufferSize, bReadAll, (uint)msTimeout);
            }
            finally
            {
                pin.Free();
            }
            values = m_typeInfo.fromPinnableArray(pinArray, (int)read);
        }

        public int Write(signals.EType type, Array values, int msTimeout)
        {
            if (values == null) throw new ArgumentNullException("values");
            if (m_typeInfo == null) throw new NotSupportedException("Cannot retrieve value for this type.");
            Array pinArray = m_typeInfo.toPinnableArray(values);
            GCHandle pin = GCHandle.Alloc(pinArray, GCHandleType.Pinned);
            try
            {
                return (int)m_native.Write(type, pin.AddrOfPinnedObject(), (uint)values.Length, (uint)msTimeout);
            }
            finally
            {
                pin.Free();
            }
        }

        public signals.IEPBuffer CreateBuffer()
        {
            IntPtr pBuf = m_native.CreateBuffer();
            if (pBuf == IntPtr.Zero) return null;
            signals.IEPBuffer buff = (signals.IEPBuffer)Registration.retrieveObject(pBuf);
            if (buff == null) buff = new CppProxyBuffer(m_module, pBuf);
            return buff;
        }

        public signals.IAttributes InputAttributes
        {
            get
            {
                if (m_inAttrs == null)
                {
                    IntPtr attrs = m_native.InputAttributes();
                    if (attrs == IntPtr.Zero) return null;
                    m_inAttrs = (signals.IAttributes)Registration.retrieveObject(attrs);
                    if (m_inAttrs == null) m_inAttrs = new CppProxyAttributes(attrs);
                }
                return m_inAttrs;
            }
        }

        public signals.IAttributes OutputAttributes
        {
            get
            {
                if (m_outAttrs == null)
                {
                    IntPtr attrs = m_native.OutputAttributes();
                    if (attrs == IntPtr.Zero) return null;
                    m_outAttrs = (signals.IAttributes)Registration.retrieveObject(attrs);
                    if (m_outAttrs == null) m_outAttrs = new CppProxyAttributes(attrs);
                }
                return m_outAttrs;
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

    class CppProxyFunction : signals.IFunction
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
                    if (m_incoming == null) m_incoming = new CppProxyInputFunction(m_spec.Module, newObjRef);
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
                    if (m_outgoing == null) m_outgoing = new CppProxyOutputFunction(m_spec.Module, newObjRef);
                }
                return m_outgoing;
            }
        }
    }

    class CppProxyInputFunction : CppProxyInEndpoint, signals.IInputFunction
	{
        private Native.IEPRecvFrom m_nativeRecv;
        private IntPtr m_nativeRecvRef;
        private readonly signals.IModule m_module;
        private signals.EType m_recvType = signals.EType.None;
        private ProxyTypes.ITypeMarshaller m_recvTypeInfo = null;
        public const int DEFAULT_BUFFER = 100;
        public int BufferSize = DEFAULT_BUFFER;
        private signals.IAttributes m_attrs = null;

        public CppProxyInputFunction(signals.IModule module, IntPtr native)
            :base(native)
        {
            if (module == null) throw new ArgumentNullException("module");
            if (native == IntPtr.Zero) throw new ArgumentNullException("native");
            m_module = module;
            m_nativeRecvRef = new IntPtr(native.ToInt64() + IntPtr.Size);
            m_nativeRecv = (Native.IEPRecvFrom)CppNativeProxy.CreateCallout(m_nativeRecvRef, typeof(Native.IEPRecvFrom));
        }

        public IntPtr NativeReceiver { get { return m_nativeRecvRef; } }

        public void Read(signals.EType type, out Array values, bool bReadAll, int msTimeout)
        {
            if (type != m_recvType || m_recvTypeInfo == null)
            {
                m_recvType = type;
                m_recvTypeInfo = ProxyTypes.getTypeInfo(type);
                if (m_recvTypeInfo == null) throw new NotSupportedException("Cannot retrieve value for this type.");
            }

            Array pinArray = m_recvTypeInfo.makePinnableArray(BufferSize);
            GCHandle pin = GCHandle.Alloc(pinArray, GCHandleType.Pinned);
            uint read;
            try
            {
                read = m_nativeRecv.Read(type, pin.AddrOfPinnedObject(), (uint)BufferSize, bReadAll, (uint)msTimeout);
            }
            finally
            {
                pin.Free();
            }
            values = m_recvTypeInfo.fromPinnableArray(pinArray, (int)read);
        }

        public signals.IEPBuffer CreateBuffer()
        {
            IntPtr pBuf = m_nativeRecv.CreateBuffer();
            if (pBuf == IntPtr.Zero) return null;
            signals.IEPBuffer buff = (signals.IEPBuffer)Registration.retrieveObject(pBuf);
            if (buff == null) buff = new CppProxyBuffer(m_module, pBuf);
            return buff;
        }

        public signals.IAttributes OutputAttributes
        {
            get
            {
                if (m_attrs == null)
                {
                    IntPtr attrs = m_nativeRecv.OutputAttributes();
                    if (attrs == IntPtr.Zero) return null;
                    m_attrs = (signals.IAttributes)Registration.retrieveObject(attrs);
                    if (m_attrs == null) m_attrs = new CppProxyAttributes(attrs);
                }
                return m_attrs;
            }
        }
    }

    class CppProxyOutputFunction : CppProxyOutEndpoint, signals.IOutputFunction
	{
        private Native.IEPSendTo m_nativeSend;
        private IntPtr m_nativeSendRef;
        private signals.EType m_sendType = signals.EType.None;
        private ProxyTypes.ITypeMarshaller m_sendTypeInfo = null;
        private signals.IAttributes m_attrs = null;

        public CppProxyOutputFunction(signals.IModule module, IntPtr native)
            :base(module, native)
        {
            m_nativeSendRef = new IntPtr(native.ToInt64() + IntPtr.Size);
            m_nativeSend = (Native.IEPSendTo)CppNativeProxy.CreateCallout(m_nativeSendRef, typeof(Native.IEPSendTo));
            m_nativeSend.AddRef(IntPtr.Zero);
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

        public int Write(signals.EType type, Array values, int msTimeout)
        {
            if (values == null) throw new ArgumentNullException("values");
            if (type != m_sendType || m_sendTypeInfo == null)
            {
                m_sendType = type;
                m_sendTypeInfo = ProxyTypes.getTypeInfo(type);
                if (m_sendTypeInfo == null) throw new NotSupportedException("Cannot retrieve value for this type.");
            }
            Array pinArray = m_sendTypeInfo.toPinnableArray(values);
            GCHandle pin = GCHandle.Alloc(pinArray, GCHandleType.Pinned);
            try
            {
                return (int)m_nativeSend.Write(type, pin.AddrOfPinnedObject(), (uint)values.Length, (uint)msTimeout);
            }
            finally
            {
                pin.Free();
            }
        }

        public signals.IAttributes InputAttributes
        {
            get
            {
                if (m_attrs == null)
                {
                    IntPtr attrs = m_nativeSend.InputAttributes();
                    if (attrs == IntPtr.Zero) return null;
                    m_attrs = (signals.IAttributes)Registration.retrieveObject(attrs);
                    if (m_attrs == null) m_attrs = new CppProxyAttributes(attrs);
                }
                return m_attrs;
            }
        }
    }
}
