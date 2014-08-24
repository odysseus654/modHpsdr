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
using System.Drawing;
using System.Threading;
using System.Windows.Forms;

namespace sharptest
{
    class Canvas : Form
    {
        public WaveformView panel1;
        public WaveformView panel2;
        public FrequencyControl freq1;

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
            this.panel1 = new WaveformView();
            this.panel2 = new WaveformView();
            this.freq1 = new FrequencyControl();
            this.SuspendLayout();

            // panel1
            this.panel1.Name = "panel1";
            this.panel1.Location = new Point(21, 50);
            this.panel1.Size = new Size(600, 300);
            this.panel1.Anchor = AnchorStyles.Left | AnchorStyles.Right | AnchorStyles.Top | AnchorStyles.Bottom;
            this.panel1.TabIndex = 1;

            // panel2
            this.panel2.Name = "panel2";
            this.panel2.Location = new Point(21, 330);
            this.panel2.Size = new Size(600, 300);
            this.panel2.Anchor = AnchorStyles.Left | AnchorStyles.Right | AnchorStyles.Top | AnchorStyles.Bottom;
            this.panel2.TabIndex = 2;

            // freq1
            this.freq1.Name = "freq1";
            this.freq1.Location = new Point(5, 5);
            this.freq1.Size = new Size(200, 40);
            this.panel2.Anchor = AnchorStyles.Left | AnchorStyles.Top;
            this.panel2.TabIndex = 0;

            // Outer Form
            this.AutoScaleDimensions = new SizeF(6F, 13F);
            this.AutoScaleMode = AutoScaleMode.Font;
            this.ClientSize = new Size(700, 650);
            this.Controls.Add(this.panel1);
            this.Controls.Add(this.panel2);
            this.Controls.Add(this.freq1);
            this.Text = "Canvas";

            this.ResumeLayout(false);
        }
    }
}
