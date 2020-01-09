#pragma once

#include "Dbg.h"
#include "LogData.h"

#define MAX_TRCAE_LEN (1024*2)
#define MAX_RECORD_LEN (MAX_TRCAE_LEN + 2 * sizeof(ROW_LOG_REC))
struct ListedNodes;

#ifdef _BUILD_X64
const size_t MAX_BUF_SIZE = 12LL * 1024 * 1024 * 1024;
#else
const size_t MAX_BUF_SIZE = 1024 * 1024 * 1024;
#endif

struct SNAPSHOT_INFO
{
    int pos;
    CHAR descr[32];
};

struct SNAPSHOT
{
    void clear() { curSnapShot = 0;  snapShots.clear(); update(); }
    void update();
    int curSnapShot;
    std::vector<SNAPSHOT_INFO> snapShots;
    DWORD first, last;
};

class Archive
{
public:
    Archive();
    ~Archive();

    void clearArchive(); 
	void onPaused();
    DWORD getCount();
    LOG_NODE* getNode(DWORD i) { return (m_pNodes && i < m_pNodes->Count()) ? (LOG_NODE*)m_pNodes->Get(i) : 0; }
    char* Alloc(DWORD cb) { return (char*)m_pTraceBuf->Alloc(cb, false); }
    bool append(DWORD pid, CmdLog* pLog);
	void RegisterApp(DbgInfo* pDbgInfo);
    bool IsEmpty() { return m_pNodes ==nullptr || m_pNodes->Count() == 0; }
    DWORD64 index(LOG_NODE* pNode) { return pNode - getNode(0); }
    ListedNodes* getListedNodes() { return m_listedNodes; }
    ROOT_NODE* getRootNode() { return m_rootNode; }
    SNAPSHOT& getSNAPSHOT() { return m_snapshot; }
    static DWORD getArchiveNumber() { return archiveNumber; }
    size_t UsedMemory();
	DWORD TickCount() { return tickCount; }

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
    SNAPSHOT m_snapshot;
    ListedNodes* m_listedNodes;
    ROOT_NODE* m_rootNode;
    MemBuf* m_pTraceBuf;
    PtrArray<LOG_NODE>* m_pNodes;
	CRITICAL_SECTION cs;
};

struct ListedNodes
{
    ListedNodes() {
        ZeroMemory(this, sizeof(*this));
        m_pListBuf = new MemBuf(MAX_BUF_SIZE, 64 * 1024 * 1024);
    }
    ~ListedNodes() {
        delete m_pNodes;
        delete m_pListBuf;
    }
    LOG_NODE* getNode(DWORD i) {
        return m_pNodes->Get(i);
    }
    void Free()
    {
        m_pListBuf->Free();
        delete m_pNodes;
        archiveCount = 0;
        m_pNodes = new PtrArray<LOG_NODE>(m_pListBuf);
    }
    size_t UsedMemory() {
        return m_pListBuf->UsedMemory();
    }
    void addNode(LOG_NODE* pNode, BOOL flowTraceHiden) {
        DWORD64 ndx = gArchive.index(pNode);
        if (ndx >= gArchive.getSNAPSHOT().first && ndx <= gArchive.getSNAPSHOT().last && pNode->isInfo() && !pNode->threadNode->pAppNode->isHiden() && !pNode->threadNode->isHiden() && (pNode->isTrace() || !flowTraceHiden))
        {
            m_pNodes->AddPtr(pNode);
        }
    }
    DWORD Count() { return m_pNodes->Count(); }
    void applyFilter(BOOL flowTraceHiden);
    void updateList(BOOL flowTraceHiden);
private:
    DWORD archiveCount;
    PtrArray<LOG_NODE>* m_pNodes;
    MemBuf* m_pListBuf;
};

