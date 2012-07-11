using System;
using System.Threading;

namespace cppProxy
{
    public class ReceiveStream : IDisposable
    {
        public delegate void OnReceive(object[] values);
        public event OnReceive data;
        private Thread m_thread;
        private signals.EType m_type;
        private signals.IEPReceiver m_recv;

        public ReceiveStream(signals.EType type, signals.IEPReceiver recv)
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

        public void Dispose()
        {
            Dispose(true);
            GC.SuppressFinalize(this);
        }

        protected virtual void Dispose(bool disposing)
        {
            if (m_thread != null && m_thread.IsAlive)
            {
                m_thread.Abort();
                m_thread.Join();
            }
        }

        private void start()
        {
            for (; ; )
            {
                object[] buffer;
                m_recv.Read(m_type, out buffer, -1);
                if(data != null) data(buffer);
            }
        }
    }
}
