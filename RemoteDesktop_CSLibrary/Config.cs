﻿using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;

namespace RemoteDesktop_CSLibrary
{
    public static class Config
    {

        public const string DLL_Name = "RemoteDesktopViewer_Library_64.dll";


        public static string Port { get { return "2020"; } }
        public static string Gateway { get { return "localhost"; } }
        public static string AuthenticationUrl { get { return "http://localhost:3406/Support/Authenticate"; } }
        public static string SignalRHubName { get { return "ProxyHub"; } }
        public static string SignalRHubUrl { get { return "http://localhost:3406"; } }
        public static int AuthenticationTimeout { get { return 10000; } }//in ms


    }
    [StructLayout(LayoutKind.Sequential)]
    public struct Traffic_Stats
    {
        public long CompressedSendBytes;
        public long CompressedRecvBytes;//overall lifetime totals 
        public long UncompressedSendBytes;

        public long UncompressedRecvBytes;//overall lifetime totals 

        public long CompressedSendBPS;
        public long CompressedRecvBPS;
        public long UncompressedSendBPS;
        public long UncompressedRecvBPS;
    }
    [StructLayout(LayoutKind.Sequential, Pack = 1)]
    public struct Settings_Header
    {
        public int Image_Quality;
        public bool GrayScale;
        public bool ShareClip;
    }
}
