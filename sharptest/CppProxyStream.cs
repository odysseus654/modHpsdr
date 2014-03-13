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
using System.Threading;

namespace cppProxy
{
    public class ReceiveStream : IDisposable
    {
        public delegate void OnReceive(Array values);
        public event OnReceive data;
        private Thread m_thread;
        private signals.EType m_type;
        private signals.IEPRecvFrom m_recv;

        public ReceiveStream(signals.EType type, signals.IEPRecvFrom recv)
        {
            m_type = type;
            m_recv = recv;
            m_thread = new Thread(new ThreadStart(start));
            m_thread.IsBackground = true;
            m_thread.Start();
        }

        ~ReceiveStream()
        {
            Dispose(false);
        }

        public void Stop()
        {
            if (m_thread != null && m_thread.IsAlive)
            {
                m_thread.Abort();
                m_thread.Join();
            }
        }

        public void Dispose()
        {
            Dispose(true);
            GC.SuppressFinalize(this);
        }

        protected virtual void Dispose(bool disposing)
        {
            Stop();
        }

        private void start()
        {
            for (; ; )
            {
                Array buffer;
                m_recv.Read(m_type, out buffer, false, 1000);
                if(data != null && buffer.Length > 0) data(buffer);
            }
        }
    }
}
