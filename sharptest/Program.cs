using System;

namespace sharptest
{
    class Program
    {
        static void Main(string[] args)
        {
            cppProxy.Discover.DoDiscovery(@"D:\modules\hpsdr-mod\Debug");
            int _i = 1;
        }
    }
}
