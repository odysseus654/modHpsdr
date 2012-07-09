using System;
using System.Collections.Generic;
using System.Threading;

namespace sharptest
{
    class Program
    {
        static void Main(string[] args)
        {
            signals.IBlockDriver[] drivers = cppProxy.CppProxyModuleDriver.DoDiscovery(@"D:\modules\hpsdr-mod\Debug");

            signals.IBlockDriver hpsdrDrver = drivers[1];
            signals.IBlock[] devices = hpsdrDrver.Discover();
            Console.Out.WriteLine(String.Format("{0} devices found", devices.Length));

            signals.IBlock hpsdr = devices[0];

            signals.IAttributes attrs = hpsdr.Attributes;
            signals.OnChanged evt = new signals.OnChanged(OnChanged);
            signals.IAttribute[] attrList = attrs.Itemize();
            for (int i = 0; i < attrList.Length; i++)
            {
                attrList[i].changed += evt;
            }

            signals.IAttribute recvSpeed = attrs.GetByName("recvRate");
            recvSpeed.Value = 48000;

            hpsdr.Start();
            Thread.Sleep(30000);
            hpsdr.Stop();
        }

        protected static object st_screenLock = new object();
        static private void OnChanged(string name, signals.EType type, object value)
        {
            lock(st_screenLock)
            {
                Console.Out.WriteLine(String.Format("{0}: ({1}){2}", name, value.GetType().Name, value));
            }
        }
    }
}
