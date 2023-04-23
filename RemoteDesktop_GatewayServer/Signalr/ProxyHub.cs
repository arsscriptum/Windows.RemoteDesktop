﻿using System;
using System.Collections.Generic;
using System.Linq;
using System.Web;
using Microsoft.AspNet.SignalR;
using System.Threading.Tasks;

namespace RemoteDesktop_GatewayServer.Signalr
{
    [AuthorizeClaims]
    public class ProxyHub : Hub
    {
        ProxyWatcher _ProxyWatcher;
        public ProxyHub() : this(ProxyWatcher.Instance) { }
        public ProxyHub(ProxyWatcher i)
        {
            _ProxyWatcher = i;
        }
        public override Task OnConnected()
        {
            return Clients.Caller.AvailableClients(RemoteDesktop_GatewayServer.Signalr.ProxyWatcher._GatewayServer.ClientManager.Clients);
        }
    }
}