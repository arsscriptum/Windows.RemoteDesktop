﻿using RemoteDesktop_Viewer.Code;
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Windows.Forms;

namespace RemoteDesktop_Viewer
{
    public partial class MainViewer : Form
    {

        [UnmanagedFunctionPointer(CallingConvention.StdCall)]
        delegate void _EmptyFunction();

        [UnmanagedFunctionPointer(CallingConvention.StdCall)]
        delegate void _OnCursorChanged(int c_type);

        [UnmanagedFunctionPointer(CallingConvention.StdCall)]
        delegate void _OnDisplayChanged(int index, int xoffset, int yoffset, int width, int height);

        [UnmanagedFunctionPointer(CallingConvention.StdCall)]
        delegate void _OnConnectingAttempt(int attempt, int maxattempts);

        [DllImport(RemoteDesktop_CSLibrary.Config.DLL_Name, CallingConvention = CallingConvention.StdCall)]
        static extern IntPtr Create_Client(IntPtr hwnd, _EmptyFunction onconnect, _EmptyFunction ondisconnect, _OnCursorChanged oncursorchange, _OnDisplayChanged ondisplaychanged, _OnConnectingAttempt onconnectingattempt);
        [DllImport(RemoteDesktop_CSLibrary.Config.DLL_Name)]
        static extern void Destroy_Client(IntPtr client);
        [DllImport(RemoteDesktop_CSLibrary.Config.DLL_Name, CharSet = CharSet.Unicode)]
        static extern void Connect(IntPtr client, string port, string ip_or_host, int id, string aeskey);
        [DllImport(RemoteDesktop_CSLibrary.Config.DLL_Name)]
        static extern void Draw(IntPtr client, IntPtr hdc);
        [DllImport(RemoteDesktop_CSLibrary.Config.DLL_Name)]
        static extern void KeyEvent(IntPtr client, int VK, bool down);
        [DllImport(RemoteDesktop_CSLibrary.Config.DLL_Name)]
        static extern void MouseEvent(IntPtr client, int action, int x, int y, int wheel);
        [DllImport(RemoteDesktop_CSLibrary.Config.DLL_Name)]
        static extern void SendCAD(IntPtr client);
        [DllImport(RemoteDesktop_CSLibrary.Config.DLL_Name)]
        static extern void SendRemoveService(IntPtr client);
        [DllImport(RemoteDesktop_CSLibrary.Config.DLL_Name, CharSet = CharSet.Unicode)]
        static extern void ElevateProcess(IntPtr client, string username, string password);
        [DllImport(RemoteDesktop_CSLibrary.Config.DLL_Name)]
        static extern RemoteDesktop_CSLibrary.Traffic_Stats get_TrafficStats(IntPtr client);
        [DllImport(RemoteDesktop_CSLibrary.Config.DLL_Name)]
        static extern void SendSettings(IntPtr client, int img_quality, bool gray, bool shareclip);
        [DllImport(RemoteDesktop_CSLibrary.Config.DLL_Name)]
        static extern void SetOnElevateFailed(IntPtr client, _EmptyFunction func);
        [DllImport(RemoteDesktop_CSLibrary.Config.DLL_Name)]
        static extern void SetOnElevateSuccess(IntPtr client, _EmptyFunction func);



        public delegate void OnConnectHandler();
        public delegate void OnDisconnectHandler();
        public delegate void OnCursorChangedHandler(int c_type);
        public delegate void OnConnectingAttemptHandler(int attempt, int maxattempts);

        public event OnConnectHandler OnConnectEvent;
        public event OnDisconnectHandler OnDisconnectEvent;
        public event OnCursorChangedHandler OnCursorChangedEvent;
        public event OnConnectingAttemptHandler OnConnectingAttemptEvent;

        private _EmptyFunction OnConnect_CallBack;
        private _OnConnectingAttempt OnConnectingAttempt_CallBack;
        private _EmptyFunction OnDisconnect_CallBack;
        private _OnCursorChanged OnCursorChanged_CallBack;
        private _OnDisplayChanged OnDisplayChanged_CallBack;
        private _EmptyFunction OnElevateFailed_CallBack;
        private _EmptyFunction OnElevateSuccess_CallBack;

        private IntPtr _Client = IntPtr.Zero;

        private System.Timers.Timer _TrafficTimer;
        private string _Host_Address;

        private RemoteDesktop_CSLibrary.Client _Proxyd_Client = null;

        public string Host_Address { get { return _Host_Address; } }
        InputListener _InputListener = null;
        private List<FileDownload> _FileDownloadControls = new List<FileDownload>();
        private Rectangle[] _Displays = new Rectangle[4];
        private SettingsDialog _SettingsDialog = null;
        private AdminLogin _AdminLogin = null;

        public MainViewer()
        {
            InitializeComponent();
            FormClosing += MainViewer_FormClosing;
            FormClosed += MainViewer_FormClosed;
            _InputListener = new InputListener(viewPort1.Handle);
            _InputListener.InputKeyEvent = KeyEvent;
            _InputListener.InputMouseEvent = MouseEvent;
            DragDrop += new DragEventHandler(Form1_DragDrop);
            DragEnter += new DragEventHandler(Form1_DragEnter);

            Application.AddMessageFilter(_InputListener);
            viewPort1.OnDraw_CB = Draw;

            //Must keep references 
            OnConnect_CallBack = OnConnect;
            OnDisconnect_CallBack = OnDisconnect;
            OnCursorChanged_CallBack = OnCursorChanged;
            OnDisplayChanged_CallBack = OnDisplayChanged;
            OnConnectingAttempt_CallBack = OnConnectingAttempt;

            OnElevateFailed_CallBack = OnElevateFailed;
            OnElevateSuccess_CallBack = OnElevateSuccess;

            _Client = Create_Client(viewPort1.Handle, OnConnect_CallBack, OnDisconnect_CallBack, OnCursorChanged_CallBack, OnDisplayChanged_CallBack, OnConnectingAttempt_CallBack);
            SetOnElevateFailed(_Client, OnElevateFailed_CallBack);
            SetOnElevateSuccess(_Client, OnElevateSuccess_CallBack);

            button3.MouseEnter += button_MouseEnter;
            button3.MouseLeave += button_MouseLeave;

            button2.MouseEnter += button_MouseEnter;
            button2.MouseLeave += button_MouseLeave;

            button4.MouseEnter += button_MouseEnter;
            button4.MouseLeave += button_MouseLeave;

            for(var i = 0; i < _Displays.Length; i++)
                _Displays[i] = new Rectangle(0, 0, 0, 0);
        }

        void button_MouseLeave(object sender, EventArgs e)
        {
            var but = (Button)sender;
            but.Location = new Point(but.Location.X, -(but.Size.Height / 3 + but.Size.Height / 3));
        }

        void button_MouseEnter(object sender, EventArgs e)
        {
            var but = (Button)sender;
            but.Location = new Point(but.Location.X, 0);
        }


        void _TrafficTimer_Elapsed(object sender, System.Timers.ElapsedEventArgs e)
        {
            var traffic = get_TrafficStats(_Client);

            this.UIThread(() =>
            {
                if(_Proxyd_Client != null)
                {
                    this.Text = "Connected to Proxy: " + _Host_Address + ":2020 --> " + _Proxyd_Client.ComputerName + ":" + _Proxyd_Client.UserName + " Out: " + RemoteDesktop_CSLibrary.FormatBytes.Format(traffic.CompressedSendBPS) + "/s In: " + RemoteDesktop_CSLibrary.FormatBytes.Format(traffic.CompressedRecvBPS) + "/s";
                } else
                {
                    this.Text = "Connected to: " + _Host_Address + ":2020,  Out: " + RemoteDesktop_CSLibrary.FormatBytes.Format(traffic.CompressedSendBPS) + "/s In: " + RemoteDesktop_CSLibrary.FormatBytes.Format(traffic.CompressedRecvBPS) + "/s";
                }
            });
        }


        private void Form1_DragEnter(object sender, DragEventArgs e)
        {
            // If the data is a file or a bitmap, display the copy cursor. 
            if(e.Data.GetDataPresent(DataFormats.FileDrop))
                e.Effect = DragDropEffects.Copy;
            else
                e.Effect = DragDropEffects.None;

        }
        private void OnCursorChanged(int c_type)
        {
            if(OnCursorChangedEvent != null)
                OnCursorChangedEvent(c_type);
            viewPort1.UIThread(() =>
            {
                viewPort1.Cursor = CursorManager.Get_Cursor(c_type);
            });
        }
        private void OnConnectingAttempt(int attempt, int maxattempts)
        {
            Connected = false;
            if(OnConnectingAttemptEvent != null)
                OnConnectingAttemptEvent(attempt, maxattempts);
        }
        private void OnDisplayChanged(int index, int xoffset, int yoffset, int width, int height)
        {
            int BorderWidth = (Width - ClientSize.Width) / 2;
            int TitlebarHeight = Height - ClientSize.Height - 2 * BorderWidth;
            int maxwidth = Screen.PrimaryScreen.Bounds.Width - BorderWidth * 2;
            int maxheight = Screen.PrimaryScreen.Bounds.Height - TitlebarHeight - BorderWidth * 2;

            _Displays[index] = new Rectangle(xoffset, yoffset, width, height);
            int viewportwidth = _Displays.Sum(a => a.Width);
            int viewportheight = _Displays.Max(a => a.Height);

            this.UIThread(() =>
            {
                viewPort1.Size = new Size(viewportwidth, viewportheight);
                this.Size = new Size(Math.Min(maxwidth, viewportwidth + System.Windows.Forms.SystemInformation.VerticalScrollBarWidth + BorderWidth * 2), Math.Min(maxheight, viewportheight + TitlebarHeight + System.Windows.Forms.SystemInformation.HorizontalScrollBarHeight + (BorderWidth * 2)));

            });
        }
        private void Form1_DragDrop(object sender, DragEventArgs e)
        {
            var f = new FileDownload((string[])e.Data.GetData(DataFormats.FileDrop), _Client);
            f.Anchor = AnchorStyles.Top | AnchorStyles.Right;
            f.Left = this.Size.Width - f.Size.Width - 25;
            f.Top = 5;
            f.OnDoneEvent += f_OnDoneEvent;
            f.Show();
            Controls.Add(f);
            f.BringToFront();
            viewPort1.SendToBack();
            _FileDownloadControls.Add(f);
        }

        void f_OnDoneEvent(FileDownload f)
        {
            f.UIThread(() =>
            {
                f.Hide();
                Controls.Remove(f);
                _FileDownloadControls.Remove(f);
            });
        }

        public void Connect(string proxy_host, RemoteDesktop_CSLibrary.Client c)
        {
            for(var i = 0; i < _Displays.Length; i++)
                _Displays[i] = new Rectangle(0, 0, 0, 0);

            _Host_Address = proxy_host;
            _Proxyd_Client = c;
            if(c == null)
                Connect(_Client, RemoteDesktop_CSLibrary.Config.Port, proxy_host, -1, "");
            else
                Connect(_Client, RemoteDesktop_CSLibrary.Config.Port, proxy_host, c.Src_ID, c.AES_Session_Key);
        }
        static int counter = 0;
        static DateTime timer = DateTime.Now;
        public void Draw(IntPtr hdc)
        {
            if((DateTime.Now - timer).TotalMilliseconds > 1000)
            {
                Debug.WriteLine("FPS: " + counter);
                counter = 1;
                timer = DateTime.Now;
            } else
                counter += 1;
            Draw(_Client, hdc);
        }
        void KeyEvent(int VK, bool down)
        {
            KeyEvent(_Client, VK, down);
        }
        private void OnElevateFailed()
        {
            MessageBox.Show("Credentials are not not valid. Could not elevate process.");
            Debug.WriteLine("Failed to elevate");
        }
        private void OnElevateSuccess()
        {
            MessageBox.Show("Successfully elevated remote process. It will now restart under the new credentials.");

            this.UIThread(() =>
            {
                if(_AdminLogin != null)
                    _AdminLogin.Close();
                _AdminLogin = null;
            });
            Debug.WriteLine("Successfully elevated");
        }



        private bool Connected = false;
        private void OnConnect()
        {
            Debug.WriteLine("Onconnect in viewer");
            this.UIThread(() => { this.Text = "Connected to: " + _Host_Address + ":2020"; });
            Connected = true;
            if(OnConnectEvent != null)
                OnConnectEvent();

            StopTrafficTimer();
            _TrafficTimer = new System.Timers.Timer(1000);
            _TrafficTimer.Elapsed += _TrafficTimer_Elapsed;
            Debug.WriteLine(" _TrafficTimer.Start();");
            _TrafficTimer.Start();
        }
        private void OnDisconnect()
        {

            StopTrafficTimer();
            if(OnDisconnectEvent != null)
                OnDisconnectEvent();
            Debug.WriteLine("OnDisconnect in viewer");
        }
        private static DateTime MouseThrottle = DateTime.Now;
        //when transferring files, limit mouse messages they cause severe congestion!
        void MouseEvent(int action, int x, int y, int wheel)
        {
            if((action == InputListener.WM_MOUSEWHEEL || action == InputListener.WM_MOUSEMOVE) && _FileDownloadControls.Any())
            {//limit mouse move messages to 10 per second
                if((DateTime.Now - MouseThrottle).Milliseconds > 100)
                {
                    MouseEvent(_Client, action, x, y, wheel);
                    MouseThrottle = DateTime.Now;
                }
            } else
                MouseEvent(_Client, action, x, y, wheel);
        }

        void StopTrafficTimer()
        {
            if(_TrafficTimer != null)
            {
                _TrafficTimer.Stop();
                _TrafficTimer.Dispose();
            }
        }
        void MainViewer_FormClosed(object sender, FormClosedEventArgs e)
        {
            if(_SettingsDialog != null)
                _SettingsDialog.Close();
            _SettingsDialog = null;
            if(_AdminLogin != null)
                _AdminLogin.Close();
            _AdminLogin = null;


            StopTrafficTimer();

            foreach(var item in _FileDownloadControls)
                item.Running = false;
            _FileDownloadControls.Clear();
            if(_Client != IntPtr.Zero)
                Destroy_Client(_Client);
            _Client = IntPtr.Zero;
            viewPort1.OnDraw_CB = null;
            Application.RemoveMessageFilter(_InputListener);
        }
        bool ClosedCalled = false;
        void MainViewer_FormClosing(object sender, FormClosingEventArgs e)
        {
            if(!ClosedCalled)
            {
                if(Connected)
                {
                    var result = MessageBox.Show("Remove Service?", "Do you want to uninstall the service from the users machine?", MessageBoxButtons.YesNo);
                    if(result == System.Windows.Forms.DialogResult.Yes)
                    {
                        SendRemoveService(_Client);
                        System.Threading.Thread.Sleep(1000);//give time for the message to go!
                    }
                    Connected = false;
                }
                ClosedCalled = true;
                OnDisconnect();
            }
        }

        private void button3_Click(object sender, EventArgs e)
        {
            SendCAD(_Client);
        }


        private void button2_Click(object sender, EventArgs e)
        {
            if(_SettingsDialog != null)
                _SettingsDialog.Close();
            _SettingsDialog = new SettingsDialog();
            _SettingsDialog.OnSettingsChangedEvent += OnSettingsChanged;
            _SettingsDialog.ShowDialog(this);
        }
        private void OnSettingsChanged(RemoteDesktop_CSLibrary.Settings_Header h)
        {
            SendSettings(_Client, h.Image_Quality, h.GrayScale, h.ShareClip);
        }

        private void button4_Click(object sender, EventArgs e)
        {
            if(_AdminLogin != null)
                _AdminLogin.Close();
            _AdminLogin = null;

            _AdminLogin = new AdminLogin();
            _AdminLogin.OnLoginEvent += f_OnLoginEvent;
            _AdminLogin.ShowDialog(this);

        }

        void f_OnLoginEvent(string username, string password)
        {
            ElevateProcess(_Client, username, password);
        }

        private void button1_Click(object sender, EventArgs e)
        {

        }
    }
}
