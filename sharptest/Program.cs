/*
	Copyright 2012-2013 Erik Anderson

	Licensed under the Apache License, Version 2.0 (the "License");
	you may not use this file except in compliance with the License.
	You may obtain a copy of the License at

		http://www.apache.org/licenses/LICENSE-2.0

	Unless required by applicable law or agreed to in writing, software
	distributed under the License is distributed on an "AS IS" BASIS,
	WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
	See the License for the specific language governing permissions and
	limitations under the License.
*/
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
        Layout.Circuit circuit;
        EventWaitHandle canvasConstructedEvent;
        int canvasBindCount;

        void Run()
        {
            library = new ModLibrary();
            Console.Out.WriteLine("Loading modules");
#if DEBUG
            cppProxy.CppProxyModuleDriver.DoDiscovery(@"..\..\..\Debug", library);
#else
            cppProxy.CppProxyModuleDriver.DoDiscovery(@"..\..\..\Release", library);
#endif

            Console.Out.WriteLine("Building schematic");
            Layout.Schematic schem = new Layout.Schematic();
            Layout.Schematic.Element radioElem = new Layout.Schematic.Element(Layout.ElementType.Module, "radio");
            Layout.Schematic.Element frameElem = new Layout.Schematic.Element(Layout.ElementType.Module, "make frame");
            Layout.Schematic.Element fftElem = new Layout.Schematic.Element(Layout.ElementType.Module, "fft");
            Layout.Schematic.Element mag2Func = new Layout.Schematic.Element(Layout.ElementType.Function, "mag^2");
            Layout.Schematic.Element divideByN = new Layout.Schematic.Element(Layout.ElementType.Function, "divide by N");
            Layout.Schematic.Element dbFunc = new Layout.Schematic.Element(Layout.ElementType.Function, "dB");
            Layout.Schematic.Element splitterElem = new Layout.Schematic.Element(Layout.ElementType.Module, "splitter");
            Layout.Schematic.Element waterfallElem = new Layout.Schematic.Element(Layout.ElementType.Module, "waterfall");
            Layout.Schematic.Element waveformElem = new Layout.Schematic.Element(Layout.ElementType.Module, "waveform");
//            Layout.Schematic.Element chopElem = new Layout.Schematic.Element(Layout.ElementType.Function, "chop real frame");
            Layout.Schematic.Element frameMin = new Layout.Schematic.Element(Layout.ElementType.FunctionOnIn, "frame min");
            Layout.Schematic.Element identityElem = new Layout.Schematic.Element(Layout.ElementType.Module, "=");
            //            Layout.Schematic.Element frameMax = new Layout.Schematic.Element(Layout.ElementType.FunctionOnIn, "frame max");

            schem.connect(radioElem, "recv1", frameElem, 0);
//            schem.connect(radioElem, "wide", fftElem, 0);
            schem.connect(frameElem, fftElem);
            schem.connect(fftElem, divideByN);
//            schem.connect(fftElem, chopElem);
//            schem.connect(chopElem, divideByN);
            schem.connect(divideByN, mag2Func);
            schem.connect(mag2Func, dbFunc);
            schem.connect(dbFunc, splitterElem);
            schem.connect(splitterElem, waterfallElem);
            schem.connect(splitterElem, 1, waveformElem, 0);
            schem.connect(splitterElem, 2, frameMin, 0);
//            schem.connect(splitterElem, 3, frameMax, 0);
            schem.connect(frameMin, identityElem);

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
            using (circuit = options[0].construct())
            {
                Console.Out.WriteLine("Configuring circuit");
                signals.IBlock hpsdr = (signals.IBlock)circuit.Entry(radioElem);
                signals.IAttributes attrs = hpsdr.Attributes;
                signals.OnChanged evt = new signals.OnChanged(OnChanged);
                foreach (signals.IAttribute attr in attrs) attr.changed += evt;
                attrs["recvRate"].Value = 48000;
                attrs["Recv1Freq"].Value = 10000000;

                signals.IBlock ident = (signals.IBlock)circuit.Entry(identityElem);
                signals.IOutEndpoint identOut = ident.Outgoing[0];
                signals.IEPBuffer outBuff = identOut.CreateBuffer();
                identOut.Connect(outBuff);
                cppProxy.ReceiveStream stream = new cppProxy.ReceiveStream(identOut.Type, outBuff);
                stream.data += new cppProxy.ReceiveStream.OnReceive(OnStreamDetail);

                canvas = new Canvas();
                canvas.freq1.Attribute = attrs["Recv1Freq"];

                // start the canvas thread, wait for all the objects to be bound
                canvasConstructedEvent = new EventWaitHandle(false, EventResetMode.ManualReset);
                canvasBindCount = 2;
                canvas.panel1.HandleCreated += new EventHandler(panel1_HandleCreated);
                canvas.panel2.HandleCreated += new EventHandler(panel2_HandleCreated);
                Thread canvasThread = canvas.Start();
                canvasConstructedEvent.WaitOne();
                canvasConstructedEvent = null;

                Console.Out.WriteLine("Powering circuit");
                circuit.Start();
                canvasThread.Join();

                Console.Out.WriteLine("Shutting down circuit");
                circuit.Stop();
                stream.Stop();
            }
  //        Console.Out.WriteLine(String.Format("{0} received total", packetsReceived));
        }

        void panel1_HandleCreated(object sender, EventArgs e)
        {
            Layout.ElemKey waveformElem = new Layout.ElemKey(Layout.ElementType.Module, "waveform");
            List<Layout.Circuit.Element> lookup = circuit.Find(waveformElem);
            signals.IBlock waveform = (signals.IBlock)lookup[0].obj;
            waveform.Attributes["targetWindow"].Value = canvas.panel1.Handle;
            if (Interlocked.Decrement(ref canvasBindCount) == 0) canvasConstructedEvent.Set();
        }
        
        void panel2_HandleCreated(object sender, EventArgs e)
        {
            Layout.ElemKey waterfallElem = new Layout.ElemKey(Layout.ElementType.Module, "waterfall");
            List<Layout.Circuit.Element> lookup = circuit.Find(waterfallElem);
            signals.IBlock waterfall = (signals.IBlock)lookup[0].obj;
            waterfall.Attributes["targetWindow"].Value = canvas.panel2.Handle;
            if (Interlocked.Decrement(ref canvasBindCount) == 0) canvasConstructedEvent.Set();
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
        private void OnStream(Array data)
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

        private void OnStreamDetail(Array data)
        {
            if (data.Length != 0)
            {
                lock (st_screenLock)
                {
                    foreach(object value in data)
                    {
                        if (value == null)
                        {
                            Console.Out.WriteLine("received: null");
                        }
                        else
                        {
                            Console.Out.WriteLine(String.Format("received: ({0}){1}", value.GetType().Name, value));
                        }
                    }
                }
            }
        }
    }
}
