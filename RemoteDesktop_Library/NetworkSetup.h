#ifndef NETWORKSETUP_H
#define NETWORKSETUP_H
#include <string>
#include <memory>
#include "CommonNetwork.h"

namespace RemoteDesktop{

	class SocketHandler;

	namespace _INTERNAL{
		extern bool NetworkStarted;
	}
	bool StartupNetwork();
	void ShutDownNetwork();
	void AddFirewallException();
	void RemoveFirewallException();
	SOCKET Connect(std::wstring port, std::wstring host);
	SOCKET Listen(std::wstring port, std::wstring host, int backlog = 10);
	void StandardSocketSetup(SOCKET socket);
	std::string GetMAC();
	std::string GetIP();
	RemoteDesktop::Network_Return SendLoop(SOCKET sock, char* data, int len);
	RemoteDesktop::Network_Return ReceiveLoop(SOCKET sock, std::vector<char>& outdata, int& datareceived);
	RemoteDesktop::Network_Return CheckState(SOCKET sock);

	void Cleanup_System_Configuration();
}
#endif