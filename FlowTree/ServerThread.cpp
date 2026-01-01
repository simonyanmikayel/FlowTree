#include "stdafx.h"
#include "ServerThread.h"
#include "Archive.h"
#include "Helpers.h"
#include "Settings.h"
#include "Dbg.h"
#include "resource.h"

#ifdef USE_PIPE
DWORD traceMapFileSize()
{
	DWORD size = 4 * 1024 * 1024; //TODO get form settings
	if (size < UDP_BUF_SIZE)
		size = UDP_BUF_SIZE;
	return size;
}

ServerThread::ServerThread()
{
}

bool ServerThread::Send(void* sendbuf, DWORD cb)
{
	DWORD bytesWritten = 0;
	return WriteFile(hPipe, sendbuf, cb, &bytesWritten, NULL) && (bytesWritten == cb);
}

bool ServerThread::Recv(void* recvbuf, DWORD cb)
{
	DWORD bytesRead = 0;
	while (ReadFile(hPipe, recvbuf, cb, &bytesRead, NULL) && (bytesRead < cb))
		bytesRead += cb;
	return bytesRead == cb;
}

void ServerThread::Terminate()
{
	if (hPipe != INVALID_HANDLE_VALUE)
	{
		FlushFileBuffers(hPipe);
		CancelIo(hPipe);
		DisconnectNamedPipe(hPipe);
		CloseHandle(hPipe);
		hPipe = INVALID_HANDLE_VALUE;
	}
}

bool ServerThread::InitWorker()
{
	hPipe = CreateNamedPipe(
		TEXT("\\\\.\\pipe\\FlowTreePipe"), // Pipe name
		PIPE_ACCESS_DUPLEX,     // Read/write access
		PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT, // Byte stream
		PIPE_UNLIMITED_INSTANCES, // Max instances
		4096,                   // Output buffer size
		8 * 1024,                   // Input buffer size
		0,                      // Default timeout
		NULL                    // Default security
	);

	return (hPipe != INVALID_HANDLE_VALUE && ConnectNamedPipe(hPipe, NULL));
}


void ServerThread::RecvTraces()
{
#ifdef USE_MEM
	TraceFileMap ctrlFileMap;
	TraceFileMap dataFileMap;
	MapSync mapSync;
	bool failed = false;
	if (!ctrlFileMap.initRader(TEXT("FlowTreeCtrlSharedMemory"), sizeof(MapSync))) {
		Helpers::SysErrMessageBox(TEXT("Failed to create control map"));
		return;
	}
	if (!dataFileMap.initRader(TEXT("FlowTreeDataSharedMemory"), traceMapFileSize())) {
		Helpers::SysErrMessageBox(TEXT("Failed to create data map"));
		return;
	}
	if (!mapSync.initReader(ctrlFileMap.pBuf)) {
		Helpers::SysErrMessageBox(TEXT("Failed to sync event"));
		return;
	}
	while (true)
	{
		DWORD waitResult = WaitForSingleObject(mapSync.hReadyEvent, 1000);
		if (waitResult != WAIT_OBJECT_0 && waitResult != WAIT_TIMEOUT)
			break;
		//Sleep(0);
		mapSync.enterCriticalSection();
		while (mapSync.recvPos() < mapSync.sendPos())
		{
			DWORD cb_cmd;
			DWORD& recvPos = mapSync.recvPos();
			DWORD  sendPos = mapSync.sendPos();
			CmdLog* pLog = (CmdLog*)(dataFileMap.pBuf + recvPos);
			if (pLog->cmd == CMD_FLOW)
				cb_cmd = sizeof(CmdFlow);
			else if (pLog->cmd == CMD_TRACE)
				cb_cmd = sizeof(CmdTrace) + ((CmdTrace*)pLog)->CmdTraceDadaSize();
			else {
				Helpers::SysErrMessageBox(TEXT("Incorrect cmd"));
				return;
			}
			if (recvPos + cb_cmd > sendPos) {
				Helpers::SysErrMessageBox(TEXT("Incorrect size of cmd"));
				return;
			}
			if (!paused && !failed) {
				if (!gArchive.append(pLog->Pid, pLog))
				{
					if ((pLog->cmd != CMD_TRACE)) { // trace is empty
						Helpers::SysErrMessageBox(TEXT("Failed to add command"));
						failed = true;;
					}
				}
			}
			recvPos += cb_cmd;
		}
		mapSync.leaveCriticalSection();

		DWORD bytesAvailable, bytesLeftThisMessage;
		if (!PeekNamedPipe(hPipe, NULL, 0, NULL, &bytesAvailable, &bytesLeftThisMessage)) {
			DWORD error = GetLastError();
			if (error != ERROR_NO_DATA) {
				break;
			}
		}
	}
#else
	DWORD cb_cmd;
	char buf[UDP_BUF_SIZE];
	while (true)
	{
		if (!Recv(buf, sizeof(CmdLog)))
			break;
		CmdLog* pLog = (CmdLog*)(buf);
		if (pLog->cmd == CMD_FLOW)
			cb_cmd = sizeof(CmdFlow) - sizeof(CmdLog);
		else if (pLog->cmd == CMD_TRACE)
			cb_cmd = sizeof(CmdTrace) - sizeof(CmdLog);
		else
			break;
		if (!Recv(buf + sizeof(CmdLog), cb_cmd))
			break;
		if (pLog->cmd == CMD_TRACE)
		{
			cb_cmd = ((CmdTrace*)pLog)->CmdTraceDadaSize();
			if (!Recv(buf + sizeof(CmdTrace), cb_cmd))
				break;
		}

		if (!paused) {
			if (!gArchive.append(pLog->Pid, pLog))
			{
				if ((pLog->cmd != CMD_TRACE)) { // trace is empty
					Helpers::SysErrMessageBox(TEXT("Failed to add command %d"), pLog->cmd);
					break;
				}
			}
		}
	}
#endif
}
#else //USE_PIPE
static char* listenPort = "1963";
WORD udpPort = 1964;

ServerThread::ServerThread()
{
    int iResult;
    listenSocket = INVALID_SOCKET;
    clientSocket = INVALID_SOCKET;

    struct addrinfo *result = NULL;
    struct addrinfo hints;

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;

    // Resolve the server address and port
    iResult = getaddrinfo(NULL, listenPort, &hints, &result);
    if (iResult != 0) {
        Helpers::ErrMessageBox(TEXT("getaddrinfo failed with error: %d"), iResult);
        goto err;
    }

    // Create a SOCKET for connecting to server
    listenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (listenSocket == INVALID_SOCKET) {
        Helpers::ErrMessageBox(TEXT("socket failed with error: %ld"), WSAGetLastError());
        goto err;
    }

    // Setup the TCP listening socket
    iResult = bind(listenSocket, result->ai_addr, (int)result->ai_addrlen);
    if (iResult == SOCKET_ERROR) {
        Helpers::ErrMessageBox(TEXT("bind to listen port %s failed with error: %ld"), listenPort, WSAGetLastError());
        goto err;
    }

    freeaddrinfo(result);

	traceThread.StartWork();

    return;
err:
    Helpers::CloseSocket(listenSocket);
    if (result)
        freeaddrinfo(result);
}

void ServerThread::Terminate()
{
    Helpers::CloseSocket(listenSocket);
    Helpers::CloseSocket(clientSocket);
	traceThread.StopWork();
}

bool ServerThread::Send(void* sendbuf, DWORD cb)
{
    int iResult = ::send(clientSocket, (char*)sendbuf, cb, 0);
    if (iResult == SOCKET_ERROR) {
        if (IsWorking())
			stdlog(TEXT("send failed with error: %d\n"), WSAGetLastError());
        Helpers::CloseSocket(clientSocket);
        return false;
    }

    return true;
}

bool ServerThread::Recv(void* recvbuf, DWORD cb)
{
    int iResult = 0;
    // Receive until all data arrived or the peer closes the connection
    while (cb > 0) {

        iResult = recv(clientSocket, (char*)recvbuf, cb, 0);
        if (iResult > 0) {
            cb -= iResult;
            recvbuf = ((char*)recvbuf) + iResult;
        }
        else if (iResult == 0) {
            break;
        }
        else {
            break;
        }
    }

    if (cb != 0) {
        Helpers::CloseSocket(clientSocket);
        return false;
    }
    else {
        return true;
    }
}

void ServerThread::RecvTraceViaTCP()
{
	DWORD cb_cmd;
	char buf[UDP_BUF_SIZE];
	DWORD cb;
	bool singleLog = gSettings.DissableBufferization();
	while (true)
	{
		if (singleLog)
			cb = INFINITE;
		else if (!Recv(&cb, sizeof(cb)))
			return;
		while (cb)
		{
			if (!Recv(buf, sizeof(CmdLog)) || (cb < sizeof(CmdLog)))
				return;
			CmdLog* pLog = (CmdLog*)(buf);
			if (pLog->cmd == CMD_FLOW)
				cb_cmd = sizeof(CmdFlow) - sizeof(CmdLog);
			else if (pLog->cmd == CMD_TRACE)
				cb_cmd = sizeof(CmdTrace) - sizeof(CmdLog);
			else
				return;
			if (!Recv(buf + sizeof(CmdLog), cb_cmd))
				return;
			if (pLog->cmd == CMD_TRACE)
			{
				cb_cmd = ((CmdTrace*)pLog)->CmdTraceDadaSize();
				if (!Recv(buf + sizeof(CmdTrace), cb_cmd))
					return;
			}
			cb -= cb_cmd;
			if (!gArchive.append(pLog->Pid, pLog))
			{
				if ((pLog->cmd != CMD_TRACE)) { // trace is empty
					Helpers::SysErrMessageBox(TEXT("Failed to add command %d"), pLog->cmd);
					return;
				}
			}
			if (singleLog)
				break;
		}
	}
}

TraceThread::TraceThread()
{
	struct sockaddr_in server;

	//Create a socket
	if ((traceSocket = socket(AF_INET, SOCK_DGRAM, 0)) == INVALID_SOCKET)
	{
		Helpers::SysErrMessageBox(TEXT("Can not create traceSocket socket"));
		goto err;
	}

	int opt = 1;
	if (setsockopt(traceSocket, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt)) < 0)
	{
		Helpers::SysErrMessageBox(TEXT("setsockopt on traceSocket for SOL_SOCKET failed\n"));
		goto err;
	}

	//Prepare the sockaddr_in structure
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = INADDR_ANY;
	//server.sin_addr.s_addr = htonl(INADDR_BROADCAST);
	server.sin_port = htons(udpPort);

	//Bind
	if (bind(traceSocket, (struct sockaddr *)&server, sizeof(server)) == SOCKET_ERROR)
	{
		Helpers::SysErrMessageBox(TEXT("Bind on UDP port %s failed : %d"), udpPort);
		goto err;
	}
	return;
err:
	Helpers::CloseSocket(traceSocket);
}

void TraceThread::Terminate()
{
	Helpers::CloseSocket(traceSocket);
}

void TraceThread::Work(LPVOID pWorkParam)
{
	int slen, cb, cb_read;
	DWORD cb_cmd;
	char buf[UDP_BUF_SIZE];

	struct sockaddr_in si_other;
	slen = sizeof(si_other);

	while (true)
	{
		//try to receive data, this is a blocking call
		if ((cb = recvfrom(traceSocket, buf, sizeof(buf), 0, (struct sockaddr *) &si_other, &slen)) < 0)
		{
			if (!IsTerminated())
				Helpers::SysErrMessageBox(TEXT("recvfrom failed"));
			break;
		}
		if (cb < sizeof(CmdLog) || cb > UDP_BUF_SIZE)
		{
			Helpers::SysErrMessageBox(TEXT("bad udp package size %d"), cb);
			break;
		}
		CmdLog* pLog = (CmdLog*)(buf);
		cb_read = 0;
		cb_cmd = UDP_BUF_SIZE;
		if (pLog->cmd == CMD_FLOW)
			cb_cmd = sizeof(CmdFlow);
		else if (pLog->cmd == CMD_TRACE)
			cb_cmd = ((CmdTrace*)pLog)->CmdTraceSize();
		while ( (cb_read + cb_cmd) <= (DWORD)cb )
		{
			if (!gArchive.append(pLog->Pid, pLog))
			{
				if ((pLog->cmd != CMD_TRACE)) { // trace is empty
					Helpers::SysErrMessageBox(TEXT("Failed to add command %d"), pLog->cmd);
					break;
				}
			}
			cb_read += cb_cmd;
			cb_cmd = UDP_BUF_SIZE;
			pLog = (CmdLog*)(buf + cb_read);

			if (pLog->cmd == CMD_FLOW)
				cb_cmd = sizeof(CmdFlow);
			else if (pLog->cmd == CMD_TRACE)
				cb_cmd = ((CmdTrace*)pLog)->CmdTraceSize();
		}
		if (cb_read != cb)
		{
			Helpers::SysErrMessageBox(TEXT("Bad log data %d %d"), cb_read, cb);
			break;
		}
	}
	Helpers::CloseSocket(traceSocket);
}
#endif //USE_PIPE

void ServerThread::IpcErr(CHAR* lpFormat, ...)
{
	if (!IsTerminated())
	{
#ifdef USE_SOCK
		Helpers::CloseSocket(clientSocket);
#endif //USE_SOCK
		va_list vl;
		va_start(vl, lpFormat);

		CHAR buf[1024];
		_vsntprintf_s(buf, 1023, 1023, lpFormat, vl);
		va_end(vl);
		stdlog(buf);
	}
}

void ServerThread::Work(LPVOID pWorkParam)
{
#ifdef USE_SOCK
	if (listen(listenSocket, SOMAXCONN) == SOCKET_ERROR) {
		Helpers::ErrMessageBox(TEXT("listen on port %s failed with error: %d\n Restart application"), listenPort, WSAGetLastError());
		Helpers::CloseSocket(listenSocket);
		return;
	}
#endif //USE_SOCK
	while (true) {
#ifdef USE_SOCK
		// Accept a client socket
		clientSocket = accept(listenSocket, NULL, NULL);
		if (listenSocket == INVALID_SOCKET && clientSocket == INVALID_SOCKET) {
			int err = WSAGetLastError();
			if (!IsTerminated())
				Helpers::ErrMessageBox(TEXT("accept failed with error: %d\n Restart application"), WSAGetLastError());
			Helpers::CloseSocket(listenSocket);
			return;
		}
#else //USE_SOCK
		Terminate();
		if (!InitWorker())
			break;
#endif //USE_SOCK

		::PostMessage(hwndMain, WM_COMMAND, ID_VIEW_UPDATE_STATUS, 0);
		DbgInfo* pDbgInfo = nullptr;
		Helpers::ClearLog();
		std::vector<CmdModule> modules;
		CmdStart cmdStart;
		while (true)
		{
			BYTE command = 0;
			if (!Recv(&command, sizeof(command)))
				break;

			bool res = true;
			switch (command)
			{
			case CMD_START:
			{
				if (!(res = Recv(CMD_BUF_AND_SIZE(cmdStart))))
				{
					IpcErr(TEXT("No start info received\n"));
					break;
				}
				CmdSettings cmdSettings;
				cmdSettings.DissableBufferization = gSettings.DissableBufferization();
				cmdSettings.UseTcpForLog = gSettings.UseTcpForLog();
				cmdSettings.LogOnlyTraces = gSettings.LogOnlyTraces();
				if (!(res = Send(&cmdSettings, sizeof(cmdSettings))))
				{
					IpcErr(TEXT("Failed to send cmdSettings\n"));
					break;
				}
				modules.clear();
				CmdInfoSize cmd;
				std::vector<DbgModule> arDbgModules;
				if (!gSettings.LogOnlyTraces())
				{
					arDbgModules = gSettings.m_DbgSettings.m_arDbgModules;
				}
				cmd.infoSize = 0;
				for (DWORD i = 0; i < (DWORD)arDbgModules.size(); i++)
				{
					if (!arDbgModules[i].m_skipModule)
						cmd.infoSize++;
				}
				if (cmd.infoSize == 0 && !gSettings.LogOnlyTraces())
				{
					// use module name from cmdStart
					DbgModule dbgModule;
					char* szDbgModName = strrchr(cmdStart.szAppName, '\\');
					if (szDbgModName)
						szDbgModName++;
					else
						szDbgModName = cmdStart.szAppName;
					strcpy_s(dbgModule.m_szModul, szDbgModName);
					arDbgModules.push_back(dbgModule);
					cmd.infoSize++;
				}
				if (!(res = Send(&cmd, sizeof(cmd))))
				{
					IpcErr(TEXT("Failed to send CmdInfoSize\n"));
					break;
				}
				CmdModule mo;
				for (DWORD i = 0; i < (DWORD)arDbgModules.size(); i++)
				{
					if (arDbgModules[i].m_skipModule)
						continue;
					mo.startAddr = 0;
					strcpy_s(mo.szModName, arDbgModules[i].m_szModul);
					if (!(res = Send(&mo, sizeof(mo))))
					{
						IpcErr(TEXT("Failed to send CmdModule\n"));
						break;
					}
				}
			}
			break;
			case CMD_MODULE:
			{
				CmdModule cmd;
				if (!(res = Recv(CMD_BUF_AND_SIZE(cmd))))
				{
					IpcErr(TEXT("Failed to receive CmdModule\n"));
					break;
				}
				modules.push_back(cmd);
			}
			break;
			case CMD_PREP_INFO:
			{
				//if (modules.size() == 0)
				//{
				//	IpcErr(TEXT("No module path sent\n"));
				//	res = false;
				//	break;
				//}
				ZeroMemory(&gDbgLoadStatus, sizeof(gDbgLoadStatus));
				gDbgLoadStatus.cModulesTotal = (DWORD)modules.size();
				Helpers::BlockLogStatus(true);
				pDbgInfo = new DbgInfo(cmdStart.szAppName);
				for (size_t i = 0; i < modules.size(); i++)
				{
					gDbgLoadStatus.cModulesLoading++;
					gDbgLoadStatus.cFunctionsLoaded = 0;
					gDbgLoadStatus.cSettingsChecked = 0;
					char* szModFileName = strrchr(modules[i].szModName, '\\');
					if (szModFileName)
						szModFileName++;
					else
						szModFileName = modules[i].szModName;
					strncpy(gDbgLoadStatus.szCurrentModule, szModFileName, MAX_PATH - 1);
					if (!pDbgInfo->AddModule(&modules[i]))
					{
						IpcErr(TEXT("Failed to load symbols for %s\n"), modules[i].szModName);
						res = false;
						break;
					}
					if (gDbgLoadStatus.stop)
					{
						res = false;
						break;
					}
				}
				if (res)
				{

					DWORD cModules = (DWORD)pDbgInfo->arModuleData.size();
					CmdInfoSize cmd;
					cmd.infoSize = cModules;
					if (!(res = Send(&cmd, sizeof(cmd))))
					{
						IpcErr(TEXT("Failed to send modules count\n"));
						break;
					}
					FUNC_ADDR fa;
					for (DWORD i = 0; i < cModules && res; i++)
					{
						CmdModule cmd2;
						cmd2.startAddr = pDbgInfo->arModuleData[i]->startAddr;
						cmd2.endAddr = pDbgInfo->arModuleData[i]->endAddr;
						cmd2.endAddr = pDbgInfo->arModuleData[i]->endAddr;
						cmd2.FuncCount = (DWORD)pDbgInfo->arModuleData[i]->info.Count();
						strcpy_s(cmd2.szModName, pDbgInfo->arModuleData[i]->szModName);
						if (!(res = Send(&cmd2, sizeof(cmd2))))
						{
							IpcErr(TEXT("Failed to send function count\n"));
							break;
						}
						for (DWORD j = 0; j < cmd2.FuncCount && res; j++)
						{
							if (!pDbgInfo->arModuleData[i]->GetFuncAddresses(j, fa)) {
								IpcErr(TEXT("Failed to get function address\n"));
								break;
							}
							if (!(res = Send(&fa, sizeof(fa))))
							{
								IpcErr(TEXT("Failed to send DbgFuncInfo\n"));
								break;
							}
						}
					}
				}
				if (!res)
				{
					delete pDbgInfo;
					pDbgInfo = nullptr;
					Helpers::UpdateStatusBar("Failed to load debug data");
				}
				else
				{
					CmdStarting cmdStarting;
					cmdStarting.TRACE_MAP_FILE_SIZE = traceMapFileSize();
					if (!(res = Send(&cmdStarting, sizeof(cmdStarting))))
					{
						IpcErr(TEXT("Failed to send cmdStarting\n"));
						break;
					}
				}
			}
			break;
			case CMD_STARTING:
			{
				CmdStarting cmdStarting;
				if (!(res = Recv(CMD_BUF_AND_SIZE(cmdStarting))))
				{
					IpcErr(TEXT("No started info received\n"));
					res = false;
					break;
				}
				pDbgInfo->appPath[MAX_PATH] = 0;
				pDbgInfo->Pid = cmdStart.Pid;
				gArchive.TickCount(cmdStarting.tickCount);
				gArchive.RegisterApp(pDbgInfo);
				Helpers::UpdateStatusBar("");
#ifdef USE_PIPE
				Helpers::BlockLogStatus(false);
				RecvTraces();
#endif
			}
			break;
			default:
				IpcErr(TEXT("Unknown command %d\n"), command);
				res = false;
				break;
			}
#ifdef USE_SOCK
			if (!res)
			{
				break;
			}

			if (command == CMD_STARTING)
			{
				break;
			}
#endif //USE_SOCK
		}
		Helpers::BlockLogStatus(false);
#ifdef USE_SOCK
		if (pDbgInfo != nullptr)
		{
			if (gSettings.UseTcpForLog()) {
				RecvTraceViaTCP();
				Helpers::CloseSocket(clientSocket);
			}
			else {
				clientSocket = INVALID_SOCKET;
			}
		}
		Helpers::CloseSocket(clientSocket);
#endif //USE_SOCK
	}

}