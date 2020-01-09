#pragma once
class WorkerThread
{
public:
    WorkerThread();
	virtual ~WorkerThread() {}
    DWORD getTID(){ return m_dwTID; }
	bool IsWorking() { return m_bWorking; }
	bool IsTerminated() { return m_Terminated; }
	void StartWork(LPVOID pWorkParam = 0);
    void StopWork();
    virtual void Work(LPVOID pWorkParam) = 0;
    virtual void OnThreadReady();
	virtual void OnThreadEnd() {};
	virtual void Terminate() = 0;
private:
    LPVOID m_pWorkParam;
	bool m_bWorking;
	bool m_Terminated;
	HANDLE m_hTreadEvent;
    HANDLE m_hThread;
    DWORD m_dwTID;
    static DWORD WINAPI ThreadFunk(LPVOID lpParameter);
};

class AutoReleaseThread : public WorkerThread
{
public:
	AutoReleaseThread() { refCount = 1; }
	void AddRef() { refCount++; }
	void Release() { if (--refCount == 0) delete this; }
	virtual void OnThreadEnd() { Release(); };
protected:
	virtual ~AutoReleaseThread() {
		refCount;
	};
private:
	int refCount;

};