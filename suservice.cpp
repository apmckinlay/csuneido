// Copyright (c) 2002 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

/* COULD: config to restart automatically
	 SERVICE_FAILURE_ACTIONS fa;
	 fa.dwResetPeriod = 86400; // One day
	 fa.lpRebootMsg = NULL;
	 fa.lpCommand = NULL;
	 fa.cActions = 1;
	 SC_ACTION sa[1];
	 sa[0].Delay = 60000; // One minute
	 sa[0].Type = SC_ACTION_RESTART;
	 fa.lpsaActions = sa;
	 l_Error = ChangeServiceConfig2
		(
		 m_ServiceHandle,
		 SERVICE_CONFIG_FAILURE_ACTIONS,
		 &fa
		); */

#include "suservice.h"
#include "win.h"
#include "except.h"
#include "ostreamstr.h"
#include "alert.h"
#include <process.h>

extern int su_port;

struct ExePath {
	char path[MAX_PATH];
	char dir[MAX_PATH];
	char name[100];
};

static void PrintError(const char* lpszFunction, const char* msg);
static void UpdateSCMStatus(DWORD dwCurrentState, DWORD dwWaitHint = 0);
static void WINAPI ServiceCtrlHandler(DWORD controlCode);
static void WINAPI ServiceMain(DWORD argc, LPTSTR* argv);
static ExePath* exe_path();
static void StopService();

static const char*
	args; // used to pass from CallServiceDispatcher to ServiceMain
static PROCESS_INFORMATION pi;
static DWORD serviceCurrentStatus = 0;
static SERVICE_STATUS_HANDLE serviceStatusHandle = 0;

void CallServiceDispatcher(const char* a) {
	args = a;
	SERVICE_TABLE_ENTRY serviceTable[] = {
		{exe_path()->name, ServiceMain}, {nullptr, nullptr}};
	StartServiceCtrlDispatcher(serviceTable);
}

void init2(HINSTANCE hInstance, LPSTR lpszCmdLine);

static void create_job(HANDLE hProcess) {
	HANDLE ghJob = CreateJobObject(nullptr, nullptr); // GLOBAL
	if (ghJob == nullptr) {
		alert("Could not create job object");
		return;
	}
	JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli = {};
	// Configure the job to terminate child processes when it's finished
	jeli.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
	if (0 ==
		SetInformationJobObject(
			ghJob, JobObjectExtendedLimitInformation, &jeli, sizeof(jeli)))
		alert("Could not SetInformationJobObject");
	if (0 == AssignProcessToJobObject(ghJob, hProcess))
		alert("Could not AssignProcessToObject");
}

void onexit(HANDLE hProcess) {
	DWORD result;
	if (!GetExitCodeProcess(hProcess, &result)) {
		alert("failed to get exit code");
		return;
	}
	if (result < 1 || 9 < result)
		return;
	OstreamStr oss;
	oss << "onexit" << result << ".bat";
	if (-1 == _spawnlp(P_NOWAIT, "cmd", "/c", oss.str(), nullptr))
		alert("failed to run " << oss.str());
}

static void WINAPI ServiceMain(DWORD argc, LPTSTR* argv) {
	SetCurrentDirectory(exe_path()->dir);

	serviceStatusHandle =
		RegisterServiceCtrlHandler(exe_path()->name, ServiceCtrlHandler);
	if (!serviceStatusHandle) {
		PrintError("ServiceMain", "RegisterServiceCtrlHandler");
		return;
	}
	// allow 5 seconds (5000 ms) for startup
	UpdateSCMStatus(SERVICE_START_PENDING, 5000);

	STARTUPINFO si;
	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);

	ZeroMemory(&pi, sizeof(pi));

	OstreamStr cmd(200);
	cmd << exe_path()->path << " -n -u " << args;
	if (!CreateProcess(NULL, // No module name (use command line).
			cmd.str(),       // Command line.
			NULL,            // Process handle not inheritable.
			NULL,            // Thread handle not inheritable.
			FALSE,           // Set handle inheritance to FALSE.
			0,               // No creation flags.
			NULL,            // Use parent's environment block.
			exe_path()->dir, // Use exe directory
			&si,             // Pointer to STARTUPINFO structure.
			&pi)) {          // Pointer to PROCESS_INFORMATION structure.
		PrintError("ServiceMain", "CreateProcess failed");
		return;
	}
	create_job(pi.hProcess);

	UpdateSCMStatus(SERVICE_RUNNING);

	// Wait until child process exits.
	// ServiceMain must not return until Server has ended
	WaitForSingleObject(pi.hProcess, INFINITE);
	onexit(pi.hProcess);

	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);

	UpdateSCMStatus(SERVICE_STOPPED);
}

struct ServiceHandle {
	ServiceHandle(SC_HANDLE h) : handle(h) {
	}
	operator SC_HANDLE() {
		return handle;
	}
	~ServiceHandle() {
		CloseServiceHandle(handle);
	}
	SC_HANDLE handle;
};

void InstallService(const char* svcargs) {
	ServiceHandle scm = OpenSCManager(0, 0, SC_MANAGER_CREATE_SERVICE);
	if (!scm) {
		PrintError("InstallService", "Open SCM Manager");
		return;
	}

	OstreamStr cmd(200);
	cmd << exe_path()->path << " -service " << svcargs;

	ServiceHandle myService = CreateService(scm, exe_path()->name,
		exe_path()->name, SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS,
		SERVICE_AUTO_START, SERVICE_ERROR_IGNORE, cmd.str(), 0, 0, 0, 0, 0);
	if (!myService) {
		PrintError("InstallService", "CreateService failed");
		return;
	}

	alert("Suneido Service successfully installed\n\n" << cmd.str());
}

void UnInstallService() {
	BOOL success;

	ServiceHandle scm = OpenSCManager(0, 0, SC_MANAGER_CREATE_SERVICE);
	if (!scm) {
		PrintError("UnInstallService", "Open SCM Manager failed");
		return;
	}

	ServiceHandle myService =
		OpenService(scm, exe_path()->name, SERVICE_ALL_ACCESS | DELETE);
	if (!myService) {
		PrintError("UnInstallService", "OpenService");
		return;
	}

	// Stop the service if necessary
	SERVICE_STATUS status;
	success = QueryServiceStatus(myService, &status);
	if (!success) {
		PrintError("UnInstallService", "QueryServiceStatus");
		return;
	}
	if (status.dwCurrentState != SERVICE_STOPPED) {
		success = ControlService(myService, SERVICE_CONTROL_STOP, &status);
		if (!success) {
			PrintError(
				"UnInstallService", "ControlService fails to stop service!");
			return;
		}
		Sleep(500);
	}

	success = DeleteService(myService);
	if (!success) {
		PrintError("UnInstallService", "DeleteService Failed");
		return;
	}
	alert("Service successfully removed.\n" << exe_path()->name);
}

static void PrintError(const char* lpszFunction, const char* msg) {
	LPVOID lpMsgBuf;
	DWORD dw = GetLastError();

	FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, dw, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (char*) &lpMsgBuf,
		0, NULL);

	alert("Error in function:" << lpszFunction << " Step: " << msg
							   << ".\nFailed with error " << dw << ":\n"
							   << lpMsgBuf << "\n"
							   << exe_path()->name);
}

static void UpdateSCMStatus(DWORD dwCurrentState, DWORD dwWaitHint) {
	serviceCurrentStatus = dwCurrentState;

	SERVICE_STATUS serviceStatus;
	ZeroMemory(&serviceStatus, sizeof(serviceStatus));
	serviceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
	serviceStatus.dwCurrentState = dwCurrentState;
	serviceStatus.dwWin32ExitCode = NO_ERROR;
	serviceStatus.dwServiceSpecificExitCode = 0;
	serviceStatus.dwCheckPoint = 0;
	serviceStatus.dwWaitHint = dwWaitHint;
	// If in the process of something, then accept
	// no control events, else accept anything
	if (dwCurrentState == SERVICE_START_PENDING)
		serviceStatus.dwControlsAccepted = 0;
	else
		serviceStatus.dwControlsAccepted =
			SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
	SetServiceStatus(serviceStatusHandle, &serviceStatus);
}

static void WINAPI ServiceCtrlHandler(DWORD controlCode) {
	if (controlCode == SERVICE_CONTROL_STOP ||
		controlCode == SERVICE_CONTROL_SHUTDOWN) {
		UpdateSCMStatus(SERVICE_STOP_PENDING, 5000);
		StopService();
		UpdateSCMStatus(SERVICE_STOPPED);
	}
	UpdateSCMStatus(serviceCurrentStatus);
}

static ExePath* exe_path() {
	static ExePath exe_path;

	if (!exe_path.path[0]) {
		char buf[MAX_PATH];
		GetModuleFileName(NULL, buf, MAX_PATH);
		char* filepart;
		int n = GetFullPathName(buf, MAX_PATH, exe_path.dir, &filepart);
		verify(0 <= n && n < MAX_PATH && filepart);

		strcpy(exe_path.path, exe_path.dir);

		int i = 0;
		for (;
			 i < sizeof exe_path.name - 1 && filepart[i] && filepart[i] != '.';
			 ++i)
			exe_path.name[i] = filepart[i];
		exe_path.name[i] = 0;

		*filepart = 0; // truncate dir after copying file part to name
	}
	return &exe_path;
}

#include "errlog.h"

static void StopService() {
	PostThreadMessage(pi.dwThreadId, WM_QUIT, 0, 0);
	if (WAIT_TIMEOUT == WaitForSingleObject(pi.hProcess, 5000)) {
		TerminateProcess(pi.hProcess, 0);
		errlog("timeout (5 sec) waiting for app to quit");
	}
}
