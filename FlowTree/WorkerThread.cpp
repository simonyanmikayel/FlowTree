#include "stdafx.h"
#include "WorkerThread.h"


WorkerThread::WorkerThread(void)
{
	m_bWorking = false;
	m_Terminated = false;
	m_hThread = 0;
}

void WorkerThread::StopWork()
{
	if (m_bWorking) {
		HANDLE hThread = m_hThread;
		m_bWorking = false; // signal end of work
		m_Terminated = true;
		Terminate();
		while (WAIT_TIMEOUT == WaitForSingleObject(hThread, 0))
		{
			//MSG msg;
			//while (PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
			//{
			//    TranslateMessage(&msg);
			//    DispatchMessage(&msg);
			//}
			Sleep(0); // alow Worker thread complite its waorks
		}
	}
}

void WorkerThread::StartWork(LPVOID pWorkParam /*=0*/)
{
	StopWork();

	m_pWorkParam = pWorkParam;
	m_hTreadEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	m_hThread = CreateThread(0, 0, ThreadFunk, this, 0, &m_dwTID);
	WaitForSingleObject(m_hTreadEvent, INFINITE);
	CloseHandle(m_hTreadEvent);
}

DWORD WINAPI WorkerThread::ThreadFunk(LPVOID lpParameter)
{
	WorkerThread* This = (WorkerThread*)lpParameter;
	HANDLE hThread = This->m_hThread;

	This->m_bWorking = true;

	SetEvent(This->m_hTreadEvent);
	This->Work(This->m_pWorkParam);

	This->m_bWorking = false;
	CloseHandle(hThread);
	This->OnThreadEnd();

	return 0;
}

void WorkerThread::OnThreadReady()
{
	SetEvent(m_hTreadEvent);
}

