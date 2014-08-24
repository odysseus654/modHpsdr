using System;
using System.Windows.Forms;

namespace sharptest
{
    class WaveformView : Control
    {
        private signals.IBlock mBoundBlock;
        private signals.IAttribute mBoundAttr;

        public WaveformView()
        {
            SetStyle(ControlStyles.Opaque | ControlStyles.AllPaintingInWmPaint, true);
        }

        protected override void Dispose(bool disposing)
        {
            WaveformBlock = null; // propery-set, detaches from attribute
            base.Dispose(disposing);
        }

        public virtual signals.IBlock WaveformBlock
        {
            get { return mBoundBlock; }
            set
            {
                if (value != mBoundBlock)
                {
                    if (mBoundAttr != null && IsHandleCreated) mBoundAttr.Value = null;
                    mBoundBlock = value;
                    mBoundAttr = null;
                    if(mBoundBlock != null) mBoundAttr = mBoundBlock.Attributes["targetWindow"];
                    if (mBoundAttr != null && IsHandleCreated) mBoundAttr.Value = Handle;
                }
            }
        }

        protected override void OnHandleCreated(EventArgs e)
        {
            if (mBoundAttr != null) mBoundAttr.Value = Handle;
            base.OnHandleCreated(e);
        }
    }
}
