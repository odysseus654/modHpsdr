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
        Layout.Circuit circuit;

        void Run()
        {
            library = new ModLibrary();
            Console.Out.WriteLine("Loading modules");
#if DEBUG
            cppProxy.CppProxyModuleDriver.DoDiscovery(@"D:\modules\hpsdr-mod\Debug", library);
#else
            cppProxy.CppProxyModuleDriver.DoDiscovery(@"D:\modules\hpsdr-mod\Release", library);
#endif

            Console.Out.WriteLine("Building schematic");
            Layout.Schematic schem = new Layout.Schematic();
            Layout.Schematic.Element radioElem = new Layout.Schematic.Element(Layout.ElementType.Module, "radio");
            Layout.Schematic.Element frameElem = new Layout.Schematic.Element(Layout.ElementType.Module, "make frame");
            Layout.Schematic.Element fftElem = new Layout.Schematic.Element(Layout.ElementType.Module, "fft");
            Layout.Schematic.Element mag2Func = new Layout.Schematic.Element(Layout.ElementType.Function, "mag^2");
            Layout.Schematic.Element dbFunc = new Layout.Schematic.Element(Layout.ElementType.Function, "dB");
            Layout.Schematic.Element waterfallElem = new Layout.Schematic.Element(Layout.ElementType.Module, "waveform");

            schem.add(fftElem);
            schem.add(frameElem);
            schem.add(mag2Func);
            schem.add(dbFunc);
            schem.add(waterfallElem);
            schem.add(radioElem);
            schem.connect(radioElem, "recv1", frameElem, 0);
            schem.connect(frameElem, 0, fftElem, 0);
            schem.connect(fftElem, 0, mag2Func, 0);
            schem.connect(mag2Func, 0, dbFunc, 0);
            schem.connect(dbFunc, 0, waterfallElem, 0);

            Console.Out.WriteLine("Resolving schematic");
            List<Layout.Schematic> options;
            try
            {
                options = schem.resolve(library);
            }
            catch (Layout.Schematic.ResolveFailure fail)
            {
                Console.Out.WriteLine("Schematic could not be resolved:");
                foreach (KeyValuePair<Layout.Schematic.ResolveFailureReason, bool> entry in fail.reasons)
                {
                    Console.Out.WriteLine("\t{0}", entry.Key.Message);
                }
                return;
            }

            Console.Out.WriteLine("Building circuit");
            circuit = options[0].construct();
            using (circuit)
            {
                Console.Out.WriteLine("Configuring circuit");
                signals.IBlock hpsdr = (signals.IBlock)circuit.Entry(radioElem);
                signals.IAttributes attrs = hpsdr.Attributes;
                signals.OnChanged evt = new signals.OnChanged(OnChanged);
                foreach (signals.IAttribute attr in attrs) attr.changed += evt;
                attrs["recvRate"].Value = 48000;
                attrs["Recv1Freq"].Value = 10000000;

  //            signals.IBlock fft = (signals.IBlock)circuit.Entry(fftElem);
  //            signals.IOutEndpoint fftOut = fft.Outgoing[0];
  //            signals.IEPBuffer outBuff = fftOut.CreateBuffer();
  //            fftOut.Connect(outBuff);
  //            cppProxy.ReceiveStream stream = new cppProxy.ReceiveStream(fftOut.Type, outBuff);
  //            stream.data += new cppProxy.ReceiveStream.OnReceive(OnStream);

                canvas = new Canvas();
                canvas.panel1.HandleCreated += new EventHandler(panel1_HandleCreated);

                Console.Out.WriteLine("Powering circuit");
                canvasThread = canvas.Start();
                canvasThread.Join();
                Console.Out.WriteLine("Shutting down circuit");
                circuit.Stop();
  //            stream.Stop();

  //            Console.Out.WriteLine(String.Format("{0} received total", packetsReceived));
            }
        }

        void panel1_HandleCreated(object sender, EventArgs e)
        {
            Layout.ElemKey waterfallElem = new Layout.ElemKey(Layout.ElementType.Module, "waveform");
            List<Layout.Circuit.Element> lookup = circuit.Find(waterfallElem);
            signals.IBlock waterfall = (signals.IBlock)lookup[0].obj;
            waterfall.Attributes["targetWindow"].Value = canvas.panel1.Handle;
            circuit.Start();
        }

        protected object st_screenLock = new object();
        private void OnChanged(signals.IAttribute attr, object value)
        {
            lock(st_screenLock)
            {
                if (value == null)
                {
                    Console.Out.WriteLine(String.Format("{0}: null", attr.Name));
                }
                else
                {
                    Console.Out.WriteLine(String.Format("{0}: ({1}){2}", attr.Name, value.GetType().Name, value));
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
