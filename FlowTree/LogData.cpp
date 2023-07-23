#include "stdafx.h"
#include "Archive.h"
#include "Settings.h"
#include "Helpers.h"

bool LOG_NODE::isSynchronized(LOG_NODE* pSyncNode)
{
    FLOW_NODE* pPeer = getPeer();
    return pSyncNode && (this == pSyncNode || pPeer == pSyncNode || isParent(pSyncNode) || (pPeer && pPeer->isParent(pSyncNode)));
}

FLOW_NODE* LOG_NODE::getPeer()
{
    if (isFlow())
        return ((FLOW_NODE*)this)->peer;
    else
        return nullptr;
}

DWORD FLOW_NODE::cbFnName(bool forceFullName)
{ 
	DbgFuncInfo* p = GetFuncInfo();
	if (!p)
		return 0;
	else if (forceFullName || gSettings.FullFnName())
		return p->cb_fnName;
	else
		return p->cb_shortFnName;
}
char* FLOW_NODE::fnName(bool forceFullName)
{ 
	DbgFuncInfo* p = GetFuncInfo();
	if (!p)
		return "?";
	else if (forceFullName || gSettings.FullFnName())
		return p->fnName;
	else
		return p->shortFnName;
}
void FLOW_NODE::CopyFuncNamme()
{
	DbgFuncInfo* p = GetFuncInfo();
	Helpers::CopyToClipboard(NULL, p->fnName, -1);
}
void FLOW_NODE::CopyFuncInfo()
{
	const int cMaxBuf = 4 * 1024;
	CHAR pBuf[cMaxBuf + 1];
	int cb = 0;
	cb += _sntprintf_s(pBuf + cb, cMaxBuf - cb, cMaxBuf - cb, TEXT("funcId: \t\t%ul\n"), funcId);
	cb += _sntprintf_s(pBuf + cb, cMaxBuf - cb, cMaxBuf - cb, TEXT("funcAddr: \t\t%llu\n"), funcAddr);
	cb += _sntprintf_s(pBuf + cb, cMaxBuf - cb, cMaxBuf - cb, TEXT("callAddr: \t\t%llu\n"), callAddr);

	DbgFuncInfo* p = GetFuncInfo();
	if (0 && p)
	{
		cb += _sntprintf_s(pBuf + cb, cMaxBuf - cb, cMaxBuf - cb, TEXT("fnName: \t\t%s\n"), p->fnName);
		cb += _sntprintf_s(pBuf + cb, cMaxBuf - cb, cMaxBuf - cb, TEXT("shortFnName: \t%s\n"), p->shortFnName);
		cb += _sntprintf_s(pBuf + cb, cMaxBuf - cb, cMaxBuf - cb, TEXT("attr:  \t\t\t%X\n"), p->attr);
		cb += _sntprintf_s(pBuf + cb, cMaxBuf - cb, cMaxBuf - cb, TEXT("cLine: \t\t\t%d\n"), p->cLine);
		cb += _sntprintf_s(pBuf + cb, cMaxBuf - cb, cMaxBuf - cb, TEXT("AddrStart: \t\t%llu\n"), p->GetAddrStart());
		cb += _sntprintf_s(pBuf + cb, cMaxBuf - cb, cMaxBuf - cb, TEXT("AddrEnd: \t\t%llu\n"), p->GetAddrEnd());
		DbgLineInfo* pLineInfo = p->pLineInfo;
		for (WORD i = 0; i < p->cLine && cb < cMaxBuf; i++)
		{ 
			char ch = ' ';
			if (pLineInfo[i].addr <= funcAddr && pLineInfo[i].addr + pLineInfo[i].dwLength > funcAddr)
				ch = '*';
			cb += _sntprintf_s(pBuf + cb, cMaxBuf - cb, cMaxBuf - cb, TEXT("%c Line %d: addr: %llu dwLength:%lu line:%lu srcName: %s srcShortName: %s\n"), ch, i, pLineInfo[i].addr, pLineInfo[i].dwLength, pLineInfo[i].line, pLineInfo[i].srcName, pLineInfo[i].srcShortName);
		}

	}
	pBuf[cb] = 0;
	Helpers::CopyToClipboard(NULL, pBuf, cb);
}

char* LOG_NODE::getFnName(bool forceFullName)
{
    if (isFlow())
    {
		return  ((FLOW_NODE*)this)->fnName(forceFullName);
	}
    else if (isTrace())
    {
        return  ((TRACE_NODE*)this)->fnName();
    }
    else
    {
        return "";
    }
}
int LOG_NODE::getFnNameSize(bool forceFullName)
{
	if (isFlow())
	{
		return  ((FLOW_NODE*)this)->cbFnName(forceFullName);
	}
	else if (isTrace())
	{
		return  ((TRACE_NODE*)this)->cb_fn_name;
	}
	else
	{
		return 0;
	}

}
int LOG_NODE::getTraceText(char* pBuf, int max_cb_trace)
{
    int cb = 0;
    int max_cb_trace_2 = max_cb_trace - 5; //space for elipces and zero terminator
    if (isTrace())
    {
        TRACE_CHANK* chank = ((TRACE_NODE*)this)->getFirestChank();
        bool truncated = false;
        while (chank && cb < max_cb_trace_2 && !truncated)
        {
            int cb_chank_trace = chank->len;
            if (max_cb_trace_2 - cb < chank->len)
            {
                truncated = true;
                cb_chank_trace = max_cb_trace_2 - cb;
            }
            memcpy(pBuf + cb, chank->trace, cb_chank_trace);
            cb += cb_chank_trace;
            pBuf[cb] = 0;
            chank = chank->next_chank;
        }
        if (truncated)
            cb += _sntprintf_s(pBuf + cb, max_cb_trace - cb, max_cb_trace - cb, "...");
    }
    return cb;
}

void FLOW_NODE::addToTree()
{
    if (threadNode->curentFlow == NULL)
    {
		threadNode->add_thread_child(this, ROOT_CHILD);
    }
    else
    {
        FLOW_NODE* lastFlowNode = threadNode->curentFlow;
        if (isEnter())
        {
            if (lastFlowNode->peer || !lastFlowNode->isEnter())
            {
				threadNode->add_thread_child(this, ROOT_CHILD);
            }
            else
            {
				threadNode->add_thread_child(this, LATEST_CHILD);
            }
        }
        else
        {
			while ((void*)lastFlowNode != (void*)threadNode)
			{
				if (lastFlowNode->peer == NULL && lastFlowNode->isEnter() && lastFlowNode->funcId == funcId)
				{
					lastFlowNode->peer = this;
                    peer = lastFlowNode;
                    if (lastFlowNode->callAddr == 0)
                        lastFlowNode->callAddr = this->callAddr;
                    if (lastFlowNode->parent != threadNode) {
						threadNode->curentFlow = (FLOW_NODE*)(lastFlowNode->parent);
					}
					break;
				}
				lastFlowNode = (FLOW_NODE*)(lastFlowNode->parent);
			}
			if ((void*)lastFlowNode == (void*)threadNode)
			{
                if (lastFlowNode->isEnter())
                {
					threadNode->add_thread_child(this, ROOT_CHILD);
                }
                else
                {
					threadNode->add_thread_child(this, LATEST_CHILD);
                }
            }
        }
    }
}

int LOG_NODE::getTreeImage()
{
    if (isRoot())
    {
        return 0;//IDI_ICON_TREE_ROOT
    }
	else if (isApp())
	{
		return 1;//IDI_ICON_TREE_APP
	}
	else if (isThread())
    {
        return 2;//IDI_ICON_TREE_THREAD
    }
    else if (isFlow())
    {
        FLOW_NODE* This = (FLOW_NODE*)this;
        return This->peer ? 3 : (This->isEnter() ? 4 : 5);//IDI_ICON_TREE_PAIRED, IDI_ICON_TREE_ENTER, IDI_ICON_TREE_EXIT
    }
    else
    {
        ATLASSERT(FALSE);
        return 0;//IDI_ICON_TREE_ROOT
    }
}

FLOW_NODE* LOG_NODE::getSyncNode()
{
    LOG_NODE* pNode = this;
    while (pNode && !pNode->isTreeNode() && (pNode->getPeer() || pNode->parent))
    {
        if (pNode->getPeer())
        {
            pNode = pNode->getPeer();
            break;
        }
        else
        {
            pNode = pNode->parent;
        }
    }
    if (pNode && pNode->isFlow() || pNode->isThread())
        return (FLOW_NODE*)pNode;
    else
        return NULL;
}

CHAR* LOG_NODE::getTreeText(int* cBuf, bool extened)
{
    const int cMaxBuf = 1024;
    static CHAR pBuf[cMaxBuf + 1];
    int cb = 0;
    CHAR* ret = pBuf;
    int NN = getNN();
#ifdef _DEBUG
    //cb += _sntprintf_s(pBuf + cb, cMaxBuf -cb , cMaxBuf - cb, TEXT("[%d %d %d]"), GetExpandCount(), line, lastChild ? lastChild->index : 0);
#endif
    if (this == gArchive.getRootNode())
    {
        cb += _sntprintf_s(pBuf + cb, cMaxBuf -cb , cMaxBuf - cb, TEXT("..."));
    }
	else if (isApp())
	{
		APP_NODE* This = (APP_NODE*)this;
		cb += _sntprintf_s(pBuf + cb, cMaxBuf -cb , cMaxBuf - cb, This->appName);
		if (gSettings.ColPID())
			cb += _sntprintf_s(pBuf + cb, cMaxBuf -cb , cMaxBuf - cb, TEXT("[%d]"), getPid());
	}
	else if (isThread())
    {
		if (gSettings.ColPID())
			cb += _sntprintf_s(pBuf + cb, cMaxBuf -cb , cMaxBuf - cb, TEXT("[%d-%d]"), getThreadNN(), getPid());
		else
			cb += _sntprintf_s(pBuf + cb, cMaxBuf -cb , cMaxBuf - cb, TEXT("[%d]"), getThreadNN());
#ifdef SHOW_CHILD_COUNT
		cb += _sntprintf_s(pBuf + cb, cMaxBuf -cb , cMaxBuf - cb, TEXT(" (%d)"), cChild); //gArchive.index(this) NN
#endif
	}
    else if (isFlow())
    {
        FLOW_NODE* This = (FLOW_NODE*)this;
        int cb_name = This->getFnNameSize();
        memcpy(pBuf + cb, This->getFnName(), cb_name);
        cb += cb_name;
        if (extened)
        {
            if (gSettings.ColNN() && NN)
                cb += _sntprintf_s(pBuf + cb, cMaxBuf -cb , cMaxBuf - cb, TEXT(" (%d)"), NN); //gArchive.index(this) NN
            if (gSettings.ShowElapsedTime() && This->getPeer())
            {
                _int64 sec1 = This->getTimeSec();
                _int64 msec1 = This->getTimeMSec();
                _int64 sec2 = (This->getPeer())->getTimeSec();
                _int64 msec2 = (This->getPeer())->getTimeMSec();
                cb += _sntprintf_s(pBuf + cb, cMaxBuf -cb , cMaxBuf - cb, TEXT(" (%lldms)"), (sec2 - sec1) * 1000 + (msec2 - msec1));
            }
#ifdef SHOW_CHILD_COUNT
			cb += _sntprintf_s(pBuf + cb, cMaxBuf -cb , cMaxBuf - cb, TEXT(" (%d)"), cChild); //gArchive.index(this) NN
#endif
		}
        if (gSettings.ColCallAddr())
        {
            cb += _sntprintf_s(pBuf + cb, cMaxBuf -cb , cMaxBuf - cb, TEXT(" (%llu)"), This->callAddr);
        }
        if (gSettings.FnCallLine())
        {
			cb += _sntprintf_s(pBuf + cb, cMaxBuf -cb , cMaxBuf - cb, TEXT(" (%d)"), This->GetCallLine());
        }
        pBuf[cb] = 0;
    }
    else
    {
        ATLASSERT(FALSE);
    }
	if (cb > cMaxBuf)
		cb = cMaxBuf;
    pBuf[cb] = 0;
    if (cBuf)
        *cBuf = cb;
    return ret;
}


CHAR* LOG_NODE::getListText(int* cBuf, LIST_COL col)
{
    const int MAX_BUF_LEN = 2 * MAX_LOG_LEN - 1;
    static CHAR pBuf[MAX_BUF_LEN + 1];
    int& cb = *cBuf;
    CHAR* ret = pBuf;

    cb = 0;
    pBuf[0] = 0;

    if (col == NN_COL)
    {
        int NN = getNN(); // gArchive.index(this); // getNN();
        if (NN)
            cb += _sntprintf_s(pBuf, MAX_BUF_LEN, MAX_BUF_LEN, TEXT("%d"), NN);
        int prev_NN = prevSibling ? prevSibling->getNN() : 0;
    }
    else if (col == APP_COLL)
    {
        cb += _sntprintf_s(pBuf, MAX_BUF_LEN, MAX_BUF_LEN, TEXT("%s"), threadNode->pAppNode->appName);
    }
    else if (col == PID_COL)
    {
        if (gSettings.ColPID())
            cb += _sntprintf_s(pBuf, MAX_BUF_LEN, MAX_BUF_LEN, TEXT("[%d-%d]"), getThreadNN(), getPid());
        else
            cb += _sntprintf_s(pBuf, MAX_BUF_LEN, MAX_BUF_LEN, TEXT("[%d]"), getThreadNN());
    }
    else if (col == TIME_COL)
    {
        if (isInfo())
        {
            struct tm timeinfo;
            DWORD sec = getTimeSec();
            DWORD msec = getTimeMSec();
            time_t rawtime = sec;
            if (0 == localtime_s(&timeinfo, &rawtime))
                cb += (int)strftime(pBuf, MAX_BUF_LEN, "%H:%M:%S", &timeinfo);

            //struct tm * timeinfo2;
            //time_t rawtime2 = _time32(NULL);
            //timeinfo2 = localtime(&rawtime2);
            //cb += strftime(pBuf + cb, MAX_BUF_LEN, " %H.%M.%S", timeinfo2);
            //int i = 4294967290 / 10000000;
            //        6553500000
            cb += _sntprintf_s(pBuf + cb, MAX_BUF_LEN - cb, MAX_BUF_LEN - cb, TEXT(".%03d"), msec);
        }
    }
    else if (col == FUNC_COL)
    {
        if (isTrace())
        {
            TRACE_NODE* This = (TRACE_NODE*)this;
            cb += This->getFnNameSize();
            memcpy(pBuf, This->getFnName(), cb);
            pBuf[cb] = 0;
        }
        else if (isFlow())
        {
            FLOW_NODE* This = (FLOW_NODE*)this;
            cb += This->getFnNameSize();
            memcpy(pBuf, This->getFnName(), cb);
            pBuf[cb] = 0;
        }
    }
    else if (col == CALL_LINE_COL)
    {
        cb += _sntprintf_s(pBuf + cb, MAX_BUF_LEN - cb, MAX_BUF_LEN - cb, TEXT("%d"), getCallLine());
        if (isFlow())
            cb += _sntprintf_s(pBuf + cb, MAX_BUF_LEN - cb, MAX_BUF_LEN - cb, TEXT(" %d"), ((FLOW_NODE*)this)->GetFuncLine());
    }
    else if (col == LOG_COL)
    {
        if (isFlow())
        {
            FLOW_NODE* This = (FLOW_NODE*)this;
            cb += This->getFnNameSize();
            memcpy(pBuf, This->getFnName(), cb);
            pBuf[cb] = 0;
        }
        else if (isTrace())
        {
            cb = 0;
            TRACE_NODE* This = (TRACE_NODE*)this;
            TRACE_CHANK* pChank = This->getFirestChank();
            if (pChank->next_chank)
            {
                while (pChank) {
                    int c = pChank->len;
                    if ((c + cb) > MAX_BUF_LEN)
                    {
                        c -= (MAX_BUF_LEN - c - cb);
                        if (c <= 0)
                            break;
                    }

                    memcpy(pBuf + cb, pChank->trace, c);
                    cb += c;
                    pChank = pChank->next_chank;
                }
                pBuf[cb] = 0;
            }
            else
            {
                cb = pChank->len;
                ret = pChank->trace;
            }
            //ret = "  2";
            //cb = 3;
        }
    }

    if (cb > MAX_BUF_LEN)
        cb = MAX_BUF_LEN;
    pBuf[cb] = 0;
    return ret;
}

int LOG_NODE::getNN()
{
	return isInfo() ? ((INFO_NODE*)this)->nn : 0;
}
LONG LOG_NODE::getTimeSec()
{
    return isInfo() ? ( ((INFO_NODE*)this)->tickCount - gArchive.TickCount() ) / 1000 : 0LL;
}
LONG LOG_NODE::getTimeMSec()
{
	return isInfo() ? (((INFO_NODE*)this)->tickCount - gArchive.TickCount()) % 1000 : 0LL;
}
int LOG_NODE::getPid()
{
    if (isApp())
        return ((APP_NODE*)this)->pDbgInfo->Pid;
    else if (threadNode)
        return threadNode->tid;
	else
		return 0;
}
int LOG_NODE::getThreadNN()
{
	return  threadNode ? threadNode->threadNN : 0;
}
DWORD64 LOG_NODE::getCallAddr()
{
    return isFlow() ? ((FLOW_NODE*)this)->callAddr : 0;
}
int LOG_NODE::getCallLine()
{
	if (isFlow())
		return ((FLOW_NODE*)this)->GetCallLine();
	else if (isTrace())
		return ((TRACE_NODE*)this)->GetCallLine();
	else
		return 0;
}

void LOG_NODE::CollapseExpand(BOOL expand)
{
    expanded = expand;
    CalcLines();
}

void LOG_NODE::CalcLines()
{
    LOG_NODE* pNode = gArchive.getRootNode();
    int l = -1;
    //stdlog("CalcLines %d\n", GetTickCount());
    do
    {
        pNode->cExpanded = 0;
        pNode->pathExpanded = (!pNode->parent || (pNode->parent->pathExpanded && pNode->parent->expanded));
        if (pNode->parent)
            pNode->parent->cExpanded++;
        pNode->line = ++l;
        //stdlog("index = %d e = %d l = %d %s\n", pNode->index, pNode->cExpanded, pNode->line, pNode->getTreeText());

        if (pNode->firstChild && pNode->expanded)
        {
            pNode = pNode->firstChild;
        }
        else if (pNode->nextSibling)
        {
            pNode = pNode->nextSibling;
        }
        else
        {
            while (pNode->parent)
            {
                pNode = pNode->parent;
                //stdlog("\t index = %d e = %d l = %d %s\n", pNode->index, pNode->cExpanded, pNode->line, pNode->getTreeText());
                if (pNode->parent && pNode->cExpanded)
                {
                    pNode->parent->cExpanded += pNode->cExpanded;
                    //stdlog("\t\t index = %d e = %d l = %d %s\n", pNode->parent->index, pNode->parent->cExpanded, pNode->parent->line, pNode->parent->getTreeText());
                }
                if (pNode == gArchive.getRootNode())
                    break;
                if (pNode->nextSibling)
                {
                    pNode = pNode->nextSibling;
                    break;
                }
            }
        }
    } while (gArchive.getRootNode() != pNode);
    //stdlog("CalcLines %d\n", GetTickCount());
}

void LOG_NODE::CollapseExpandAll(bool expand)
{
    //stdlog("CollapseExpandAll %d\n", GetTickCount());
    LOG_NODE* pNode = this;
    LOG_NODE* pNode0 = pNode;
    do
    {
        pNode->expanded = (pNode->lastChild && expand) ? 1 : 0;

        if (pNode->firstChild)
        {
            pNode = pNode->firstChild;
        }
        else if (pNode->nextSibling)
        {
            pNode = pNode->nextSibling;
        }
        else
        {
            while (pNode->parent)
            {
                pNode = pNode->parent;
                if (pNode == pNode0)
                    break;
                if (pNode->nextSibling)
                {
                    pNode = pNode->nextSibling;
                    break;
                }
            }
        }
    } while (pNode0 != pNode);
    //stdlog("CollapseExpandAll %d\n", GetTickCount());
    CalcLines();
}


inline DbgFuncInfo* FLOW_NODE::GetFuncInfo()
{
	return getApp()->GetFuncInfo(funcId);
}
int FLOW_NODE::GetFuncLine()
{
	DbgFuncInfo* p = GetFuncInfo();
	if (p) {
		DbgLineInfo* pLineInfo = p->GetLineInfo(funcAddr);
		return pLineInfo->line;
	}
	return 0;
}
char* FLOW_NODE::getFuncSrc(bool fullPath)
{
	char* src = "";
	DbgFuncInfo* p = GetFuncInfo();
	if (p) {
		DbgLineInfo* pLineInfo = p->GetLineInfo(funcAddr);
		if (fullPath)
			src = pLineInfo->srcName;
		else
			src = pLineInfo->srcShortName;
	}
	return src;
}
char* FLOW_NODE::getCallSrc(bool fullPath)
{
	char* src = "";
	DbgFuncInfo* p = getApp()->GetFuncInfoByAddr(callAddr);
	if (p) {		
		DbgLineInfo* pLineInfo = p->pLineInfo;
		if (fullPath)
			src = pLineInfo->srcName;
		else
			src = pLineInfo->srcShortName;
	}
	return src;
}
int FLOW_NODE::GetCallLine()
{
	if (callLine != -1)
	{
		callLine = -1;
		DbgFuncInfo* p = getApp()->GetFuncInfoByAddr(callAddr);
		if (p) {
			callLine = p->GetLineInfo(callAddr)->line;
		}
	}
	return callLine;
}
DbgFuncInfo* APP_NODE::GetFuncInfoByAddr(DWORD64 addr)
{
	PtrArray<DbgFuncInfo>* p = pDbgInfo->FuncInfo();
	DWORD l = 0, u = p->Count(), idx, funcId = INFINITE;
	while (l < u)
	{
		idx = (l + u) / 2;
		if (addr < p->Get(idx)->GetAddrStart()) //comparison < 0
			u = idx;
		else if (addr > p->Get(idx)->GetAddrEnd()) //comparison > 0
			l = idx + 1;
		else
		{
			funcId = idx; //found 
			break;
		}
	}
	return p->Get(funcId);
}

//return true if check changed
bool LOG_NODE::CheckAll(bool check, bool recursive)
{
    bool checkChanged = false;
    checkChanged = CheckNode(check) || checkChanged;
    LOG_NODE* pNode = firstChild;
    while (pNode && pNode->hasCheckBox)
    {
        checkChanged = pNode->CheckNode(check) || checkChanged;
        if (recursive)
            checkChanged = pNode->CheckAll(check, recursive) || checkChanged;
        pNode = pNode->nextSibling;
    }
    return checkChanged;
}

bool LOG_NODE::ShowOnlyThis()
{
    bool checkChanged = false;
    checkChanged = getRoot()->CheckAll(false) || checkChanged;
    if (isRoot())
    {
        checkChanged = CheckAll(true, true) || checkChanged;
    }
    else if (isApp())
    {
        checkChanged = CheckAll(true) || checkChanged;
    }
    else if (isThread())
    {
        checkChanged = getApp()->CheckAll(false) || checkChanged;
        checkChanged = getApp()->CheckNode(true) || checkChanged;
        checkChanged = getTrhread()->CheckNode(true) || checkChanged;
    }
    else
    {
        ATLASSERT(FALSE);
    }
    return checkChanged;
}

bool LOG_NODE::CheckNode(bool check)
{
    bool checkChanged = false;
    if (hasCheckBox)
    {
        if (checked != check)
            checkChanged = true;
        checked = check ? 1 : 0;
    }
    return checkChanged;
}

APP_NODE* LOG_NODE::getApp() { return threadNode->pAppNode; }
THREAD_NODE* LOG_NODE::getTrhread() { return threadNode; }
ROOT_NODE* LOG_NODE::getRoot() { return (ROOT_NODE*)getApp()->parent; }