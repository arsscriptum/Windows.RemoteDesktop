#ifndef NETWORKCLIENT123_H
#define NETWORKCLIENT123_H
#include "INetwork.h"
#include <memory>
#include <thread>

namespace RemoteDesktop{
	class SocketHandler;
	class DesktopMonitor;

	class Network_Client : public INetwork{

		bool _ShouldDisconnect = false;
		void _Run_Standard(int dst_id, std::wstring aeskey); 
		void _Run_Gateway(std::wstring gatewayurl);
		void _Run(std::shared_ptr<SocketHandler>& socket);
		std::wstring _Dst_Host, _Dst_Port;
		std::thread _BackgroundWorker;
		int MaxConnectAttempts = DEFAULTMAXCONNECTATTEMPTS;
		void Setup(std::wstring port, std::wstring host);

		std::weak_ptr<SocketHandler> _Socket;
		std::unique_ptr<DesktopMonitor> _DesktopMonitor;

		void _HandleViewerDisconnect(std::weak_ptr<SocketHandler>& ptr);

		void _HandleConnect(std::shared_ptr<SocketHandler>& ptr);
		void _HandleDisconnect(std::shared_ptr<SocketHandler>& ptr);
		void _HandleReceive(Packet_Header* p, const char* d, std::shared_ptr<SocketHandler>& ptr);

	public:
		Network_Client();

		virtual ~Network_Client();	
		virtual void Start(std::wstring port, std::wstring host) override;
		void Start(std::wstring port, std::wstring host, int id, std::wstring aeskey);
		void Start(std::wstring port, std::wstring host, std::wstring gatewayurl);

		virtual void Stop(bool blocking = false)override;
		virtual void Set_RetryAttempts(int num_of_retry)override { MaxConnectAttempts = num_of_retry; }
		virtual int Get_RetryAttempts(int num_of_retry) const override { return MaxConnectAttempts; }
		virtual RemoteDesktop::Network_Return Send(RemoteDesktop::NetworkMessages m, const RemoteDesktop::NetworkMsg& msg, Auth_Types to_which_type) override;
		virtual int Connection_Count() const override { return 1; }

		std::function<void(int)> OnGatewayConnected;
	};

}

#endif