﻿using System;
using System.Collections.Generic;
using System.Threading;

namespace sharptest
{
    class Program
    {
        static void Main(string[] args)
        {
            signals.IBlockDriver[] drivers = cppProxy.CppProxyModuleDriver.DoDiscovery(@"D:\modules\hpsdr-mod\Debug");

            signals.IBlock fft = drivers[2].Create();

            signals.IBlockDriver hpsdrDriver = drivers[4];
            signals.IBlock[] devices = hpsdrDriver.Discover();
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

            signals.IOutEndpoint recv1 = hpsdr.Outgoing[2];
            signals.IEPBuffer buff = recv1.CreateBuffer();
            signals.IInEndpoint fftIn = fft.Incoming[0];
            recv1.Connect(buff);
            fftIn.Connect(buff);

            signals.IOutEndpoint fftOut = fft.Outgoing[0];
            signals.IEPBuffer buff2 = fftOut.CreateBuffer();
            fftOut.Connect(buff2);
            cppProxy.ReceiveStream stream = new cppProxy.ReceiveStream(fftOut.Type, buff2);
            stream.data += new cppProxy.ReceiveStream.OnReceive(OnStream);

            hpsdr.Start();
            Thread.Sleep(30000);
            hpsdr.Stop();

            Console.Out.WriteLine(String.Format("{0} received total", packetsReceived));
            Thread.Sleep(20000);
        }

        protected static object st_screenLock = new object();
        static private void OnChanged(string name, signals.EType type, object value)
        {
            lock(st_screenLock)
            {
                if (value == null)
                {
                    Console.Out.WriteLine(String.Format("{0}: null", name));
                }
                else
                {
                    Console.Out.WriteLine(String.Format("{0}: ({1}){2}", name, value.GetType().Name, value));
                }
            }
        }

        protected static int packetsReceived = 0;
        static private void OnStream(object[] data)
        {
            if (data.Length != 0)
            {
                lock (st_screenLock)
                {
                    Console.Out.WriteLine(String.Format("received: {0} {1}", data.Length, data[0].GetType().Name));
                    packetsReceived += data.Length;
                }
            }
        }
    }
}
