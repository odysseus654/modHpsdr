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
#if DEBUG
            cppProxy.CppProxyModuleDriver.DoDiscovery(@"D:\modules\hpsdr-mod\Debug", library);
#else
            cppProxy.CppProxyModuleDriver.DoDiscovery(@"D:\modules\hpsdr-mod\Release", library);
#endif

            Schematic schem = new Schematic();
            Schematic.Element fftElem = new Schematic.Element(Schematic.ElementType.Module, "fft");
            Schematic.Element radioElem = new Schematic.Element(Schematic.ElementType.Module, "radio");

            schem.addGeneric(fftElem);
            schem.add(radioElem);
            schem.connect(radioElem, "recv1", fftElem, 0);
            List<Schematic> options = schem.resolve(library);
            Circuit circuit = options[0].construct();

            signals.IBlock hpsdr = (signals.IBlock)circuit.Entry(radioElem);
            signals.IAttributes attrs = hpsdr.Attributes;
            signals.OnChanged evt = new signals.OnChanged(OnChanged);
            foreach(signals.IAttribute attr in attrs) attr.changed += evt;
            attrs.GetByName("recvRate").Value = 48000;

            signals.IBlock fft = (signals.IBlock)circuit.Entry(fftElem);
            signals.IOutEndpoint fftOut = fft.Outgoing[0];
            signals.IEPBuffer outBuff = fftOut.CreateBuffer();
            fftOut.Connect(outBuff);
            cppProxy.ReceiveStream stream = new cppProxy.ReceiveStream(fftOut.Type, outBuff);
            stream.data += new cppProxy.ReceiveStream.OnReceive(OnStream);

            Canvas canvas = new Canvas();
            circuit.Start();
            Thread canvasThread = canvas.Start();
            canvasThread.Join();
            circuit.Stop();
            stream.Stop();
            
            Console.Out.WriteLine(String.Format("{0} received total", packetsReceived));
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
