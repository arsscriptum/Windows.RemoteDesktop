﻿using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Drawing;
using System.Data;
using System.Linq;
using System.Text;
using System.Windows.Forms;
using System.Diagnostics;
using Microsoft.AspNet.SignalR.Client;
using System.Net;
using RemoteDesktop_Viewer.Code;
using RemoteDesktop_CSLibrary;

namespace RemoteDesktop_Viewer
{
    public partial class ProxyClients : UserControl
    {
        public delegate void OnConnectAttemptHandler(string ip_or_host, Client c);
        public event OnConnectAttemptHandler OnConnectAttemptEvent;

        public delegate void OnDisconnectHandler();
        public event OnDisconnectHandler OnDisconnectEvent;

        private List<Client> _Clients = new List<Client>();
        public ProxyAuth _ProxyAuth = null;
        public ProxyAuth ProxyAuth
        {
            get { return _ProxyAuth; }
            set
            {
                _ProxyAuth = value;
                if (value != null)
                    StartService();
                else
                    StopService();

            }
        }
        private Microsoft.AspNet.SignalR.Client.HubConnection _Hub = null;
        private IHubProxy _ProxyHub = null;

        public ProxyClients()
        {
            InitializeComponent();
            listView1.AutoResizeColumns(ColumnHeaderAutoResizeStyle.HeaderSize);
            this.HandleDestroyed += ProxyClients_HandleDestroyed;
            contextMenuStrip1.Opening += contextMenuStrip1_Opening;
    
        }

        void contextMenuStrip1_Opening(object sender, CancelEventArgs e)
        {
            e.Cancel = listView1.SelectedIndices.Count == 0;
        }

        void ProxyClients_HandleDestroyed(object sender, EventArgs e)
        {
            StopService();
        }

        private void StartService()
        {
            try
            {

                if (_ProxyAuth != null)
                {
                    if (_ProxyAuth.Authenticated)
                    {
                        _Hub = new Microsoft.AspNet.SignalR.Client.HubConnection(RemoteDesktop_CSLibrary.Config.SignalRHubUrl);
                        _ProxyHub = _Hub.CreateHubProxy(RemoteDesktop_CSLibrary.Config.SignalRHubName);
                        _Hub.TransportConnectTimeout = new TimeSpan(0, 0, 4);
                        _ProxyHub.On<List<Client>>("AvailableClients", ReceivedClients);
                        _Hub.Error += _Hub_Error;
                        if (ProxyAuth.UsingWindowsAuth)
                        {
                            _Hub.Credentials = System.Net.CredentialCache.DefaultNetworkCredentials;
                        }
                        else
                        {
                            _Hub.CookieContainer = new CookieContainer();
                            _Hub.CookieContainer.Add(_ProxyAuth.AuthCookie);
                        }
                        _Hub.Start();
                    }
                }
            }
            catch (Exception e)
            {
                Debug.WriteLine(e.Message);
            }
        }

        void _Hub_Error(Exception obj)
        {
            if (OnDisconnectEvent != null)
                OnDisconnectEvent();
        }
        private void Fill(List<string> data, Client c)
        {
            data.Clear();
            data.Add(c.Src_ID.ToString());
            data.Add(c.Firewall_IP.ToString());
            data.Add(c.ComputerName.ToString());
            data.Add(c.UserName);
        }
        private void ReceivedClients(List<Client> clients)
        {
            _Clients = clients;
            listView1.UIThread(() =>
            {
                listView1.Items.Clear();//clear all items

                var copy = clients.Where(a => a != null).ToList();//create a copy

                foreach (var item in clients.Where(a => a != null && a.Host == Client.Host_Type.Viewer))
                {
                    copy.Remove(item);
                    var left = new List<string>() { "*", "*", "*", "*", "<------>" };
                    var right = new List<string>() { "*", "*", "*", "*" };

                    Fill(right, item);

                    var found = copy.FirstOrDefault(a => a.Src_ID == item.Dst_ID);
                    if (found != null)
                    {
                        copy.Remove(found);
                        Fill(left, found);
                    }
                    left.Add("<------>");
                    left.AddRange(right);
                    var it = listView1.Items.Add(new ListViewItem(left.ToArray()));
                }
                foreach (var item in copy)
                {
                    var left = new List<string>();
                    var right = new List<string>() { "*", "*", "*", "*" };
                    Fill(left, item);
                    left.Add("<------>");
                    left.AddRange(right);
                    var it = listView1.Items.Add(new ListViewItem(left.ToArray()));
                    if (item.Status == Client.Connection_Status.Reserved) it.BackColor = Color.Gray;
                }
                if (listView1.Items.Count > 0)
                    listView1.AutoResizeColumns(ColumnHeaderAutoResizeStyle.ColumnContent);
                else
                    listView1.AutoResizeColumns(ColumnHeaderAutoResizeStyle.HeaderSize);

            });

        }
        private void StopService()
        {
            if (_Hub != null)
                _Hub.Dispose();
            _ProxyAuth = null;
        }
        private void toolStripMenuItem1_Click(object sender, EventArgs e)
        {
            if (listView1.SelectedItems.Count > 0)
            {
                var id = listView1.SelectedItems[0].Text;
                if (id.StartsWith("*"))
                {
                    MessageBox.Show("You cannot connect to this machine, that is a viewer, not a server!");
                    return;
                }
                var intid = -1;
                if (!Int32.TryParse(id, out intid))
                {
                    MessageBox.Show("Cannot parse ID!");
                    return;
                }

                var c = _Clients.FirstOrDefault(a => a != null && a.Src_ID == intid);

                if (c != null)
                {
                    if (c.Status == Client.Connection_Status.Paired)
                    {
                        MessageBox.Show("You cannot connect to this machine, it is already paired.");
                    }
                    else
                    {
                        Debug.WriteLine("Attempting connect to " + RemoteDesktop_CSLibrary.Config.Gateway + " id: " + c.Src_ID);
                        if (OnConnectAttemptEvent != null)
                        {
                            var splits = RemoteDesktop_CSLibrary.Config.Gateway.Split(':');
                            OnConnectAttemptEvent(splits.FirstOrDefault(), c);
                        }
                    }
                }
            }
        }
    }
}
