using System;

namespace sharptest
{
    class Program
    {
        static void Main(string[] args)
        {
            signals.IBlockDriver[] drivers = cppProxy.CppProxyModuleDriver.DoDiscovery(@"D:\modules\hpsdr-mod\Debug");
            int _i = 1;
        }
    }
}
