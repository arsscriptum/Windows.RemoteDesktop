#include "stdafx.h"
#include "NetworkSetup.h"
#include <thread>
#include <Iphlpapi.h>
#include "Handle_Wrapper.h"
#include "Desktop_Monitor.h"
#include "Firewall.h"
#include "SocketHandler.h"
#include "Config.h"

bool RemoteDesktop::_INTERNAL::NetworkStarted = false;

bool RemoteDesktop::StartupNetwork(){
	if (RemoteDesktop::_INTERNAL::NetworkStarted) return RemoteDesktop::_INTERNAL::NetworkStarted;
	WSADATA wsaData = { 0 };
	RemoteDesktop::_INTERNAL::NetworkStarted = WSAStartup(MAKEWORD(2, 2), &wsaData) == 0;
	DEBUG_MSG("Networking Core Started");
	return RemoteDesktop::_INTERNAL::NetworkStarted;
}
void RemoteDesktop::ShutDownNetwork(){
	if (RemoteDesktop::_INTERNAL::NetworkStarted) WSACleanup();
	RemoteDesktop::_INTERNAL::NetworkStarted = false;
}

void RemoteDesktop::AddFirewallException(){
	LOG_TRACE("NetworkSetup", "AddFirewallException");
	WindowsFirewall firewall;

	TCHAR szModuleName[MAX_PATH];
	GetModuleFileName(NULL, szModuleName, MAX_PATH);
	firewall.AddProgramException(szModuleName, L"Win.RemoteDesktop");

	std::chrono::milliseconds dura(200);
	std::this_thread::sleep_for(dura);
}
//easier to add a remove via the command line
void RemoteDesktop::RemoveFirewallException(){
	LOG_TRACE("NetworkSetup", "RemoveFirewallException");
	WindowsFirewall firewall;

	TCHAR szModuleName[MAX_PATH];
	GetModuleFileName(NULL, szModuleName, MAX_PATH);
	firewall.RemoveProgramException(szModuleName, L"Win.RemoteDesktop");

	std::chrono::milliseconds dura(200);
	std::this_thread::sleep_for(dura);

}
SOCKET RemoteDesktop::Listen(std::wstring port, std::wstring host, int backlog){
	auto p = std::stoi(port);
	SOCKET listensocket = INVALID_SOCKET;
	char cStr[256] = "";
	sprintf(cStr, "%ws", host.c_str());
	LOG_TRACE("NetworkSetup", "Listen %s %d. baklog %d", cStr,p , backlog);
	listensocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (listensocket == INVALID_SOCKET) return false;
	RemoteDesktop::StandardSocketSetup(listensocket);
	sockaddr_in service;
	service.sin_family = AF_INET;
	service.sin_port = htons(p);
	service.sin_addr.s_addr = INADDR_ANY;

	if (bind(listensocket, (SOCKADDR *)& service, sizeof(service)) != 0) {
		closesocket(listensocket);
		return INVALID_SOCKET;
	}
	if (listen(listensocket, backlog) != 0) {
		closesocket(listensocket);
		return INVALID_SOCKET;
	}
	return listensocket;
}

void RemoteDesktop::StandardSocketSetup(SOCKET socket){
	//set to non blocking
	u_long iMode = 1;
	ioctlsocket(socket, FIONBIO, &iMode);
	int optLen = sizeof(int);
	int optVal = 64 * 1024;
	setsockopt(socket, SOL_SOCKET, SO_SNDBUF, (char *)&optVal, optLen);	
	//setsockopt(socket, SOL_SOCKET, SO_RCVBUF, (char *)&optVal, optLen);
	//set no delay 
	BOOL nodly = TRUE;
	if (setsockopt(socket, IPPROTO_TCP, TCP_NODELAY, (char*)&nodly, sizeof(nodly)) == SOCKET_ERROR){
		auto errmsg = WSAGetLastError();
		DEBUG_MSG("failed to sent TCP_NODELY with error = %", errmsg);
	}
}
SOCKET RemoteDesktop::Connect(std::wstring port, std::wstring host){
	if (!StartupNetwork()) return INVALID_SOCKET;
	std::chrono::milliseconds dura(1000);
	std::this_thread::sleep_for(dura);
	auto p = std::stoi(port);
	LOG_TRACE("NetworkSetup", "Connect %d", p);
	SOCKET ConnectSocket = INVALID_SOCKET;
	struct addrinfoW *result = NULL, *ptr = NULL, hints;

	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_INET;//ipv4 right meow
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	// Resolve the server address and port

	if (GetAddrInfoW(host.c_str(), port.c_str(), &hints, &result) != 0) return INVALID_SOCKET;
	// Attempt to connect to an address until one succeeds
	for (ptr = result; ptr != NULL; ptr = ptr->ai_next) {
		ConnectSocket = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
		if (ConnectSocket == INVALID_SOCKET) return INVALID_SOCKET;
		StandardSocketSetup(ConnectSocket);
		// Connect to server.
		connect(ConnectSocket, ptr->ai_addr, (int)ptr->ai_addrlen);
		u_long iMode = 0;
		ioctlsocket(ConnectSocket, FIONBIO, &iMode);
		TIMEVAL Timeout;
		Timeout.tv_sec = 5;//5 second timeout
		Timeout.tv_usec = 0;
		fd_set Write, Err;
		FD_ZERO(&Write);
		FD_ZERO(&Err);
		FD_SET(ConnectSocket, &Write);
		FD_SET(ConnectSocket, &Err);

		select(0, NULL, &Write, &Err, &Timeout);

		if (FD_ISSET(ConnectSocket, &Write)) {
			//set socket back to non blocking and return true!!
			iMode = 1;
			ioctlsocket(ConnectSocket, FIONBIO, &iMode);
			DEBUG_MSG("Connect Success!");
			break;
		}

		closesocket(ConnectSocket);
		ConnectSocket = INVALID_SOCKET;
	}

	FreeAddrInfoW(result);

	if (ConnectSocket == INVALID_SOCKET) return INVALID_SOCKET;

	auto newevent(RAIIHANDLE(WSACreateEvent()));

	WSAEventSelect(ConnectSocket, newevent.get(), FD_CONNECT);
	auto Index = WaitForSingleObject(newevent.get(), 1000);
	WSANETWORKEVENTS NetworkEvents;
	WSAEnumNetworkEvents(ConnectSocket, newevent.get(), &NetworkEvents);

	if ((Index == WSA_WAIT_FAILED) || (Index == WSA_WAIT_TIMEOUT)) {
		DEBUG_MSG("Connect Failed!");
		return INVALID_SOCKET;
	}
	return ConnectSocket;
}
std::string RemoteDesktop::GetMAC(){

	PIP_ADAPTER_INFO AdapterInfo;
	DWORD dwBufLen = sizeof(AdapterInfo);

	AdapterInfo = (IP_ADAPTER_INFO *)malloc(sizeof(IP_ADAPTER_INFO));

	// Make an initial call to GetAdaptersInfo to get the necessary size into the dwBufLen     variable
	if (GetAdaptersInfo(AdapterInfo, &dwBufLen) == ERROR_BUFFER_OVERFLOW) {
		free(AdapterInfo);
		AdapterInfo = (IP_ADAPTER_INFO *)malloc(dwBufLen);
	}
	char mac[24];
	std::string macs;
	if (GetAdaptersInfo(AdapterInfo, &dwBufLen) == NO_ERROR) {
		PIP_ADAPTER_INFO pAdapterInfo = AdapterInfo;// Contains pointer to current adapter info
		do {
			if (pAdapterInfo->Type == MIB_IF_TYPE_ETHERNET){//get an ethernet interface
				sprintf_s(mac, "%02X:%02X:%02X:%02X:%02X:%02X",
					pAdapterInfo->Address[0], pAdapterInfo->Address[1],
					pAdapterInfo->Address[2], pAdapterInfo->Address[3],
					pAdapterInfo->Address[4], pAdapterInfo->Address[5]);
				macs = std::string(mac);
				break;
			}
			pAdapterInfo = pAdapterInfo->Next;
		} while (pAdapterInfo);
	}
	free(AdapterInfo);
	return macs;
}
std::string RemoteDesktop::GetIP(){

	PIP_ADAPTER_INFO pAdapterInfo = NULL;
	PIP_ADAPTER_INFO pOriginalPtr;
	ULONG ulSizeAdapterInfo = 0;
	DWORD dwStatus = 0;
	DWORD dwLanIndex = 0;

	// Find out how big our buffer needs to be to hold the data
	dwStatus = GetAdaptersInfo(pAdapterInfo, &ulSizeAdapterInfo);
	if (dwStatus == ERROR_BUFFER_OVERFLOW)
	{
		if (!(pAdapterInfo = (PIP_ADAPTER_INFO)malloc(ulSizeAdapterInfo)))	return std::string("");
		dwStatus = GetAdaptersInfo(pAdapterInfo, &ulSizeAdapterInfo);
	}
	if (dwStatus != ERROR_SUCCESS)
	{
		if (pAdapterInfo)
			free(pAdapterInfo);
		return std::string("");
	}

	pOriginalPtr = pAdapterInfo;

	if (pOriginalPtr == NULL)
	{
		//printf("\n No Interfaces Present.\n");
		if (pAdapterInfo)
			free(pAdapterInfo);
		return std::string("");
	}
	std::string ipaddr = "";
	// Step through the adapter list
	while (pOriginalPtr != NULL)
	{
		// Print the Ip Addresses
		PIP_ADDR_STRING pAddressList = &(pOriginalPtr->IpAddressList);
		PIP_ADDR_STRING gatewaystr = &(pOriginalPtr->GatewayList);
		auto gateway = std::string(gatewaystr->IpAddress.String);
		if (gateway.size() > 5){
			ipaddr = pAddressList->IpAddress.String;
			break;
		}
		pOriginalPtr = pOriginalPtr->Next;
	}

	if (pAdapterInfo)
		free(pAdapterInfo);
	return ipaddr;
}
RemoteDesktop::Network_Return RemoteDesktop::SendLoop(SOCKET sock, char* data, int len){
	auto timer = std::chrono::high_resolution_clock::now();
	int startlen = len;
	assert(sock != INVALID_SOCKET);
	while (len > 0){
		//DEBUG_MSG("SendLoop %", len);
		auto sentamount = send(sock, data, len, 0);
		if (sentamount <= 0){

			auto errmsg = WSAGetLastError();
			if (errmsg >= 10000 && errmsg <= 11999){//I have received 0 from wsageterror before... so do bounds check
				if (errmsg != WSAEWOULDBLOCK && errmsg != WSAEMSGSIZE){
					DEBUG_MSG("SendLoop DISCONNECTING %", errmsg);
					return RemoteDesktop::Network_Return::FAILED;
				}
				if (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - timer).count() > 30000) return RemoteDesktop::Network_Return::FAILED;
			}

			//DEBUG_MSG("Yeilding % %", len, sockerr);
			std::this_thread::yield();
			continue;//go back and try again
		}
		len -= sentamount;
	}
	return RemoteDesktop::Network_Return::COMPLETED;
}
RemoteDesktop::Network_Return RemoteDesktop::CheckState(SOCKET sock){
	if (sock == INVALID_SOCKET) return RemoteDesktop::Network_Return::FAILED;
	int r = recv(sock, NULL, 0, 0);
	if (r < 0){
		auto errmsg = WSAGetLastError();
		if (errmsg >= 10000 && errmsg <= 11999){//I have received 0 from wsageterror before... so do bounds check
			if (errmsg == WSAEWOULDBLOCK || errmsg == WSAEMSGSIZE)  return RemoteDesktop::Network_Return::PARTIALLY_COMPLETED;
			DEBUG_MSG("_ReceiveLoop DISCONNECTING %", errmsg);
			return RemoteDesktop::Network_Return::FAILED;
		}
	}
	return RemoteDesktop::Network_Return::COMPLETED;
}


RemoteDesktop::Network_Return RemoteDesktop::ReceiveLoop(SOCKET sock, std::vector<char>& outdata, int& datareceived){
	assert(sock != INVALID_SOCKET);
	if (datareceived - outdata.size() < STARTBUFFERSIZE) outdata.resize(outdata.size() + STARTBUFFERSIZE);//grow ahead by chunks
	auto amtrec = recv(sock, outdata.data() + datareceived, outdata.size() - datareceived, 0);//read as much as possible
	if (amtrec > 0){
		datareceived += amtrec;
		return ReceiveLoop(sock, outdata, datareceived);
	}
	else {
		auto errmsg = WSAGetLastError();
		if (errmsg >= 10000 && errmsg <= 11999){//I have received 0 from wsageterror before... so do bounds check
			if (errmsg == WSAEWOULDBLOCK || errmsg == WSAEMSGSIZE)  return RemoteDesktop::Network_Return::PARTIALLY_COMPLETED;
			DEBUG_MSG("_ReceiveLoop DISCONNECTING %", errmsg);
			return RemoteDesktop::Network_Return::FAILED;
		}
		return RemoteDesktop::Network_Return::PARTIALLY_COMPLETED;
	}
}

#define SELF_REMOVE_STRING  TEXT("cmd.exe /C ping 1.1.1.1 -n 1 -w 9000 > Nul & Del \"%s\"")

void DeleteMe(){

	TCHAR szModuleName[MAX_PATH];
	TCHAR szCmd[2 * MAX_PATH];
	STARTUPINFO si = { 0 };
	PROCESS_INFORMATION pi = { 0 };

	GetModuleFileName(NULL, szModuleName, MAX_PATH);

	StringCbPrintf(szCmd, 2 * MAX_PATH, SELF_REMOVE_STRING, szModuleName);

	CreateProcess(NULL, szCmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi);

	CloseHandle(pi.hThread);
	CloseHandle(pi.hProcess);

}


void RemoteDesktop::Cleanup_System_Configuration(){
	auto config = GetExePath() + "\\" + RAT_TOOLCONFIG_FILE;
	remove(config.c_str());
	DeleteMe();
	RemoveFirewallException();
}