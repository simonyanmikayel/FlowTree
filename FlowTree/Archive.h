#pragma once

#include "Dbg.h"
#include "LogData.h"

#define MAX_TRCAE_LEN (1024*2)
#define MAX_LOG_LEN MAX_TRCAE_LEN
#define MAX_RECORD_LEN (MAX_TRCAE_LEN + 2 * sizeof(ROW_LOG_REC))
struct ListedNodes;

#ifdef _BUILD_X64
const size_t MAX_BUF_SIZE = 12LL * 1024 * 1024 * 1024;
#else
const size_t MAX_BUF_SIZE = 1024 * 1024 * 1024;
#endif

struct NODE_FILTER {
	NODE_FILTER() { 
		reset(); 
	}
	void reset() { 
		tid = pid = undefined; 
	}
	void setTid(int i) { 
		tid = i; pid = undefined; 
	}
	void setPid(int i) { 
		pid = i; tid = undefined; 
	}
	bool isUndefined() { 
		return pid == undefined && tid == undefined; 
	}
	bool isTid(int i) { return tid == i; }
	bool isPid(int i) { return pid == i; }
private:
	int pid;
	int tid;
	static constexpr int undefined = -11111;
};

class Archive
{
public:
    Archive();
    ~Archive();

    void clearArchive(); 
	void onPaused();
    inline DWORD getCount();
	void lock() { EnterCriticalSection(&cs); }
	void unlock() { LeaveCriticalSection(&cs); }
	LOG_NODE* getNode(DWORD i) { return (m_pNodes && i < m_pNodes->Count()) ? (LOG_NODE*)m_pNodes->Get(i) : 0; }
    char* Alloc(DWORD cb) { return (char*)m_pTraceBuf->Alloc(cb, false); }
	LOG_NODE* append(DWORD pid, CmdLog* pLog);
	void RegisterApp(DbgInfo* pDbgInfo);
    bool IsEmpty() { return m_pNodes ==nullptr || m_pNodes->Count() == 0; }
    DWORD64 index(LOG_NODE* pNode) { return pNode - getNode(0); }
    ListedNodes* getListedNodes() { return m_listedNodes; }
    ROOT_NODE* getRootNode() { return m_rootNode; }
    static DWORD getArchiveNumber() { return archiveNumber; }
    size_t UsedMemory();
	DWORD TickCount() { return tickCount; }
    BYTE getNewBookmarkNumber() { return ++bookmarkNumber; }
    BYTE resteBookmarkNumber() { return bookmarkNumber = 0; }

private:
	inline APP_NODE* getApp(DWORD Pid);
	inline APP_NODE* addApp(DbgInfo* pDbgInfo);
	inline THREAD_NODE* addThread(APP_NODE* pAppNode, int tid);
    inline LOG_NODE* addFlow(THREAD_NODE* pThreadNode, CmdFlow *pCmdFlow);
    inline LOG_NODE* addTrace(THREAD_NODE* pThreadNode, CmdTrace *pCmdTrace, int& prcessed);
    inline THREAD_NODE*   getThread(APP_NODE* pAppNode, CmdLog* pLog);

	int nn;
	DWORD tickCount;
    static DWORD archiveNumber;
	APP_NODE* curApp;
	THREAD_NODE* curThread;
    ListedNodes* m_listedNodes;
    ROOT_NODE* m_rootNode;
    MemBuf* m_pTraceBuf;
    PtrArray<LOG_NODE>* m_pNodes;
	CRITICAL_SECTION cs;
    BYTE bookmarkNumber = 0;
};

struct ListedNodes
{
	ListedNodes() {
		ZeroMemory(this, sizeof(*this));
		m_pListBuf = new MemBuf(MAX_BUF_SIZE, 64 * 1024 * 1024);
	}
	~ListedNodes() {
		delete m_pListNodes;
		delete m_pListBuf;
	}
	LOG_NODE* getNode(DWORD i) {
		return m_pListNodes->Get(i);
	}
	void FreeListedNodes()
	{
		m_pListBuf->Free();
		delete m_pListNodes;
		archiveCount = 0;
		m_pListNodes = new PtrArray<LOG_NODE>(m_pListBuf);
	}
	size_t AllocMemory() {
		return m_pListBuf->AllocMemory();
	}
	size_t UsedMemory() {
		return m_pListBuf->UsedMemory();
	}
	DWORD Count() { return m_pListNodes->Count(); }
	void updateList(bool reset, bool flowTraceHiden, bool searchFilterOn);
	static bool isVisibleLog(LOG_NODE* pNode, bool flowTraceHiden) {
		return pNode->threadNode && pNode->isInfo() && !pNode->threadNode->isHiden()
			&& (pNode->isTrace() || !flowTraceHiden);
	}
private:
	void addNode(LOG_NODE* pNode, bool flowTraceHiden, bool searchFilterOn) {
		//DWORD64 ndx = gArchive.index(pNode);
		if (isVisibleLog(pNode, flowTraceHiden) && (pNode->lineSearchPos || !searchFilterOn))
		{
			m_pListNodes->AddPtr(pNode);
		}
	}
	DWORD archiveCount;
	PtrArray<LOG_NODE>* m_pListNodes;
	MemBuf* m_pListBuf;
};

