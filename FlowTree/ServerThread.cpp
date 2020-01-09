#include "stdafx.h"
#include "ServerThread.h"
#include "Archive.h"
#include "Helpers.h"
#include "Settings.h"
#include "Dbg.h"
#include "resource.h"

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
        Helpers::ErrMessageBox(TEXT("bind failed with error: %ld"), WSAGetLastError());
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

bool ServerThread::Send(void* sendbuf, int cb)
{
    int iResult = ::send(clientSocket, (char*)sendbuf, cb, 0);
    if (iResult == SOCKET_ERROR) {
        if (IsWorking())
			stdlog(TEXT("send failed with error: %d"), WSAGetLastError());
        Helpers::CloseSocket(clientSocket);
        return false;
    }

    return true;
}

bool ServerThread::Recv(void* recvbuf, int cb)
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

void ServerThread::IpcErr(CHAR* lpFormat, ...)
{
	if (!IsTerminated())
	{
		Helpers::CloseSocket(clientSocket);
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
    if (listen(listenSocket, SOMAXCONN) == SOCKET_ERROR) {
		Helpers::ErrMessageBox(TEXT("listen failed with error: %d\n Restart application"), WSAGetLastError());
		Helpers::CloseSocket(listenSocket);
		return;
    }

    //keep listening for connection
    while (true)
    {
		// Accept a client socket
        clientSocket = accept(listenSocket, NULL, NULL);
		if (listenSocket == INVALID_SOCKET && clientSocket == INVALID_SOCKET) {
            int err = WSAGetLastError();
			if (!IsTerminated())
				Helpers::ErrMessageBox(TEXT("accept failed with error: %d\n Restart application"), WSAGetLastError());
			Helpers::CloseSocket(listenSocket);
			return;
        }
        //stdlog("Accepted a client socket\n");

		::PostMessage(hwndMain, WM_COMMAND, ID_VIEW_UPDATE_STATUS, 0);
		DbgInfo* pDbgInfo = nullptr;
		std::vector<CmdNextModule> modules;
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
					IpcErr(TEXT("No start info received"));
					break;
				}
				modules.clear();
				CmdInfoSize cmd;
				std::vector<DbgModule>& arDbgModules = gSettings.m_DbgSettings.m_arDbgModules;
				cmd.infoSize = (DWORD)arDbgModules.size();
				if (!(res = Send(&cmd, sizeof(cmd))))
				{
					IpcErr(TEXT("Failed to send CmdInfoSize"));
					break;
				}
				CmdNextModule mo;
				for (DWORD i = 0; i < cmd.infoSize; i++)
				{
					mo.BaseOfDll = 0;
					strcpy_s(mo.szModName, arDbgModules[i].m_szModul);
					if (!(res = Send(&mo, sizeof(mo))))
					{
						IpcErr(TEXT("Failed to send CmdNextModule"));
						break;
					}
				}
			}
			break;
			case CMD_NEXT_MODULE:
			{
				CmdNextModule cmd;
				if (!(res = Recv(CMD_BUF_AND_SIZE(cmd))))
				{					
					IpcErr(TEXT("Failed to receive CmdNextModule"));
					break;
				}
				modules.push_back(cmd);
			}
			break;
			case CMD_PREP_INFO:
			{
				if (modules.size() == 0)
				{
					IpcErr(TEXT("No module path sent"));
					res = false;
					break;
				}
				pDbgInfo = new DbgInfo();
				for (size_t i = 0; i < modules.size(); i++)
				{
					if (!pDbgInfo->AddInfo(modules[i].BaseOfDll, modules[i].szModName))
					{
						IpcErr(TEXT("Failed to load symbols for %s"), modules[i].szModName);
						res = false;
						break;
					}
				}
				if (res)
				{
					pDbgInfo->FuncInfo()->Sort();
					CmdInfoSize cmd;
					cmd.infoSize = pDbgInfo->FuncInfo()->Count();
					if (!(res = Send(&cmd, sizeof(cmd))))
					{
						IpcErr(TEXT("Failed to send CmdInfoSize"));
						break;
					}
					FUNC_ADDR fa;
					for (DWORD i = 0; i < cmd.infoSize && res; i++)
					{
						DbgFuncInfo *p = pDbgInfo->FuncInfo()->Get(i);
						fa.addrStart = p->GetAddrStart();
						fa.addrEnd = p->GetAddrEnd();
						fa.attr = p->attr;
						if (!(res = Send(&fa, sizeof(fa))))
						{
							IpcErr(TEXT("Failed to send DbgFuncInfo"));
							break;
						}
					}
				}
				if (!res)
				{
					delete pDbgInfo;
					pDbgInfo = nullptr;
				}
			}
			break;
			default:
				IpcErr(TEXT("Unknown command %d"), command);
				res = false;
				break;
			}

            if (!res)
            {
                break;
            }

            if (command == CMD_PREP_INFO)
            {
                break;
            }
        }

		if (pDbgInfo != nullptr)
		{
			memcpy(pDbgInfo->appPath, cmdStart.szAppName, MAX_PATH);
			pDbgInfo->appPath[MAX_PATH] = 0;
			pDbgInfo->Pid = cmdStart.Pid;
			clientSocket = INVALID_SOCKET;
			gArchive.RegisterApp(pDbgInfo);
		}
		Helpers::CloseSocket(clientSocket);

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
		Helpers::SysErrMessageBox(TEXT("Bind failed : %d"));
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
	TRACE_BUF traceBuf;
	struct sockaddr_in si_other;
	slen = sizeof(si_other);

	while (true)
	{
		//try to receive data, this is a blocking call
		if ((cb = recvfrom(traceSocket, (char*)(&traceBuf), sizeof(traceBuf), 0, (struct sockaddr *) &si_other, &slen)) < 0)
		{
			if (!IsTerminated())
				Helpers::SysErrMessageBox(TEXT("recvfrom failed"));
			break;
		}
		if (cb < 3 * sizeof(DWORD) || traceBuf.cb > UDP_BUF_SIZE)
		{
			Helpers::SysErrMessageBox(TEXT("bad udp package size %d"), cb);
			break;
		}
		CmdLog* pLog = (CmdLog*)(traceBuf.buf);
		cb_read = 0;
		cb_cmd = UDP_BUF_SIZE;
		if (pLog->cmd == CMD_FLOW)
			cb_cmd = sizeof(CmdFlow);
		else if (pLog->cmd == CMD_TRACE)
			cb_cmd = ((CmdTrace*)pLog)->CmdTraceSize();
		while ( (cb_read + cb_cmd) <= traceBuf.cb )
		{
			if (!gArchive.append(traceBuf.Pid, pLog))
			{
				Helpers::SysErrMessageBox(TEXT("Failed to add command %d"), pLog->cmd);
				break;
			}
			cb_read += cb_cmd;
			cb_cmd = UDP_BUF_SIZE;
			pLog = (CmdLog*)(traceBuf.buf + cb_read);

			if (pLog->cmd == CMD_FLOW)
				cb_cmd = sizeof(CmdFlow);
			else if (pLog->cmd == CMD_TRACE)
				cb_cmd = ((CmdTrace*)pLog)->CmdTraceSize();
		}
		if (cb_read != traceBuf.cb)
		{
			Helpers::SysErrMessageBox(TEXT("Bad log data %d %d"), cb_read, cb);
			break;
		}
	}
	Helpers::CloseSocket(traceSocket);
}