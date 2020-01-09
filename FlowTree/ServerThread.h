#pragma once
#include "WorkerThread.h"
#include "Dbg.h"

class TraceThread : public WorkerThread
{
public:
	TraceThread();
	virtual void Terminate();
protected:
	void Work(LPVOID pWorkParam);
	SOCKET traceSocket;
};

class ServerThread : public WorkerThread
{
public:
	ServerThread();
	virtual void Terminate();
protected:
	void Work(LPVOID pWorkParam);
	bool Send(void* sendbuf, int cb);
	bool Recv(void* recvbuf, int cb);
	void IpcErr(CHAR* lpFormat, ...);
	TraceThread traceThread;
	SOCKET clientSocket;
	SOCKET listenSocket;
};

