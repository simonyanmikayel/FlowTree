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

// DWORD FLOW_NODE::cbFnName(bool forceFullName)
// { 
//     ModuleData* pModuleData = GetModuleData();
//     if (!pModuleData)
//         return 0;
//     return pModuleData->cbFnName(funcId, forceFullName);
// }

void FLOW_NODE::CopyFuncNamme()
{
	Helpers::CopyToClipboard(NULL, getFnName(true), -1);
}

void FLOW_NODE::CopyFuncInfo()
{
	const int cMaxBuf = 4 * 1024;
	CHAR pBuf[cMaxBuf + 1];
	int cb = 0;
    cb += _sntprintf_s(pBuf + cb, cMaxBuf - cb, cMaxBuf - cb, TEXT("moduleId: \t\t%d\n"), moduleId);
	cb += _sntprintf_s(pBuf + cb, cMaxBuf - cb, cMaxBuf - cb, TEXT("funcId: \t\t%ul\n"), funcId);
	cb += _sntprintf_s(pBuf + cb, cMaxBuf - cb, cMaxBuf - cb, TEXT("funcAddr: \t\t%llu\n"), funcAddr);
	cb += _sntprintf_s(pBuf + cb, cMaxBuf - cb, cMaxBuf - cb, TEXT("callAddr: \t\t%llu\n"), callAddr);

	pBuf[cb] = 0;
	Helpers::CopyToClipboard(NULL, pBuf, cb);
}

char* LOG_NODE::getFnName(bool forceFullName)
{
    if (isFlow())
    {
        FLOW_NODE* This = (FLOW_NODE*)this;
        ModuleData* pModuleData = This->GetModuleData();
        if (!pModuleData)
            return 0;
        return pModuleData->fnName(This->funcId, forceFullName);
	}
    else if (isTrace())
    {
        return  ((TRACE_NODE*)this)->tarce_fnName();
    }
    else
    {
        return "";
    }
}
// int LOG_NODE::getFnNameSize(bool forceFullName)
// {
// 	if (isFlow())
// 	{
// 		return  ((FLOW_NODE*)this)->cbFnName(forceFullName);
// 	}
// 	else if (isTrace())
// 	{
// 		return  ((TRACE_NODE*)this)->cb_fn_name;
// 	}
// 	else
// 	{
// 		return 0;
// 	}

// }

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
        if (isEnterType())
        {
            if (lastFlowNode->peer || !lastFlowNode->isEnterType())
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
				if (lastFlowNode->peer == NULL && lastFlowNode->isEnterType() && lastFlowNode->moduleId == moduleId && lastFlowNode->funcId == funcId)
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
                if (lastFlowNode->isEnterType())
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
        return This->peer ? 3 : (This->isEnterType() ? 4 : 5);//IDI_ICON_TREE_PAIRED, IDI_ICON_TREE_ENTER, IDI_ICON_TREE_EXIT
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
        cb += _sntprintf_s(pBuf + cb, cMaxBuf -cb , cMaxBuf - cb, TEXT("%s"), This->getFnName());
        if (extened)
        {
            if (gSettings.ColNN() && NN)
                cb += _sntprintf_s(pBuf + cb, cMaxBuf -cb , cMaxBuf - cb, TEXT(" (%d)"), NN); //gArchive.index(this) NN
            if (gSettings.ShowElapsedTime() && This->getPeer())
            {
                DWORD tickCount1 = This->tickCount;
                DWORD tickCount2 = (This->getPeer())->tickCount;
                cb += _sntprintf_s(pBuf + cb, cMaxBuf -cb , cMaxBuf - cb, TEXT(" (%dms)"), tickCount2 - tickCount1);
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
			cb += _sntprintf_s(pBuf + cb, cMaxBuf -cb , cMaxBuf - cb, TEXT(" (%d)"), This->getCallLine());
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
            INFO_NODE* This = (INFO_NODE*)this;
            DWORD tickCount1 = gArchive.TickCount();
            DWORD tickCount2 = This->tickCount;
            cb += _sntprintf_s(pBuf + cb, MAX_BUF_LEN - cb, MAX_BUF_LEN - cb, TEXT("%d"), tickCount2 - tickCount1);
        }
    }
    else if (col == FUNC_COL)
    {
        if (isTrace())
        {
            TRACE_NODE* This = (TRACE_NODE*)this;
            cb += _sntprintf_s(pBuf + cb, MAX_BUF_LEN - cb, MAX_BUF_LEN - cb, TEXT("%s"), This->getFnName());
        }
        else if (isFlow())
        {
            FLOW_NODE* This = (FLOW_NODE*)this;
            cb += _sntprintf_s(pBuf + cb, MAX_BUF_LEN - cb, MAX_BUF_LEN - cb, TEXT("%s"), This->getFnName());
        }
    }
    else if (col == CALL_LINE_COL)
    {
        if (isInfo())
            cb += _sntprintf_s(pBuf + cb, MAX_BUF_LEN - cb, MAX_BUF_LEN - cb, TEXT("%d"), ((INFO_NODE*)this)->getCallLine());
        if (isFlow())
            cb += _sntprintf_s(pBuf + cb, MAX_BUF_LEN - cb, MAX_BUF_LEN - cb, TEXT(" %d"), ((FLOW_NODE*)this)->GetFuncLine());
    }
    else if (col == LOG_COL)
    {
        if (isFlow())
        {
            FLOW_NODE* This = (FLOW_NODE*)this;
            cb += _sntprintf_s(pBuf + cb, MAX_BUF_LEN - cb, MAX_BUF_LEN - cb, TEXT("%s"), This->getFnName());
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
    {
        if (((INFO_NODE*)this)->callLine != -1)
        {
            ((INFO_NODE*)this)->callLine = -1;
            WORD moduleId;
            DWORD funcId;
            if (getApp()->findAddr(((FLOW_NODE*)this)->callAddr, funcId, moduleId))
            {
                ModuleData* pModuleData = getApp()->pDbgInfo->arModuleData[moduleId];
                DbgLineInfo* pLineInfo = pModuleData->GetLineInfo(funcId, ((FLOW_NODE*)this)->callAddr);
                if (pLineInfo)
                    ((INFO_NODE*)this)->callLine = pLineInfo->line;
            }
        }
        return ((INFO_NODE*)this)->callLine;
    }
    else if (isTrace())
    {
        return ((INFO_NODE*)this)->callLine;
    }
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

inline ModuleData* FLOW_NODE::GetModuleData()
{
    if (moduleId < getApp()->pDbgInfo->arModuleData.size())
    {
        return getApp()->pDbgInfo->arModuleData[moduleId];
    }
    else
        return nullptr;
}

DbgLineInfo* FLOW_NODE::GetLineInfo() {
    ModuleData* pModuleData = GetModuleData();
    if (!pModuleData)
        return 0;
    return pModuleData->GetLineInfo(funcId, funcAddr);
}

int FLOW_NODE::GetFuncLine()
{
    DbgLineInfo* pLineInfo = GetLineInfo();
    if(!pLineInfo)
        return 0;
    return pLineInfo->line;
}

char* FLOW_NODE::getFuncSrc(bool fullPath)
{
    ModuleData* pModuleData = GetModuleData();
    if (!pModuleData)
        return "";
    return pModuleData->getFuncSrc(funcId, funcAddr, fullPath);
}

char* INFO_NODE::getCallSrc(bool fullPath)
{
    if (isFlow())
    {
        FLOW_NODE* This = (FLOW_NODE*)this;
        char* src = "";
        WORD moduleId;
        DWORD funcId;
        if (getApp()->findAddr(This->callAddr, funcId, moduleId))
        {
            ModuleData* pModuleData = getApp()->pDbgInfo->arModuleData[moduleId];
            src = pModuleData->getFuncSrc(funcId, This->callAddr, fullPath);
        }
        return src;
    }
    else if (isTrace())
    {
        return ((TRACE_NODE*)this)->tarce_srcName();
    }
    else
        return "";

}

bool APP_NODE::findAddr(DWORD64 addr, DWORD& funcId, WORD& moduleId)
{
    WORD cModules = (WORD)pDbgInfo->arModuleData.size();
	bool ret = false;
	for (WORD i = 0; i < cModules; i++)
	{        
		if (addr < pDbgInfo->arModuleData[i]->startAddr || addr > pDbgInfo->arModuleData[i]->endAddr)
			continue;
		addr -= pDbgInfo->arModuleData[i]->startAddr;
		DWORD l = 0, u = (DWORD)pDbgInfo->arModuleData[i]->info.Count(), idx;
		while (l < u)
		{
			idx = (l + u) / 2;
            DbgFuncInfo* funcInfo = pDbgInfo->arModuleData[i]->GetDbgFuncInfo(idx);
			if (addr < funcInfo->addrStart) //comparison < 0
				u = idx;
			else if (addr > funcInfo->addrEnd) //comparison > 0
				l = idx + 1;
			else {
				funcId = idx; //found 
				moduleId = i;
				ret = true;
				break;
			}
		}
		break;
	}
	return ret;
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