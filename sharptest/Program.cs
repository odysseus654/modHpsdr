using System;
using System.Collections.Generic;
using System.Threading;

namespace sharptest
{
    class Program
    {
        static void Main(string[] args)
        {
            ModLibrary library = new ModLibrary();
            cppProxy.CppProxyModuleDriver.DoDiscovery(@"D:\modules\hpsdr-mod\Debug", library);

            Schematic circuit = new Schematic();
            Schematic.Element fftElem = new Schematic.Element(Schematic.ElementType.Module, "fft");
            Schematic.Element radioElem = new Schematic.Element(Schematic.ElementType.Module, "radio");

            circuit.addGeneric(fftElem);
            circuit.add(radioElem);
            circuit.connect(radioElem, "recv1", fftElem, 0);
            List<Schematic> options = circuit.resolve(library);

            Dictionary<Schematic.UniqueElemKey, object> radio = options[0].construct();

            signals.IBlock hpsdr = (signals.IBlock)radio[new Schematic.UniqueElemKey(radioElem)];
            signals.IBlock fft = (signals.IBlock)radio[new Schematic.UniqueElemKey(fftElem)];

            signals.IAttributes attrs = hpsdr.Attributes;
            signals.OnChanged evt = new signals.OnChanged(OnChanged);
            signals.IAttribute[] attrList = attrs.Itemize();
            for (int i = 0; i < attrList.Length; i++)
            {
                attrList[i].changed += evt;
            }

            signals.IAttribute recvSpeed = attrs.GetByName("recvRate");
            recvSpeed.Value = 48000;

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
//                lock (st_screenLock)
                {
//                    Console.Out.WriteLine(String.Format("received: {0} {1}", data.Length, data[0].GetType().Name));
                    packetsReceived += data.Length;
                }
            }
        }
    }
}
