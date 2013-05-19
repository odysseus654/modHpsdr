using System;
using System.Drawing;
using System.Threading;
using System.Windows.Forms;

namespace sharptest
{
    class Canvas : Form
    {
        public Control panel1;

        public Canvas()
        {
            InitializeForm();
        }

        public Thread Start()
        {
            Thread newThread = new Thread(ThreadMain);
            newThread.Start();
            return newThread;
        }

        private void ThreadMain()
        {
            Application.EnableVisualStyles();
            Application.SetCompatibleTextRenderingDefault(false);
            Application.Run(this);
        }

        private void InitializeForm()
        {
            this.panel1 = new Control();
            this.SuspendLayout();

            // panel1
            this.panel1.Location = new Point(21, 129);
            this.panel1.Size = new Size(200, 100);
            this.panel1.Anchor = AnchorStyles.Left | AnchorStyles.Right | AnchorStyles.Top | AnchorStyles.Bottom;
            this.panel1.TabIndex = 0;

            // Outer Form
            this.AutoScaleDimensions = new SizeF(6F, 13F);
            this.AutoScaleMode = AutoScaleMode.Font;
            this.ClientSize = new Size(284, 262);
            this.Controls.Add(this.panel1);
            this.Text = "Canvas";

            this.ResumeLayout(false);
        }
    }
}
