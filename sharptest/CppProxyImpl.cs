using System;
using System.Collections.Generic;
using System.Reflection;
using System.Reflection.Emit;
using System.Runtime.InteropServices;

namespace cppProxy
{
    public abstract class CppNativeProxy
    {
        protected readonly IntPtr thisPtr;
        private static AssemblyBuilder asmBuilder = null;
        private static ModuleBuilder modBuilder = null;
        private static Dictionary<Type, Type> proxyMap = new Dictionary<Type, Type>();

        public static object CreateCallout(IntPtr ifacePtr, Type ifaceType)
        {
            if (ifacePtr == IntPtr.Zero) throw new ArgumentNullException("ifacePtr");
            if (ifaceType == null) throw new ArgumentNullException("ifaceType");

            Type proxyType = null;
            if(!proxyMap.TryGetValue(ifaceType, out proxyType))
            {
                proxyType = BuildDynamicType(ifaceType);
                proxyMap.Add(ifaceType, proxyType);
            }
            return proxyType.GetConstructor(new Type[] { typeof(IntPtr) }).Invoke(new object[] { ifacePtr });
        }

        static CppNativeProxy()
        {
            AssemblyName assemblyName = new AssemblyName();
            assemblyName.Name = "CppNativeProxy";
            AppDomain thisDomain = System.Threading.Thread.GetDomain();

            asmBuilder = thisDomain.DefineDynamicAssembly(assemblyName, AssemblyBuilderAccess.Run);

            Type caType = typeof(System.Runtime.CompilerServices.RuntimeCompatibilityAttribute);
            CustomAttributeBuilder caBuilder = new CustomAttributeBuilder(
                caType.GetConstructor(Type.EmptyTypes), new object[] { });
            asmBuilder.SetCustomAttribute(caBuilder);

#if DEBUG
            modBuilder = asmBuilder.DefineDynamicModule(asmBuilder.GetName().Name, true);
#else
            modBuilder = asmBuilder.DefineDynamicModule(asmBuilder.GetName().Name, false);
#endif
        }

        protected CppNativeProxy(IntPtr ifacePtr)
        {
            thisPtr = ifacePtr;
        }

        private static Type BuildDynamicType(Type ifaceType)
        {   // from http://www.codeproject.com/Articles/13337/Introduction-to-Creating-Dynamic-Types-with-Reflec
            if (ifaceType == null) throw new ArgumentNullException("ifaceType");
            if (!ifaceType.IsInterface || (!ifaceType.IsNestedPublic && !ifaceType.IsPublic)) throw new ArgumentException("Must represent a public interface", "ifaceType");

            string typeName = "proxy_" + ifaceType.Name;
            TypeBuilder typeBuilder = modBuilder.DefineType(typeName,
                TypeAttributes.Public | TypeAttributes.Class | TypeAttributes.AnsiClass | TypeAttributes.AutoClass |
                TypeAttributes.BeforeFieldInit | TypeAttributes.AutoLayout,
                typeof(CppNativeProxy), new Type[] { ifaceType });

            ConstructorBuilder constructor = typeBuilder.DefineConstructor(
                MethodAttributes.Public | MethodAttributes.SpecialName | MethodAttributes.RTSpecialName,
                CallingConventions.Standard, new Type[] { typeof(IntPtr) });

            //Define the reflection ConstructorInfor for System.Object
            ConstructorInfo conObj = typeof(CppNativeProxy).GetConstructor(BindingFlags.NonPublic | BindingFlags.Instance,
                null, new Type[] { typeof(IntPtr) }, null);

            //call constructor of base object
            ILGenerator il = constructor.GetILGenerator();
            il.Emit(OpCodes.Ldarg_0);
            il.Emit(OpCodes.Ldarg_1);
            il.Emit(OpCodes.Call, conObj);
            il.Emit(OpCodes.Ret);

            uint vtblOff = 0;
            uint vtblNum = 0;
            BuildDynamicInterface(ifaceType, typeBuilder, 0, ref vtblNum, ref vtblOff);

            return typeBuilder.CreateType();
        }

        private static void BuildDynamicInterface(Type ifaceType, TypeBuilder typeBuilder, uint vtblNo, ref uint vtblCount, ref uint vtblOff)
        {
            if (ifaceType == null) throw new ArgumentNullException("ifaceType");
            if (typeBuilder == null) throw new ArgumentNullException("typeBuilder");
            if (!ifaceType.IsInterface) throw new ArgumentException("Must represent an interface", "ifaceType");

            Type[] parentTypes = ifaceType.GetInterfaces();
            if (parentTypes.Length > 0)
            {
                BuildDynamicInterface(parentTypes[0], typeBuilder, vtblNo, ref vtblCount, ref vtblOff);
                for (int idx = 1; idx < parentTypes.Length; idx++)
                {
                    uint nextVtbl = ++vtblCount;
                    uint nextOff = 0;
                    BuildDynamicInterface(parentTypes[idx], typeBuilder, nextVtbl, ref vtblCount, ref nextOff);
                }
            }
            MethodInfo[] methods = ifaceType.GetMethods();
            FieldInfo thisPtrRef = typeof(CppNativeProxy).GetField("thisPtr", BindingFlags.NonPublic | BindingFlags.Instance);
            for (int idx = 0; idx < methods.Length; idx++)
            {
                // adapted from http://rogue-modron.blogspot.com/2011/11/invoking-native.html
                MethodInfo method = methods[idx];
                Type calliReturnType = GetPointerTypeIfReference(method.ReturnType);
                ParameterInfo[] methodParms = method.GetParameters();
                Type[] invokeParameterTypes = new Type[methodParms.Length];
                Type[] calliParameterTypes = new Type[methodParms.Length+1];
                calliParameterTypes[0] = typeof(IntPtr);
                for (int i = 0; i < methodParms.Length; i++)
                {
                    invokeParameterTypes[i] = methodParms[i].ParameterType;
                    calliParameterTypes[i+1] = GetPointerTypeIfReference(methodParms[i].ParameterType);
                }
                
                MethodBuilder calliMethod = typeBuilder.DefineMethod(method.Name,
                    MethodAttributes.Public | MethodAttributes.Virtual,
                    method.ReturnType, invokeParameterTypes);
                ILGenerator generator = calliMethod.GetILGenerator();

                LocalBuilder vtbl = generator.DeclareLocal(typeof(IntPtr));
#if DEBUG
                vtbl.SetLocalSymInfo("vtbl");
#endif

                // push "this"
                generator.Emit(OpCodes.Ldarg_0);
                generator.Emit(OpCodes.Ldfld, thisPtrRef);
                if (vtblNo != 0)
                {
                    generator.Emit(OpCodes.Ldc_I4, IntPtr.Size * vtblNo);
                    generator.Emit(OpCodes.Add);
                }
                generator.Emit(OpCodes.Dup);
                generator.Emit(OpCodes.Stloc_0);

                // Emits instructions for loading the parameters into the stack.
                for (int i = 1; i < calliParameterTypes.Length; i++)
                {
                    switch (i)
                    {
                        case 1:
                            generator.Emit(OpCodes.Ldarg_1);
                            break;
                        case 2:
                            generator.Emit(OpCodes.Ldarg_2);
                            break;
                        case 3:
                            generator.Emit(OpCodes.Ldarg_3);
                            break;
                        default:
                            generator.Emit(OpCodes.Ldarg, i);
                            break;
                    }
                }

                // Emits instruction for loading the address of the
                //native function into the stack.

                generator.Emit(OpCodes.Ldloc_0);
                generator.Emit(OpCodes.Ldind_I);
                if (vtblOff != 0)
                {
                    generator.Emit(OpCodes.Ldc_I4, IntPtr.Size * vtblOff);
                    generator.Emit(OpCodes.Add);
                }
                vtblOff++;
                generator.Emit(OpCodes.Ldind_I);

                // Emits calli opcode.
                generator.EmitCalli(OpCodes.Calli, CallingConvention.ThisCall, calliReturnType, calliParameterTypes);

                // Emits instruction for returning a value.
                generator.Emit(OpCodes.Ret);
            }
        }

        private static Type GetPointerTypeIfReference(Type type)
        {
            return type.IsByRef ? Type.GetType(type.FullName.Replace("&", "*")) : type;
        }

        public static Callback CreateCallin(object target, Type ifaceType)
        {
            return new Callback(ifaceType, target);
        }

        private struct Signature : IEquatable<Signature>
        {
            public Type returnType;
            public Type[] parms;

            public override int GetHashCode()
            {
                int hash = returnType == null ? 0 : returnType.GetHashCode();
                if (parms != null)
                {
                    for (int i = 0; i < parms.Length; i++)
                    {
                        if (parms[i] != null)
                        {
                            unchecked
                            {
                                hash = (hash * 2) ^ parms[i].GetHashCode();
                            }
                        }
                    }
                }
                return hash;
            }

            public override bool Equals(object obj)
            {
                if (obj == null || GetType() != obj.GetType())
                {
                    return false;
                }
                return Equals((Signature)obj);
            }

            public bool Equals(Signature other)
            {
                if (returnType != other.returnType || (parms == null) != (other.returnType == null)) return false;
                if (parms == null) return true;
                if (parms.Length != other.parms.Length) return false;
                for (int i = 0; i < parms.Length; i++)
                {
                    if ((parms[i] == null) != (other.parms[i] == null)
                        || (parms[i] != null && !parms[i].Equals(other.parms[i]))) return false;
                }
                return true;
            }

            public override string ToString()
            {
                string parmList;
                if (parms == null)
                {
                    parmList = "null";
                }
                else
                {
                    parmList = "";
                    for (int i = 0; i < parms.Length; i++)
                    {
                        if (i > 0) parmList += ",";
                        parmList += parms[i].ToString();
                    }
                }
                return (returnType == null ? "null" : returnType.ToString()) + ":" + parmList;
            }
        }

        public class Callback : IDisposable
        {
            private IntPtr m_native = IntPtr.Zero;
            private List<Delegate> m_delegateList;
            private static int nextDelegate = 1;
            private static Dictionary<Signature, Type> delegateCache;

            public Callback(Type ifaceType, object target)
            { // technique from http://code4k.blogspot.com/2010/10/implementing-unmanaged-c-interface.html
                if (target == null) throw new ArgumentNullException("target");
                if (ifaceType == null) throw new ArgumentNullException("ifaceType");
                if (!ifaceType.IsInterface) throw new ArgumentException("Must represent an interface", "ifaceType");
                if (!ifaceType.IsInstanceOfType(target)) throw new ArgumentException("Must implement the specified interface", "target");
                if (ifaceType.GetInterfaces().Length > 0) new NotSupportedException("Interface inheritence not yet supported");

                m_delegateList = new List<Delegate>();
                MethodInfo[] methods = ifaceType.GetMethods();
                m_native = Marshal.AllocHGlobal(IntPtr.Size * (methods.Length+1));
                IntPtr vtblPtr = new IntPtr(m_native.ToInt64() + IntPtr.Size);   // Write pointer to vtbl
                Marshal.WriteIntPtr(m_native, vtblPtr);

                for (int idx = 0; idx < methods.Length; idx++)
                {
                    MethodInfo method = methods[idx];
                    Delegate del = createDelegate(target, method);
                    m_delegateList.Add(del);
                    Marshal.WriteIntPtr(vtblPtr, IntPtr.Size * idx, Marshal.GetFunctionPointerForDelegate(del));
                }
            }

            public IntPtr Pointer { get { return m_native; } }

            static Callback()
            {
                delegateCache = new Dictionary<Signature, Type>();
            }

            ~Callback()
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
                if (m_native != IntPtr.Zero)
                {
                    Marshal.FreeHGlobal(m_native);
                    m_native = IntPtr.Zero;
                }
            }

            private Delegate createDelegate(object ifaceObj, MethodInfo method)
            { // adapted from http://blogs.msdn.com/b/joelpob/archive/2004/02/15/73239.aspx
                ParameterInfo[] parms = method.GetParameters();

                Signature sig;
                sig.returnType = method.ReturnType;
                sig.parms = new Type[parms.Length];
                for (int i = 0; i < parms.Length; i++)
                {
                    sig.parms[i] = parms[i].ParameterType;
                }

                Type dType;
                if (!delegateCache.TryGetValue(sig, out dType))
                {
                    TypeBuilder typeBuilder = modBuilder.DefineType(String.Format("delegate_{0}", nextDelegate++),
                        TypeAttributes.Public | TypeAttributes.Sealed | TypeAttributes.Class |
                        TypeAttributes.AnsiClass | TypeAttributes.AutoClass, typeof(MulticastDelegate));

                    ConstructorBuilder constructorBuilder = typeBuilder.DefineConstructor(
                        MethodAttributes.RTSpecialName | MethodAttributes.HideBySig | MethodAttributes.Public,
                        CallingConventions.Standard, new Type[] { typeof(object), typeof(System.IntPtr) });
                    constructorBuilder.SetImplementationFlags(MethodImplAttributes.Runtime | MethodImplAttributes.Managed);

                    MethodBuilder methodBuilder = typeBuilder.DefineMethod("Invoke",
                        MethodAttributes.Public | MethodAttributes.HideBySig | MethodAttributes.NewSlot |
                        MethodAttributes.Virtual, sig.returnType, sig.parms);
                    methodBuilder.SetImplementationFlags(MethodImplAttributes.Managed | MethodImplAttributes.Runtime);

                    dType = typeBuilder.CreateType();
                    delegateCache.Add(sig, dType);
                }
                return Delegate.CreateDelegate(dType, ifaceObj, method);
            }
        }
    }
}
