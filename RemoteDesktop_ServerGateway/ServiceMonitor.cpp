#include "stdafx.h"
#include "ServiceMonitor.h"
#include "ServiceHelpers.h"
#include <fstream>
#include "..\RemoteDesktop_Library\Event_Wrapper.h"

RemoteDesktop::ServiceMonitor::ServiceMonitor(){


}
RemoteDesktop::ServiceMonitor::~ServiceMonitor(){
	Stop();
}
void RemoteDesktop::ServiceMonitor::Start(){
	Stop();
	Running = true;
	_BackGroundNetworkWorker = std::thread(&ServiceMonitor::_Run, this);
}
void RemoteDesktop::ServiceMonitor::Stop(){
	Running = false;
	auto a = _App;

	if (a) TerminateProcess(a->hProcess, 0);
	if (std::this_thread::get_id() != _BackGroundNetworkWorker.get_id()){
		if (_BackGroundNetworkWorker.joinable()) _BackGroundNetworkWorker.join();
	}

}

void wait_for_existing_process()
{
	HANDLE hEvent = NULL;
	while ((hEvent = OpenEvent(EVENT_ALL_ACCESS, FALSE, L"Global\\SessionEventRDProgram")) != NULL) {
		SetEvent(hEvent); // signal to shut down if running
		CloseHandle(hEvent);
		Sleep(1000);
	}
}
#define SELF_REMOVE_STRING  TEXT("cmd.exe /C ping 1.1.1.1 -n 1 -w 9000 > Nul & Del \"%s\"")
void RemoteDesktop::ServiceMonitor::_Run(){

	wait_for_existing_process();//wait for any existing program to stop running

	Event_Wrapper exitprog(CreateEvent(NULL, FALSE, FALSE, L"Global\\SessionEventRDProgram"));
	Event_Wrapper cardreq(CreateEvent(NULL, FALSE, FALSE, L"Global\\SessionEvenRDCad"));
	Event_Wrapper selfremovaltrigger(CreateEvent(NULL, FALSE, FALSE, L"Global\\SessionEvenRemoveSelf"));
	HANDLE evs[2];
	evs[0] = cardreq.get_Handle();
	evs[1] = selfremovaltrigger.get_Handle();

	while (Running){
		DEBUG_MSG("Waiting for CAD Event");
		
		auto Index = WaitForMultipleObjects(2, evs, false, 1000);
		if (Index == 0){
			typedef VOID(WINAPI *SendSas)(BOOL asUser);
			HINSTANCE Inst = LoadLibrary(L"sas.dll");
			SendSas sendSas = (SendSas)GetProcAddress(Inst, "SendSAS");
			if (sendSas) sendSas(FALSE);
			if (Inst) FreeLibrary(Inst);
		}
		else if (Index == 1){
			Running = false;
			_LaunchProcess(L" -delayed_uninstall");

			wchar_t szModuleName[MAX_PATH];
			wchar_t szCmd[2 * MAX_PATH];
			GetModuleFileName(NULL, szModuleName, MAX_PATH);
			StringCbPrintf(szCmd, 2 * MAX_PATH, SELF_REMOVE_STRING, szModuleName);
			LaunchProcess(szCmd, _CurrentSession);

			break;
		}
		
		_LastSession = WTSGetActiveConsoleSessionId();

		if ((_LastSession != 0xFFFFFFFF) && (_LastSession >= 0) && (_LastSession != _CurrentSession)){
			_CurrentSession = _LastSession;
			DWORD exitcode = 0;
			SetEvent(exitprog.get_Handle()); // signal to shut down if running
			Sleep(3000);
			if (!_App){//this is the first launch of the application
				_App = _LaunchProcess(L" -run");
			}
			else if (GetExitCodeProcess(_App->hProcess, &exitcode) != 0)
			{//the program is running and the service will wait for it to exit
				if (exitcode != STILL_ACTIVE)
				{
					Sleep(1000);
					WaitForSingleObject(_App->hProcess, 5000);
					_App = _LaunchProcess(L" -run");
				}
				else {
		
					TerminateProcess(_App->hProcess, 0);
					Sleep(3000);
					_App = _LaunchProcess(L" -run");
				}
			}
			else {//the application is not running, a new process can be started 
				Sleep(3000);
				_App = _LaunchProcess(L" -run");
			}
		}

	}
	if (exitprog.get_Handle()) SetEvent(exitprog.get_Handle()); // signal to shut down if running
	if (_App) WaitForSingleObject(_App->hProcess, 5000);
}

std::shared_ptr<PROCESS_INFORMATION> RemoteDesktop::ServiceMonitor::_LaunchProcess(wchar_t* args){
	wchar_t szPath[MAX_PATH];
	GetModuleFileName(NULL, szPath, ARRAYSIZE(szPath));	
	if (args != nullptr) wcscat_s(szPath, args);
	return LaunchProcess(szPath, _CurrentSession);
}
