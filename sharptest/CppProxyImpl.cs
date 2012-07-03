using System;
using System.Collections.Generic;
using System.Reflection;
using System.Reflection.Emit;
using System.Runtime.InteropServices;
using System.Runtime.Remoting.Proxies;
using System.Runtime.Remoting.Messaging;

namespace cppProxy
{
    class CppNativeProxy : RealProxy
    {
        private IntPtr thisPtr;
        private Dictionary<string,Delegate> methods;

        public static object Create(IntPtr ifacePtr, Type ifaceType)
        {
            CppNativeProxy proxy = new CppNativeProxy(ifaceType, ifacePtr);
            return proxy.GetTransparentProxy();
        }

        private CppNativeProxy(Type ifaceType, IntPtr ifacePtr)
            : base(ifaceType)
        {
            methods = new Dictionary<string, Delegate>();
            thisPtr = ifacePtr;
            IntPtr vtblPtr = Marshal.ReadIntPtr(thisPtr);
            BuildDynamicInterface(ifaceType, vtblPtr);
        }

        public override IMessage Invoke(IMessage msg)
        {
            IMethodCallMessage call = (IMethodCallMessage)msg;
            Delegate del = null;
            if(!methods.TryGetValue(call.MethodName, out del)) throw new NotImplementedException("method not defined");
            object[] args = new object[call.ArgCount + 1];
            args[0] = thisPtr;
            call.Args.CopyTo(args, 1);
            try
            {
                object ret = del.DynamicInvoke(args);
                return new ReturnMessage(ret, null, 0, call.LogicalCallContext, call);
            }
            catch (TargetInvocationException e)
            {
                throw e.InnerException;
            }
        }

        private void BuildDynamicInterface(Type ifaceType, IntPtr vtblBase)
        {
            MethodInfo[] methods = ifaceType.GetMethods();
            for (int idx = 0; idx < methods.Length; idx++)
            {
                MethodInfo method = methods[idx];
                IntPtr methodAddress = Marshal.ReadIntPtr(vtblBase, IntPtr.Size * idx);
                Delegate newDel = BuildDynamicDelegate(method, methodAddress);
                this.methods.Add(method.Name, newDel);
            }
        }

        private delegate TResult Func<TResult>(IntPtr here);
        private delegate TResult Func<T, TResult>(IntPtr here, T arg);
        private delegate TResult Func<T1, T2, TResult>(IntPtr here, T1 arg1, T2 arg2);
        private delegate TResult Func<T1, T2, T3, TResult>(IntPtr here, T1 arg1, T2 arg2, T3 arg3);
        private delegate TResult Func<T1, T2, T3, T4, TResult>(IntPtr here, T1 arg1, T2 arg2, T3 arg3, T4 arg4);
        private delegate TResult Func<T1, T2, T3, T4, T5, TResult>(IntPtr here, T1 arg1, T2 arg2, T3 arg3, T4 arg4, T5 arg5);
        private delegate TResult Func<T1, T2, T3, T4, T5, T6, TResult>(IntPtr here, T1 arg1, T2 arg2, T3 arg3, T4 arg4, T5 arg5, T6 arg6);

        public Delegate BuildDynamicDelegate(MethodInfo invokeInfo, IntPtr methodAddress)
        {
            // adapted from http://rogue-modron.blogspot.com/2011/11/invoking-native.html
            // and http://stackoverflow.com/questions/773099/generating-delegate-types-dynamically-in-c-sharp

            // Gets the return type for the dynamic method and calli.
            Type delegateType = invokeInfo.GetType();

            // Note: for calli, a type such as System.Int32& must be
            // converted to System.Int32* otherwise the execution 
            // will be slower.
            Type invokeReturnType = invokeInfo.ReturnType;
            Type calliReturnType = GetPointerTypeIfReference(invokeInfo.ReturnType);

            // Gets the parameter types for the dynamic method 
            // and calli.
            ParameterInfo[] invokeParameters = invokeInfo.GetParameters();
            Type[] invokeParameterTypes = new Type[invokeParameters.Length+1];
            Type[] calliParameterTypes = new Type[invokeParameters.Length+1];
            calliParameterTypes[0] = invokeParameterTypes[0] = typeof(IntPtr);
            for (int i = 0; i < invokeParameters.Length; i++)
            {
                invokeParameterTypes[i+1] = invokeParameters[i].ParameterType;
                calliParameterTypes[i+1] = GetPointerTypeIfReference(invokeParameters[i].ParameterType);
            }

            // Defines the delegate type
            Type dType = null;
            switch(invokeParameterTypes.Length-1)
            {
                case 0:
                    dType = typeof(Func<>).MakeGenericType(invokeReturnType);
                    break;
                case 1:
                    dType = typeof(Func<,>).MakeGenericType(invokeParameterTypes[1], invokeReturnType);
                    break;
                case 2:
                    dType = typeof(Func<,,>).MakeGenericType(invokeParameterTypes[1], invokeParameterTypes[2],
                        invokeReturnType);
                    break;
                case 3:
                    dType = typeof(Func<,,,>).MakeGenericType(invokeParameterTypes[1], invokeParameterTypes[2],
                        invokeParameterTypes[3], invokeReturnType);
                    break;
                case 4:
                    dType = typeof(Func<,,,,>).MakeGenericType(invokeParameterTypes[1], invokeParameterTypes[2],
                        invokeParameterTypes[3], invokeParameterTypes[4], invokeReturnType);
                    break;
                case 5:
                    dType = typeof(Func<,,,,,>).MakeGenericType(invokeParameterTypes[1], invokeParameterTypes[2],
                        invokeParameterTypes[3], invokeParameterTypes[4], invokeParameterTypes[5], invokeReturnType);
                    break;
                case 6:
                    dType = typeof(Func<,,,,,,>).MakeGenericType(invokeParameterTypes[1], invokeParameterTypes[2],
                        invokeParameterTypes[3], invokeParameterTypes[4], invokeParameterTypes[5],
                        invokeParameterTypes[6], invokeReturnType);
                    break;
                default:
                    throw new NotImplementedException("too many arguments");
            }

            // Defines the dynamic method.
            DynamicMethod calliMethod = new DynamicMethod(invokeInfo.Name, invokeReturnType,
                    invokeParameterTypes, typeof(CppNativeProxy), true);

            // Gets an ILGenerator.
            ILGenerator generator = calliMethod.GetILGenerator();

            // push "this"
            generator.Emit(OpCodes.Ldarg_0);

            // Emits instructions for loading the parameters into the stack.
            for (int i = 1; i < calliParameterTypes.Length; i++)
            {
                switch(i)
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
            switch (IntPtr.Size)
            {
                case 4:
                    generator.Emit(OpCodes.Ldc_I4, methodAddress.ToInt32());
                    break;
                case 8:
                    generator.Emit(OpCodes.Ldc_I8, methodAddress.ToInt64());
                    break;
                default:
                    throw new PlatformNotSupportedException("unexpected pointer size");
            }

            // Emits calli opcode.
            generator.EmitCalli(OpCodes.Calli, CallingConvention.ThisCall, calliReturnType, calliParameterTypes);

            // Emits instruction for returning a value.
            generator.Emit(OpCodes.Ret);

            return calliMethod.CreateDelegate(dType);
        }

        private static Type GetPointerTypeIfReference(Type type)
        {
            return type.IsByRef ? Type.GetType(type.FullName.Replace("&", "*")) : type;
        }
    }
}
