#pragma once
#include "Buffer.h"
#include "ServerThread.h"

class Archive;
struct LOG_NODE;
struct ROOT_NODE;
struct APP_NODE;
struct THREAD_NODE;
struct TRACE_NODE;
struct FLOW_NODE;
struct ListedNodes;

extern Archive  gArchive;

typedef enum { LOG_TYPE_ENTER, LOG_TYPE_EXIT, LOG_TYPE_TRACE } ROW_LOG_TYPE;

#pragma pack(push,1)
enum THREAD_NODE_CHILD { ROOT_CHILD, LATEST_CHILD };
enum LOG_DATA_TYPE { ROOT_DATA_TYPE, APP_DATA_TYPE, THREAD_DATA_TYPE, FLOW_DATA_TYPE, TRACE_DATA_TYPE };
enum LIST_COL { NN_COL, APP_COLL, PID_COL, TIME_COL, FUNC_COL, CALL_LINE_COL, LOG_COL, MAX_COL };

struct LOG_NODE
{
    THREAD_NODE* threadNode;
    LOG_NODE* parent;
    LOG_NODE* lastChild;
    LOG_NODE* prevSibling;
    struct {
        WORD hasNewLine : 1;
        WORD hasSearchResult : 1;
        WORD hasCheckBox : 1;
        WORD checked : 1;
        WORD selected : 1;
        WORD expanded : 1;
        WORD hasNodeBox : 1;
        WORD pathExpanded : 1;
        WORD bookmark : 1;
    };
    BYTE nextChankCounter;
    int cExpanded;
    int line;
    int lineSearchPos;
    LOG_NODE* nextChankMarker;
    LOG_NODE* nextChank;
    LOG_NODE* firstChild;
    LOG_NODE* nextSibling;

    LOG_DATA_TYPE data_type;
    DWORD lengthCalculated;
    void init() { lengthCalculated = 0; }
    bool isRoot() { return data_type == ROOT_DATA_TYPE; }
	bool isApp() { return data_type == APP_DATA_TYPE; }
	bool isThread() { return data_type == THREAD_DATA_TYPE; }
    bool isFlow() { return data_type == FLOW_DATA_TYPE; }
    bool isTrace() { return data_type == TRACE_DATA_TYPE; }
    bool isInfo() { return isFlow() || isTrace(); }
    bool isChecked() { return checked; }

    void CalcLines();
    int GetExpandCount() { return expanded ? cExpanded : 0; }
    void CollapseExpand(BOOL expand);
    int GetPosInTree() { return line; }
    void CollapseExpandAll(bool expand);

#ifdef SHOW_CHILD_COUNT
	DWORD cChild;
#endif
    void add_child(LOG_NODE* pNode)
    {
        if (!firstChild)
            firstChild = pNode;
        if (lastChild)
            lastChild->nextSibling = pNode;

        nextChankCounter++;
        if (nextChankCounter == 255) // 255 is max number of nextChankCounter. then it will start from 0 
        {
            if (!nextChankMarker)
                firstChild->nextChank = pNode;
            else
                nextChankMarker->nextChank = pNode;
            nextChankMarker = pNode;
        }
        pNode->prevSibling = lastChild;
        pNode->parent = this;
        lastChild = pNode;
#ifdef SHOW_CHILD_COUNT
		cChild++;
		LOG_NODE* pParent = this->parent;
		while (pParent)
		{
			pParent->cChild++;
			pParent = pParent->parent;
		}
#endif
    }

    bool isTreeNode() { return parent && !isTrace(); }
    bool isParent(LOG_NODE* pNode) { LOG_NODE* p = parent; while (p != pNode && p) p = p->parent; return p != NULL; }
    FLOW_NODE* getPeer();
    bool isSynchronized(LOG_NODE* pSyncNode);
    int getNN();
    LONG getTimeSec();
    LONG getTimeMSec();
    int   getPid();
	int   getThreadNN();
    DWORD64 getCallAddr();
    int   getCallLine();
    CHAR* getTreeText(int* cBuf = NULL, bool extened = true);
    CHAR* getListText(int* cBuf, LIST_COL col);
    THREAD_NODE* getTrhread();
    APP_NODE* getApp();
    ROOT_NODE* getRoot();
    int getTreeImage();
    FLOW_NODE* getSyncNode();
	char* getFnName(bool forceFullName = false);
    int getFnNameSize(bool forceFullName = false);
    int getTraceText(char* pBuf, int max_cb_trace);
    bool CheckAll(bool check, bool recursive = false);
    bool ShowOnlyThis();
    bool CheckNode(bool check);

};

struct ROOT_NODE : LOG_NODE
{
};

struct APP_NODE : LOG_NODE
{
	int threadCount;
	DWORD lastNN;
	DWORD lost;
	DbgInfo* pDbgInfo;
	char* appPath;
	char* appName;
	DbgFuncInfo* GetFuncInfo(DWORD i) { return pDbgInfo->FuncInfo()->Get(i); }
	DbgFuncInfo* GetFuncInfoByAddr(DWORD64 callAddr);
};

struct THREAD_NODE : LOG_NODE
{
	APP_NODE* pAppNode;
	FLOW_NODE* curentFlow;
    TRACE_NODE* latestTrace;
	int tid;
	int threadNN;
    char COLOR_BUF[10];
    int  cb_color_buf;

    void add_thread_child(FLOW_NODE* pNode, THREAD_NODE_CHILD type)
    {
        LOG_NODE* pParent = NULL;
        if (type == ROOT_CHILD)
            pParent = this;
        else
            pParent = (LOG_NODE*)curentFlow;
        ATLASSERT(pParent != NULL);
        pParent->add_child((LOG_NODE*)pNode);
        curentFlow = pNode;
    }
    bool isHiden() { return !isChecked() || !pAppNode->isChecked(); }
};

struct INFO_NODE : LOG_NODE
{
	int nn;
    int log_type;
	DWORD tickCount;
    bool isEnter() { return log_type == LOG_TYPE_ENTER; }
    bool isTrace() { return log_type == LOG_TYPE_TRACE; }
	void SetCallLine( int i) { callLine = i; }
protected:
	int callLine;
};

struct FLOW_NODE : INFO_NODE
{
    FLOW_NODE* peer;
	DWORD funcId;
	DWORD64 funcAddr;
	DWORD64 callAddr;
	DWORD cbFnName(bool forceFullName = false);
	char* fnName(bool forceFullName = false);
	DbgFuncInfo* GetFuncInfo();
	int GetFuncLine();
	char* getFuncSrc(bool fullPath);
	int GetCallLine();
	char* getCallSrc(bool fullPath);
	bool isOpenEnter() { return isEnter() && peer == 0; }
    void addToTree();
	void CopyFuncNamme();
	void CopyFuncInfo();
};

struct TRACE_CHANK
{
    TRACE_CHANK* next_chank;
    int len;
    char trace[1];
};

struct TRACE_NODE : INFO_NODE
{
    int MsgType;
    int cb_trace;
	int cb_fn_name;
	int GetCallLine() { return callLine; }
	char* fnName() { return ((char*)(this)) + sizeof(TRACE_NODE); }
	TRACE_CHANK* getFirestChank() { return (TRACE_CHANK*)(fnName() + cb_fn_name); }
    TRACE_CHANK* getLastChank() { TRACE_CHANK* p = getFirestChank(); while (p->next_chank) p = p->next_chank; return p; }
};

#pragma pack(pop)

