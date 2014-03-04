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
using System.Reflection;
using System.Reflection.Emit;
using System.Runtime.InteropServices;

namespace cppProxy
{
    static class Utilities
    {
        private static unsafe int getStringLength(IntPtr strz)
        {
            if (strz == IntPtr.Zero) throw new ArgumentNullException("strz");
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
                TypeAttributes.Public | TypeAttributes.Class | TypeAttributes.AutoClass |
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

                // assemble the parameters
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

                // create the method
                MethodBuilder calliMethod = typeBuilder.DefineMethod(method.Name,
                    MethodAttributes.Public | MethodAttributes.Virtual,
                    method.ReturnType, invokeParameterTypes);
                ILGenerator generator = calliMethod.GetILGenerator();

                // retrieve our stored "this"
                generator.Emit(OpCodes.Ldarg_0);
                generator.Emit(OpCodes.Ldfld, thisPtrRef);

                // adjust "this" to find our vtbl
                if (vtblNo != 0)
                {
                    Emit_Ldc_I4(generator, IntPtr.Size * vtblNo);
                    generator.Emit(OpCodes.Add);
                }

                generator.Emit(OpCodes.Dup);
                if (calliParameterTypes.Length > 0)
                {
                    LocalBuilder vtbl = generator.DeclareLocal(typeof(IntPtr));
#if DEBUG
                    vtbl.SetLocalSymInfo("vtbl");
#endif
                    Emit_StLoc(generator, vtbl);

                    // Emits instructions for loading the parameters into the stack.
                    for (int i = 1; i < calliParameterTypes.Length; i++)
                    {
                        Emit_LdArg(generator, i);
                    }
                    Emit_LdLoc(generator, vtbl);
                }

                // Find the function pointer inside the selected vtbl
                generator.Emit(OpCodes.Ldind_I);
                if (vtblOff != 0)
                {
                    Emit_Ldc_I4(generator, IntPtr.Size * vtblOff);
                    generator.Emit(OpCodes.Add);
                }
                vtblOff++;  // bump function pointer index for next caller
                generator.Emit(OpCodes.Ldind_I);

                // Emits calli opcode.
                // Note that verification throws "Expected ByRef", but this is explicitly "Correct CIL" according to Part III:3.20
                generator.EmitCalli(OpCodes.Calli, CallingConvention.ThisCall, calliReturnType, calliParameterTypes);

                // Returning to the caller, with a value if requested.
                generator.Emit(OpCodes.Ret);
            }
        }

        private static Type GetPointerTypeIfReference(Type type)
        {
            return type.IsByRef ? Type.GetType(type.FullName.Replace("&", "*")) : type;
        }

        private static void Emit_LdLoc(ILGenerator generator, LocalBuilder local)
        {
            // Helper function to generate efficient CIL for retrieving a local variable
            switch (local.LocalIndex)
            {
                case 0:
                    generator.Emit(OpCodes.Ldloc_0);
                    break;
                case 1:
                    generator.Emit(OpCodes.Ldloc_1);
                    break;
                case 2:
                    generator.Emit(OpCodes.Ldloc_2);
                    break;
                case 3:
                    generator.Emit(OpCodes.Ldloc_3);
                    break;
                default:
                    generator.Emit((local.LocalIndex < 256) ? OpCodes.Ldloc_S : OpCodes.Ldloc, local.LocalIndex);
                    break;
            }
        }

        private static void Emit_StLoc(ILGenerator generator, LocalBuilder local)
        {
            // Helper function to generate efficient CIL for storing a local variable
            switch (local.LocalIndex)
            {
                case 0:
                    generator.Emit(OpCodes.Stloc_0);
                    break;
                case 1:
                    generator.Emit(OpCodes.Stloc_1);
                    break;
                case 2:
                    generator.Emit(OpCodes.Stloc_2);
                    break;
                case 3:
                    generator.Emit(OpCodes.Stloc_3);
                    break;
                default:
                    generator.Emit((local.LocalIndex < 256) ? OpCodes.Stloc_S : OpCodes.Stloc, local.LocalIndex);
                    break;
            }
        }

        private static void Emit_LdArg(ILGenerator generator, int arg)
        {
            // Helper function to generate efficient CIL for retrieving a parameter
            switch (arg)
            {
                case 0:
                    generator.Emit(OpCodes.Ldarg_0);
                    break;
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
                    generator.Emit((arg < 256) ? OpCodes.Ldarg_S : OpCodes.Ldarg, arg);
                    break;
            }
        }

        private static void Emit_Ldc_I4(ILGenerator generator, long val)
        {
            // Helper function to generate efficient CIL for using a constant int value
            switch (val)
            {
                case -1:
                    generator.Emit(OpCodes.Ldc_I4_M1);
                    break;
                case 0:
                    generator.Emit(OpCodes.Ldc_I4_0);
                    break;
                case 1:
                    generator.Emit(OpCodes.Ldc_I4_1);
                    break;
                case 2:
                    generator.Emit(OpCodes.Ldc_I4_2);
                    break;
                case 3:
                    generator.Emit(OpCodes.Ldc_I4_3);
                    break;
                case 4:
                    generator.Emit(OpCodes.Ldc_I4_4);
                    break;
                case 5:
                    generator.Emit(OpCodes.Ldc_I4_5);
                    break;
                case 6:
                    generator.Emit(OpCodes.Ldc_I4_6);
                    break;
                case 7:
                    generator.Emit(OpCodes.Ldc_I4_7);
                    break;
                case 8:
                    generator.Emit(OpCodes.Ldc_I4_8);
                    break;
                default:
                    generator.Emit((val >= -128 && val < 128) ? OpCodes.Ldc_I4_S : OpCodes.Ldc_I4, val);
                    break;
            }
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

        public class Callback : Microsoft.Win32.SafeHandles.SafeHandleZeroOrMinusOneIsInvalid
        {
            private List<Delegate> m_delegateList;
            private static int nextDelegate = 1;
            private static Dictionary<Signature, Type> delegateCache;

            public Callback(Type ifaceType, object target):base(true)
            { // technique from http://code4k.blogspot.com/2010/10/implementing-unmanaged-c-interface.html
                if (target == null) throw new ArgumentNullException("target");
                if (ifaceType == null) throw new ArgumentNullException("ifaceType");
                if (!ifaceType.IsInterface) throw new ArgumentException("Must represent an interface", "ifaceType");
                if (!ifaceType.IsInstanceOfType(target)) throw new ArgumentException("Must implement the specified interface", "target");
                if (ifaceType.GetInterfaces().Length > 0) new NotSupportedException("Interface inheritence not yet supported");

                m_delegateList = new List<Delegate>();
                MethodInfo[] methods = ifaceType.GetMethods();
                handle = Marshal.AllocHGlobal(IntPtr.Size * (methods.Length+1));
                IntPtr vtblPtr = new IntPtr(handle.ToInt64() + IntPtr.Size);   // Write pointer to vtbl
                Marshal.WriteIntPtr(handle, vtblPtr);

                for (int idx = 0; idx < methods.Length; idx++)
                {
                    MethodInfo method = methods[idx];
                    Delegate del = createDelegate(target, method);
                    m_delegateList.Add(del);
                    Marshal.WriteIntPtr(vtblPtr, IntPtr.Size * idx, Marshal.GetFunctionPointerForDelegate(del));
                }
            }

            static Callback()
            {
                delegateCache = new Dictionary<Signature, Type>();
            }

            override protected bool ReleaseHandle()
            {
                Marshal.FreeHGlobal(handle);
                return true;
            }

            private static Delegate createDelegate(object ifaceObj, MethodInfo method)
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
                        TypeAttributes.AutoClass, typeof(MulticastDelegate));

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
