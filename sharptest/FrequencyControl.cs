using System;
using System.Drawing;
using System.Windows.Forms;

namespace sharptest
{
    class FrequencyControl : Control
    {
        private enum FocusState { None, General, Char, Insert /* , Block */ }
        private long mValue;
        private Color mFocusBackColor;
        private Color mEditColor;
        private FocusState mFocusState;
        private int mFocusPos;
        private Font mMinorFont;
        private SizeF mMinorSize;
        private SizeF mMajorSize;
        private signals.IAttribute mBoundAttr;
        private Timer mBlinkTimer;
        private bool mBlinkOn;
        private bool mIsEditing;
        private long mDefIncr;
        private int mMaxDigits;

        public FrequencyControl()
        {
            mDefIncr = 1000;
            mMaxDigits = 8;
            mIsEditing = false;
            mFocusState = FocusState.None;
            mFocusBackColor = Color.DarkGray;
            mEditColor = Color.White;

            mBlinkTimer = new Timer();
            mBlinkTimer.Tick += OnBlinkTick;

            SetStyle(ControlStyles.ResizeRedraw | ControlStyles.DoubleBuffer | ControlStyles.AllPaintingInWmPaint |
                ControlStyles.UserPaint | ControlStyles.Selectable | ControlStyles.StandardClick, true);

            Value = 10000000;
            BackColor = Color.Black;
            ForeColor = Color.Yellow;
        }

        protected override void Dispose(bool disposing)
        {
            Attribute = null; // propery-set, detaches from attribute
            CursorOff();
            mBlinkTimer.Dispose();
            base.Dispose(disposing);
        }

        public virtual signals.IAttribute Attribute
        {
            get { return mBoundAttr; }
            set
            {
                if (value != mBoundAttr)
                {
                    if (mBoundAttr != null) mBoundAttr.changed -= new signals.OnChanged(OnAttrValueChanged);
                    mBoundAttr = value;
                    if (mBoundAttr != null) mBoundAttr.changed += new signals.OnChanged(OnAttrValueChanged);
                    OnAttributeChanged(EventArgs.Empty);
                }
            }
        }

        public event EventHandler AttributeChanged;
        protected virtual void OnAttributeChanged(EventArgs e)
        {
            if (mBoundAttr != null) OnAttrValueChanged(mBoundAttr, mBoundAttr.Value);
            if (AttributeChanged != null) AttributeChanged(this, e);
        }

        public override string Text
        {
            get { return mValue.ToString(); }
            set
            {
                if (ReadOnly) return;
                string newText = value;
                if (mIsEditing)
                {
                    if (newText.Length > 0)
                    {
                        newText = Int64.Parse(newText).ToString();
                    }
                }
                else
                {
                    long newVal = Int64.Parse(value);
                    newText = newVal.ToString();
                    if (newText.Length > mMaxDigits) throw new ArgumentOutOfRangeException();

                    if (newVal != mValue)
                    {
                        long oldVal = mValue;
                        try
                        {
                            mValue = newVal;
                            OnValueChanged(EventArgs.Empty);
                        }
                        catch (ArgumentException)
                        {
                            mValue = oldVal;
                            throw;
                        }
                    }
                }
                base.Text = newText;
                Invalidate();
            }
        }

        public virtual long Value
        {
            get { return mValue; }
            set
            {
                if (value != mValue && !ReadOnly)
                {
                    string newText = value.ToString();
                    if (newText.Length > mMaxDigits) throw new ArgumentOutOfRangeException();

                    long oldVal = mValue;
                    try
                    {
                        mValue = value;
                        OnValueChanged(EventArgs.Empty);
                    }
                    catch (ArgumentException)
                    {
                        mValue = oldVal;
                        throw;
                    }
                    mIsEditing = false;
                    base.Text = newText;
                    Invalidate();
                }
            }
        }

        public event EventHandler ValueChanged;
        protected virtual void OnValueChanged(EventArgs e)
        {
            if (mBoundAttr != null) mBoundAttr.Value = mValue;
            if (ValueChanged != null) ValueChanged(this, e);
        }

        public virtual bool Editing
        {
            get { return mIsEditing; }
            set
            {
                if (mIsEditing != value)
                {
                    if (value)
                    {
                        mIsEditing = true;
                    }
                    else
                    {
                        long newVal = Int64.Parse(base.Text);
                        long oldVal = mValue;
                        try
                        {
                            mValue = newVal;
                            OnValueChanged(EventArgs.Empty);
                        }
                        catch (ArgumentException)
                        {
                            mValue = oldVal;
                            throw;
                        }
                        base.Text = Text;
                        mIsEditing = false;
                        if (!mIsEditing) Text = Text;
                    }
                    OnEditingChanged(EventArgs.Empty);
                }
            }
        }

        public event EventHandler EditingChanged;
        protected virtual void OnEditingChanged(EventArgs e)
        {
            Invalidate();
            if (EditingChanged != null) EditingChanged(this, e);
        }

        public bool ReadOnly
        {
            get { return mBoundAttr != null && mBoundAttr.isReadOnly; }
        }

        public virtual Color FocusBackColor
        {
            get { return mFocusBackColor; }
            set { mFocusBackColor = value; OnFocusBackColorChanged(EventArgs.Empty); }
        }

        public event EventHandler FocusBackColorChanged;
        protected virtual void OnFocusBackColorChanged(EventArgs e)
        {
            Invalidate();
            if (FocusBackColorChanged != null) FocusBackColorChanged(this, e);
        }

        public virtual Color EditColor
        {
            get { return mEditColor; }
            set { mEditColor = value; OnEditColorChanged(EventArgs.Empty); }
        }

        public event EventHandler EditColorChanged;
        protected virtual void OnEditColorChanged(EventArgs e)
        {
            Invalidate();
            if (EditColorChanged != null) EditColorChanged(this, e);
        }

        public virtual long DefaultIncrement
        {
            get { return mDefIncr; }
            set { mDefIncr = value; }
        }

        public virtual int MaxDigits
        {
            get { return mMaxDigits; }
            set { mMaxDigits = value; }
        }

        protected override void OnPaintBackground(PaintEventArgs pevent)
        {
            pevent.Graphics.FillRectangle(
                new SolidBrush(Focused && mFocusState == FocusState.General ? FocusBackColor : BackColor),
                pevent.ClipRectangle);
        }

        protected override void OnPaint(PaintEventArgs e)
        {
            base.OnPaint(e);
            Graphics g = e.Graphics;
            Brush b = new SolidBrush(mIsEditing ? EditColor : ForeColor);

            int blinkWidth = Math.Max(2, SystemInformation.CaretWidth);
            FocusState focusState = Focused ? mFocusState : FocusState.None;
            string text = Text;
            int focusPos = Math.Min(text.Length-1, mFocusPos);

            StringFormat sf = (StringFormat)StringFormat.GenericTypographic.Clone();
            sf.Alignment = StringAlignment.Center;
            sf.LineAlignment = StringAlignment.Center;

            PointF lastPos = new PointF(ClientRectangle.Right - Margin.Right,
                (ClientRectangle.Height - Margin.Vertical + mMajorSize.Height) / 2 + ClientRectangle.Top + Margin.Top);
            if (mBlinkOn && focusState == FocusState.Insert && focusPos < 0)
            {
                RectangleF focusRect = new RectangleF(lastPos.X - 1.5f, lastPos.Y - mMinorFont.Height + 4,
                    blinkWidth, mMinorFont.Height - 8);
                g.FillRectangle(new SolidBrush(mEditColor), focusRect);
            }

            Font thisFont = mMinorFont;
            SizeF thisSize = mMinorSize;
            for (int idx = text.Length - 1, cnt = 0; idx >= 0; idx--, cnt++)
            {
                if(cnt == 3)
                {
                    thisFont = Font;
                    thisSize = mMajorSize;
                }
                RectangleF thisRect = new RectangleF(lastPos.X - thisSize.Width, lastPos.Y - thisSize.Height,
                    thisSize.Width, thisSize.Height);
                g.DrawString(text.Substring(idx, 1), thisFont, b, thisRect, sf);

                if (mBlinkOn && focusPos == cnt)
                {
                    switch (focusState)
                    {
                        case FocusState.Char:
                            {
                                RectangleF focusRect = new RectangleF(thisRect.X, thisRect.Bottom - 5, thisRect.Width, blinkWidth);
                                g.FillRectangle(new SolidBrush(mEditColor), focusRect);
                            }
                            break;
                        case FocusState.Insert:
                            {
                                RectangleF focusRect = new RectangleF(thisRect.X - 1.5f, thisRect.Y + 4, blinkWidth, thisRect.Height - 8);
                                g.FillRectangle(new SolidBrush(mEditColor), focusRect);
                            }
                            break;
                    }
                }
                
                lastPos.X = thisRect.Left;
                if (cnt % 3 == 2) lastPos.X -= mMajorSize.Width / 3;
            }
        }

        private int FocusHitTest(Point point)
        {
            float lastPos = ClientRectangle.Right - Margin.Right;
            if (point.X > lastPos) return int.MinValue;
            float thisSize = mMinorSize.Width;
            for (int idx = Text.Length - 1, cnt = 0; idx >= 0; idx--, cnt++)
            {
                if(cnt == 3) thisSize = mMajorSize.Width;
                lastPos -= thisSize;
                if (point.X >= lastPos) return cnt;
                if (cnt % 3 == 2) lastPos -= mMajorSize.Width / 3;
            }
            return int.MaxValue;
        }

        protected override void OnSizeChanged(EventArgs e)
        {
            float height = ClientRectangle.Height - Margin.Vertical;
            Font = new Font(FontFamily.GenericSansSerif, height, FontStyle.Regular, GraphicsUnit.Pixel);
            mMinorFont = new Font(FontFamily.GenericSansSerif, height * 0.6f, FontStyle.Regular, GraphicsUnit.Pixel);
            PointF origin = new PointF(0, 0);
            using(Graphics g = CreateGraphics())
            {
                mMinorSize = g.MeasureString("0", mMinorFont, origin, StringFormat.GenericTypographic);
                mMajorSize = g.MeasureString("0", Font, origin, StringFormat.GenericTypographic);
                mMinorSize.Height += 4;
            }
            base.OnSizeChanged(e);
        }

        private void OnAttrValueChanged(signals.IAttribute attr, object value)
        {
            if (attr == mBoundAttr)
            {
                Value = Convert.ToInt64(value);
            }
        }

        protected override void OnTextChanged(EventArgs e)
        {
            Invalidate();
            base.OnTextChanged(e);
        }

        protected override void OnForeColorChanged(EventArgs e)
        {
            Invalidate();
            base.OnForeColorChanged(e);
        }

        protected override void OnBackColorChanged(EventArgs e)
        {
            Invalidate();
            base.OnBackColorChanged(e);
        }

        protected override void OnGotFocus(EventArgs e)
        {
            if(mFocusState != FocusState.General) CursorOn();
            Invalidate();
            base.OnGotFocus(e);
        }

        protected override void OnLostFocus(EventArgs e)
        {
            CursorOff();
            Invalidate();
            base.OnLostFocus(e);
        }

        private void CursorOff()
        {
            if(mBlinkTimer.Enabled) mBlinkTimer.Stop();
        }

        private void CursorOn()
        {
            if (mBlinkTimer.Enabled) mBlinkTimer.Stop();
            mBlinkTimer.Interval = SystemInformation.CaretBlinkTime;
            mBlinkOn = true;
            mBlinkTimer.Start();
        }

        private void OnBlinkTick(object o, EventArgs e)
        {
            mBlinkOn = !mBlinkOn;
            Invalidate();
        }

        protected override void OnMouseDown(MouseEventArgs e)
        {
            base.OnMouseDown(e);
            if (!Focused && CanFocus) Focus();
        }

        protected override void OnMouseClick(MouseEventArgs e)
        {
            base.OnMouseClick(e);
            switch (mFocusState)
            {
                case FocusState.General:
                    {
                        int hitTest = FocusHitTest(e.Location);
                        if (hitTest == int.MinValue || hitTest == int.MaxValue)
                        {
                            mFocusState = FocusState.Insert;
                        }
                        else
                        {
                            mFocusState = FocusState.Char;
                        }
                        mFocusPos = hitTest;
                        Invalidate();
                    }
                    break;
                case FocusState.Insert:
                case FocusState.Char:
                    {
                        int hitTest = FocusHitTest(e.Location);
                        if (hitTest == mFocusPos)
                        {
                            mFocusState = FocusState.General;
                        }
                        else if (hitTest == int.MinValue || hitTest == int.MaxValue)
                        {
                            mFocusState = FocusState.Insert;
                        }
                        else
                        {
                            mFocusState = FocusState.Char;
                        }
                        mFocusPos = hitTest;
                        Invalidate();
                    }
                    break;
                default:
                    mFocusState = FocusState.General;
                    Invalidate();
                    break;
            }
            if (mFocusState != FocusState.General) CursorOn();
        }

        protected override bool IsInputKey(Keys keyData)
        {
            switch (keyData)
            {
                case Keys.Left:
                case Keys.Right:
                case Keys.Insert:
                    if (mFocusState == FocusState.Insert || mFocusState == FocusState.Char) return true;
                    break;
                case Keys.Up:
                case Keys.Down:
                case Keys.Delete:
                case Keys.Back:
//                case Keys.Enter:
//                case Keys.Escape:
                    return true;
            }
            return base.IsInputKey(keyData);
        }

        protected override void OnKeyDown(KeyEventArgs e)
        {
            int len = Text.Length;
            switch (e.KeyData)
            {
                case Keys.Insert:
                    if (mFocusState == FocusState.Char)
                    {
                        mFocusState = FocusState.Insert;
                        Invalidate();
                        CursorOn();
                        e.Handled = true;
                    }
                    else if (mFocusState == FocusState.Insert)
                    {
                        mFocusState = FocusState.Char;
                        if (mFocusPos < 0)
                        {
                            mFocusPos = 0;
                        }
                        else if (mFocusPos > len)
                        {
                            mFocusPos = len;
                        }
                        Invalidate();
                        CursorOn();
                        e.Handled = true;
                    }
                    break;

                case Keys.Left:
                    if (mFocusState != FocusState.Char && mFocusState == FocusState.Insert) break;
                    if (mFocusPos < len-1) mFocusPos++;
                    Invalidate();
                    CursorOn();
                    e.Handled = true;
                    break;

                case Keys.Right:
                    if (mFocusState == FocusState.Char)
                    {
                        if (mFocusPos > 0) mFocusPos--;
                    }
                    else if (mFocusState == FocusState.Insert)
                    {
                        if (mFocusPos >= 0) mFocusPos--;
                    }
                    else break;
                    Invalidate();
                    CursorOn();
                    e.Handled = true;
                    break;

                case Keys.Up:
                    {
                        long off = mDefIncr;
                        if (mFocusState == FocusState.Char)
                        {
                            int pos = Math.Min(Math.Max(mFocusPos, 0), len - 1);
                            off = (long)Math.Pow(10, pos);
                        }
                        try
                        {
                            Value += off;
                        }
                        catch (ArgumentException) {}
                        Invalidate();
                        CursorOn();
                        e.Handled = true;
                    }
                    break;

                case Keys.Down:
                    {
                        long off = mDefIncr;
                        if (mFocusState == FocusState.Char)
                        {
                            int pos = Math.Min(Math.Max(mFocusPos, 0), len - 1);
                            off = (long)Math.Pow(10, pos);
                        }
                        try
                        {
                            Value -= off;
                        }
                        catch (ArgumentException) {}
                        Invalidate();
                        CursorOn();
                        e.Handled = true;
                    }
                    break;

                case Keys.Delete:
                    {
                        switch (mFocusState)
                        {
                            case FocusState.Char:
                            case FocusState.Insert:
                                if(mFocusPos >= 0)
                                {
                                    string text = Text;
                                    int pos = text.Length - Math.Min(mFocusPos, text.Length - 1);
                                    text = text.Substring(0, pos) + text.Substring(pos + 1);
                                    try
                                    {
                                        Text = text;
                                    }
                                    catch (ArgumentException)
                                    {
                                        Editing = true;
                                        Text = text;
                                    }
                                    Invalidate();
                                    mFocusState = FocusState.Insert;
                                    mFocusPos = pos - 1;
                                    e.Handled = true;
                                }
                                break;
                            case FocusState.General:
                                Editing = true;
                                Text = "";
                                mFocusState = FocusState.Insert;
                                mFocusPos = -1;
                                Invalidate();
                                e.Handled = true;
                                break;
                        }
                    }
                    break;
            }
            base.OnKeyDown(e);
        }

        protected override void OnKeyPress(KeyPressEventArgs e)
        {
            char ch = e.KeyChar;
            switch (ch)
            {
                case '0':
                case '1':
                case '2':
                case '3':
                case '4':
                case '5':
                case '6':
                case '7':
                case '8':
                case '9':
                    {
                        switch (mFocusState)
                        {
                            case FocusState.Char:
                            case FocusState.Insert:
                                {
                                    string text = Text;
                                    int pos = text.Length - Math.Min(Math.Max(mFocusPos, 0), text.Length-1);
                                    text = text.Substring(0, pos) + ch + text.Substring(pos + mFocusState == FocusState.Char ? 1 : 0);
                                    if (text.Length <= mMaxDigits)
                                    {
                                        try
                                        {
                                            Text = text;
                                        }
                                        catch (ArgumentException)
                                        {
                                            Editing = true;
                                            Text = text;
                                        }
                                    }
                                    Invalidate();
                                    e.Handled = true;
                                }
                                break;
                            case FocusState.General:
                                Editing = true;
                                Text = ch.ToString();
                                mFocusState = FocusState.Insert;
                                mFocusPos = -1;
                                Invalidate();
                                e.Handled = true;
                                break;
                        }
                    }
                    break;

                case (char)8: // backspace
                    {
                        switch (mFocusState)
                        {
                            case FocusState.Char:
                            case FocusState.Insert:
                                {
                                    string text = Text;
                                    int pos = text.Length - Math.Min(Math.Max(mFocusPos, 0), text.Length - 1);
                                    text = text.Substring(0, pos - 1) + text.Substring(pos);
                                    try
                                    {
                                        Text = text;
                                    }
                                    catch (ArgumentException)
                                    {
                                        Editing = true;
                                        Text = text;
                                    }
                                    mFocusPos = pos;
                                    mFocusState = FocusState.Insert;
                                    Invalidate();
                                    e.Handled = true;
                                }
                                break;
                            case FocusState.General:
                                Editing = true;
                                Text = "";
                                mFocusState = FocusState.Insert;
                                mFocusPos = -1;
                                Invalidate();
                                e.Handled = true;
                                break;
                        }
                    }
                    break;

                case (char)13: // enter
                    if (Editing)
                    {
                        try
                        {
                            Editing = false;
                        }
                        catch (ArgumentException) { }
                        e.Handled = true;
                    }
                    break;

                case (char)27: // escape
                    if (Editing)
                    {
                        Text = mValue.ToString();
                        Editing = false;
                        e.Handled = true;
                    }
                    break;

            }
            
            base.OnKeyPress(e);
        }
    }
}
