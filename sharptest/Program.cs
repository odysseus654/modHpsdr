using System;
using System.Collections.Generic;
using System.Threading;

namespace sharptest
{
    class Program
    {
        static void Main(string[] args)
        {
            (new Program()).Run();
        }

        private ModLibrary library;
        Canvas canvas;
        Thread canvasThread;
        Circuit circuit;

        void Run()
        {
            library = new ModLibrary();
#if DEBUG
            cppProxy.CppProxyModuleDriver.DoDiscovery(@"D:\modules\hpsdr-mod\Debug", library);
#else
            cppProxy.CppProxyModuleDriver.DoDiscovery(@"D:\modules\hpsdr-mod\Release", library);
#endif

            Schematic schem = new Schematic();
            Schematic.Element fftElem = new Schematic.Element(Schematic.ElementType.Module, "fft");
            Schematic.Element radioElem = new Schematic.Element(Schematic.ElementType.Module, "radio");
            Schematic.Element mag2Func = new Schematic.Element(Schematic.ElementType.Function, "mag2");
            Schematic.Element waterfallElem = new Schematic.Element(Schematic.ElementType.Module, "waterfall");

            schem.add(fftElem);
            schem.add(mag2Func);
            schem.add(waterfallElem);
            schem.add(radioElem);
            schem.connect(radioElem, "recv1", fftElem, 0);
            schem.connect(fftElem, 0, mag2Func, 0);
            schem.connect(mag2Func, 0, waterfallElem, 0);
            List<Schematic> options = schem.resolve(library);
            circuit = options[0].construct();

            signals.IBlock hpsdr = (signals.IBlock)circuit.Entry(radioElem);
            signals.IAttributes attrs = hpsdr.Attributes;
            signals.OnChanged evt = new signals.OnChanged(OnChanged);
            foreach(signals.IAttribute attr in attrs) attr.changed += evt;
            attrs["recvRate"].Value = 48000;
/*
            signals.IBlock fft = (signals.IBlock)circuit.Entry(fftElem);
            signals.IOutEndpoint fftOut = fft.Outgoing[0];
            signals.IEPBuffer outBuff = fftOut.CreateBuffer();
            fftOut.Connect(outBuff);
            cppProxy.ReceiveStream stream = new cppProxy.ReceiveStream(fftOut.Type, outBuff);
            stream.data += new cppProxy.ReceiveStream.OnReceive(OnStream);
            */
            canvas = new Canvas();
            canvas.panel1.HandleCreated += new EventHandler(panel1_HandleCreated);
            canvasThread = canvas.Start();
            canvasThread.Join();
            circuit.Stop();
//            stream.Stop();
            
//            Console.Out.WriteLine(String.Format("{0} received total", packetsReceived));
        }

        void panel1_HandleCreated(object sender, EventArgs e)
        {
            Schematic.Element waterfallElem = new Schematic.Element(Schematic.ElementType.Module, "waterfall");
            signals.IBlock waterfall = (signals.IBlock)circuit.Entry(waterfallElem);
            waterfall.Attributes["targetWindow"].Value = canvas.panel1.Handle;
            circuit.Start();
        }

        protected object st_screenLock = new object();
        private void OnChanged(string name, signals.EType type, object value)
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

        protected int packetsReceived = 0;
        private void OnStream(object[] data)
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
