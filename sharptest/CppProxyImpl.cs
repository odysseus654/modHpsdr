using System;
using System.Collections.Generic;
using System.Reflection;
using System.Reflection.Emit;
using System.Runtime.InteropServices;

namespace cppProxy
{
    public abstract class CppNativeProxy
    {
        protected IntPtr thisPtr;
        private static AssemblyBuilder asmBuilder = null;
        private static ModuleBuilder modBuilder = null;
        private static Dictionary<Type, Type> proxyMap = new Dictionary<Type, Type>();

        public static object Create(IntPtr ifacePtr, Type ifaceType)
        {
            Type proxyType = null;
            if(!proxyMap.TryGetValue(ifaceType, out proxyType))
            {
                proxyType = BuildDynamicType(ifaceType);
                proxyMap.Add(ifaceType, proxyType);
            }
            return proxyType.GetConstructor(new Type[] { typeof(IntPtr) }).Invoke(new object[] { ifacePtr });
        }

        protected CppNativeProxy(IntPtr ifacePtr)
        {
            thisPtr = ifacePtr;
        }

        private static Type BuildDynamicType(Type ifaceType)
        {   // from http://www.codeproject.com/Articles/13337/Introduction-to-Creating-Dynamic-Types-with-Reflec
            if (asmBuilder == null)
            {
                AssemblyName assemblyName = new AssemblyName();
                assemblyName.Name = "CppNativeProxy";
                AppDomain thisDomain = System.Threading.Thread.GetDomain();
                asmBuilder = thisDomain.DefineDynamicAssembly(assemblyName, AssemblyBuilderAccess.Run);
            }
            if (modBuilder == null)
            {
                modBuilder = asmBuilder.DefineDynamicModule(asmBuilder.GetName().Name, false);
            }

            string typeName = "proxy_" + ifaceType.Name;
            TypeBuilder typeBuilder = modBuilder.DefineType(typeName,
                TypeAttributes.Public | TypeAttributes.Class | TypeAttributes.AutoClass | TypeAttributes.BeforeFieldInit |
                TypeAttributes.AutoLayout,
                typeof(CppNativeProxy), new Type[] { ifaceType });

            {
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
            }

            MethodInfo[] methods = ifaceType.GetMethods();
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
                
                MethodBuilder calliMethod = typeBuilder.DefineMethod(method.Name, MethodAttributes.Public | MethodAttributes.Virtual,
                    method.ReturnType, invokeParameterTypes);
                ILGenerator generator = calliMethod.GetILGenerator();

                // push "this"
                FieldInfo thisPtrRef = typeof(CppNativeProxy).GetField("thisPtr", BindingFlags.NonPublic | BindingFlags.Instance);
                generator.Emit(OpCodes.Ldarg_0);
                generator.Emit(OpCodes.Ldfld, thisPtrRef);

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
                generator.Emit(OpCodes.Ldarg_0);
                generator.Emit(OpCodes.Ldfld, thisPtrRef);
                generator.Emit(IntPtr.Size == 4 ? OpCodes.Ldind_I4 : OpCodes.Ldind_I8);
                if (idx != 0)
                {
                    generator.Emit(OpCodes.Ldc_I4, IntPtr.Size * idx);
                    generator.Emit(OpCodes.Add);
                }
                generator.Emit(IntPtr.Size == 4 ? OpCodes.Ldind_I4 : OpCodes.Ldind_I8);

                // Emits calli opcode.
                generator.EmitCalli(OpCodes.Calli, CallingConvention.ThisCall, calliReturnType, calliParameterTypes);

                // Emits instruction for returning a value.
                generator.Emit(OpCodes.Ret);
            }

            return typeBuilder.CreateType();
        }

        private static Type GetPointerTypeIfReference(Type type)
        {
            return type.IsByRef ? Type.GetType(type.FullName.Replace("&", "*")) : type;
        }
    }
}
