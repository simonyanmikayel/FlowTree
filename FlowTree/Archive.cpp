#include "stdafx.h"
#include "Archive.h"
#include "Helpers.h"
#include "Settings.h"

Archive    gArchive;
DWORD Archive::archiveNumber = 0;

Archive::Archive()
{
    ZeroMemory(this, sizeof(*this));
    m_pTraceBuf = new MemBuf(MAX_BUF_SIZE, 256 * 1024 * 1024);
    m_listedNodes = new ListedNodes();
	InitializeCriticalSectionAndSpinCount(&cs, 0x00000400);
	clearArchive();
}

Archive::~Archive()
{
    //we intentionaly do not free memory for this single instance application quick exit
    //delete m_pMemBuf;
	//DeleteCriticalSection(&cs);
}

BOOL IsProcessRunning(DWORD pid)
{
	HANDLE process = OpenProcess(SYNCHRONIZE, FALSE, pid);
	DWORD ret = WaitForSingleObject(process, 0);
	CloseHandle(process);
	return (ret == WAIT_TIMEOUT);
}

void Archive::onPaused()
{
	APP_NODE* pApp = (APP_NODE*)gArchive.getRootNode()->lastChild;
	while (pApp)
	{
		pApp->lastNN = INFINITE;
		pApp = (APP_NODE*)pApp->prevSibling;
	}
}

void Archive::clearArchive()
{
	EnterCriticalSection(&cs);
	archiveNumber++;
	curApp = 0;
    curThread = 0;
	nn = 0;
	tickCount = GetTickCount();
    getSNAPSHOT().clear();

	std::vector<DbgInfo*> arDbgInfo;
	if (m_rootNode)
	{
		APP_NODE* pApp = (APP_NODE*)m_rootNode->lastChild;
		while (pApp)
		{
			if (IsProcessRunning(pApp->pDbgInfo->Pid))
				arDbgInfo.push_back(pApp->pDbgInfo);
			else
				delete pApp->pDbgInfo;

			pApp = (APP_NODE*)pApp->prevSibling;
		}
	}

    delete m_pNodes;
    m_pNodes = nullptr;
    m_rootNode = nullptr;
    m_pTraceBuf->Free();
    m_listedNodes->Free();

	m_pNodes = new PtrArray<LOG_NODE>(m_pTraceBuf);
	m_rootNode = (ROOT_NODE*)m_pNodes->Add(sizeof(ROOT_NODE), true);
	m_rootNode->data_type = ROOT_DATA_TYPE;
	ATLASSERT(m_pNodes && m_rootNode);

	for (size_t i = 0; i < arDbgInfo.size(); i++)
	{
		addApp(arDbgInfo[i]);
	}
	LeaveCriticalSection(&cs);
}

size_t Archive::UsedMemory() 
{
    return m_pTraceBuf->UsedMemory() + m_listedNodes->UsedMemory(); 
}


APP_NODE* Archive::addApp(DbgInfo* pDbgInfo)
{
	if (m_rootNode == nullptr)
		return nullptr;

	APP_NODE* pNode = (APP_NODE*)m_pNodes->Add(sizeof(APP_NODE), true);
	if (!pNode)
		return nullptr;

	pNode->data_type = APP_DATA_TYPE;
	pNode->pDbgInfo = pDbgInfo;
	pNode->appPath = pDbgInfo->appPath;

	pNode->appName = pNode->appPath;
	char* name_part = strrchr(pNode->appName, '\\');
	if (name_part)
	{
		pNode->appName = name_part + 1;
	}

	m_rootNode->add_child(pNode);
	pNode->hasCheckBox = 1;
	pNode->checked = 1;
	return pNode;
}

APP_NODE* Archive::getApp(DWORD pid)
{
	if (curApp)
	{
		if (curApp->pDbgInfo->Pid == pid)
			return curApp;
	}

	curApp = (APP_NODE*)gArchive.getRootNode()->lastChild;
	while (curApp)
	{
		if (curApp->pDbgInfo->Pid == pid)
			break;
		curApp = (APP_NODE*)curApp->prevSibling;
	}

	return curApp;
}

THREAD_NODE* Archive::addThread(APP_NODE* pAppNode, int tid)
{
    THREAD_NODE* pNode = (THREAD_NODE*)m_pNodes->Add(sizeof(THREAD_NODE), true);
    if (!pNode)
        return nullptr;

    pNode->data_type = THREAD_DATA_TYPE;
	pNode->pAppNode = pAppNode;
    pNode->tid = tid;
	pNode->threadNN = ++(pAppNode->threadCount);

	pAppNode->add_child(pNode);
    pNode->hasCheckBox = 1;
    pNode->checked = 1;
    return pNode;
}

THREAD_NODE* Archive::getThread(APP_NODE* pAppNode, CmdLog* p)
{
    if (curThread && curThread->tid == p->tid)
        return curThread;

    curThread = NULL;
    curThread = (THREAD_NODE*)pAppNode->lastChild;
    while (curThread)
    {
        if (curThread->tid == p->tid)
            break;
        curThread = (THREAD_NODE*)curThread->prevSibling;
    }

    if (!curThread)
    {
        curThread = addThread(pAppNode, p->tid);
    }

    return curThread;
}


LOG_NODE* Archive::addFlow(THREAD_NODE* pThreadNode, CmdFlow* pCmdFlow)
{
    FLOW_NODE* pNode = (FLOW_NODE*)m_pNodes->Add(sizeof(FLOW_NODE), true);
    if (!pNode)
        return nullptr;

    pNode->data_type = FLOW_DATA_TYPE;
    pNode->log_type = pCmdFlow->enter ? LOG_TYPE_ENTER : LOG_TYPE_EXIT;
	pNode->tickCount = pCmdFlow->tickCount;
	pNode->funcId = pCmdFlow->funcId;
	pNode->funcAddr = pCmdFlow->funcAddr;
	pNode->callAddr = pCmdFlow->callAddr;
	pNode->nn = ++nn;

    pNode->threadNode = pThreadNode;

    ((FLOW_NODE*)pNode)->addToTree();

    return pNode;
}

static int getCollor(char* pBuf, int &iSkip, int cb)
{
    int color = 0;
    if (iSkip < cb && isdigit(pBuf[iSkip]))
    {
        color = pBuf[iSkip] - '0';
        iSkip++;
        if (iSkip < cb && isdigit(pBuf[iSkip]))
        {
            color = (10 * color) + (pBuf[iSkip] - '0');
            iSkip++;
        }
    }
    return color;
}

static bool setCollor(THREAD_NODE* pThreadNode, unsigned char* pTrace, int i, int &cb_trace, int& color)
{
    char* pBuf = pThreadNode->COLOR_BUF;
    bool bRet = false;
    if (pTrace[i] == '\033')
    {
        pThreadNode->cb_color_buf = 0;
    }
    else if (pThreadNode->cb_color_buf == 0)
    {
        // no color in buffer and in trace
        return false;
    }
    bool reset_buffer = false;
    int cb_color_old = pThreadNode->cb_color_buf;
    int cb_color = min(cb_trace - i, (int)sizeof(pThreadNode->COLOR_BUF) - pThreadNode->cb_color_buf - 1);
    if (cb_color <= 0)
    {
        pThreadNode->cb_color_buf = 0;
        return false;
    }
    memcpy(pBuf + pThreadNode->cb_color_buf, pTrace + i, cb_color);
    pThreadNode->cb_color_buf += cb_color;
    pBuf[pThreadNode->cb_color_buf] = 0;

    int iSkip = 0, color1 = 0, color2 = 0;
    if (pBuf[0] == '\033' && pBuf[1] == '[')
    {
        iSkip = 2;
        color1 = getCollor(pBuf, iSkip, pThreadNode->cb_color_buf);
        if (pBuf[iSkip] == ';')
        {
            iSkip++;
            color2 = getCollor(pBuf, iSkip, pThreadNode->cb_color_buf);
        }
        if (pBuf[iSkip] == ';') //getting third color as color2
        {
            iSkip++;
            color2 = getCollor(pBuf, iSkip, pThreadNode->cb_color_buf);
        }

        if (iSkip < pThreadNode->cb_color_buf && pBuf[iSkip] == 'm')
        {
            iSkip++;
            bRet = true;
        }
    }

    if (iSkip - cb_color_old != cb_trace - i)
    {
        reset_buffer = true; //we have data after collor, so will wait for new one
    }

    if (iSkip)
    {
        iSkip -= cb_color_old;
        if (iSkip > 0)
        {
            if (cb_trace - i - iSkip >= 0)
            {
                memmove(pTrace + i, pTrace + i + iSkip, cb_trace - i - iSkip);
                cb_trace -= iSkip;
            }
            else
            {
                ATLASSERT(FALSE);
                reset_buffer = true;
                bRet = false;
            }
        }
    }

    if (bRet)
    {
        if (!color)
            color = color1 ? color1 : color2;
        reset_buffer = true;
    }

    if (reset_buffer)
    {
        pThreadNode->cb_color_buf = 0;
    }

    return bRet;
}

LOG_NODE* Archive::addTrace(THREAD_NODE* pThreadNode, CmdTrace *pCmdTrace, int& prcessed)
{
    if (prcessed >= pCmdTrace->cb_trace)
        return NULL;

    bool endsWithNewLine = false;
    unsigned char* pTrace = (unsigned char*)pCmdTrace->trace();
#ifdef _DEBUG
    //if (221 == pLogRec->call_line)
    if (0 != strstr((char*)pTrace, "tsc.c;3145"))
        int k = 0;
#endif
    int i = prcessed;
    int cWhite = 0;
    int color = 0;
    setCollor(pThreadNode, pTrace, i, pCmdTrace->cb_trace, color);
    for (; i < pCmdTrace->cb_trace; i++)
    {
        if (pTrace[i] == '\033')
        {
            if (setCollor(pThreadNode, pTrace, i, pCmdTrace->cb_trace, color))
            {
                i--;
                continue;
            }
        }

        if (pTrace[i] == '\n')
        {
            i++;
            break;
        }
        //remove contraol c
        if (pTrace[i] < ' ')
        {
            cWhite++;
            pTrace[i] = ' ';
        }
    }
    endsWithNewLine = (i > 0 && pTrace[i - 1] == '\n');
    i -= prcessed;
    if (i < 0)
    {
        ATLASSERT(FALSE);
        i = 0;
    }
    int cb = (endsWithNewLine && i > 0) ? (i - 1) : i;

    //do not add blank lines
    if (cb == cWhite)
    {
        if (pThreadNode->latestTrace && endsWithNewLine)
            pThreadNode->latestTrace->hasNewLine = 1;
        if (!pThreadNode->emptLineColor && color)
            pThreadNode->emptLineColor = color;
        prcessed += i;
        return NULL;
    }

    if (pThreadNode->emptLineColor && !color)
    {
        color = pThreadNode->emptLineColor;
    }
    pThreadNode->emptLineColor = 0;

    bool newChank = false;
    // check if we need append to previous trace
    if (pThreadNode->latestTrace) // && (pThreadNode->latestTrace->parent == pThreadNode->curentFlow)
    {
        if (pThreadNode->latestTrace->hasNewLine == 0)
        {
            if (pThreadNode->latestTrace->cb_trace + cb < MAX_TRCAE_LEN)
                newChank = true;
            else
                pThreadNode->latestTrace->hasNewLine = 1;
        }
        if (endsWithNewLine && newChank)
            pThreadNode->latestTrace->hasNewLine = 1;
        if (cb == 0 && pCmdTrace->call_line == pThreadNode->latestTrace->GetCallLine())
            newChank = true;
    }

    if (newChank)
    {
        if (cb)
        {
            TRACE_CHANK* pLastChank = pThreadNode->latestTrace->getLastChank();
            pLastChank->next_chank = (TRACE_CHANK*)Alloc(sizeof(TRACE_CHANK) + cb);
            if (!pLastChank->next_chank)
                return nullptr;
            TRACE_CHANK* pChank = pLastChank->next_chank;
            pChank->len = cb;
            pChank->next_chank = 0;
            memcpy(pChank->trace, pCmdTrace->trace() + prcessed, cb);
            pChank->trace[cb] = 0;
            pThreadNode->latestTrace->cb_trace += cb;
        }
    }
    else
    {
        //add new trace
        int cb_fn_name = pCmdTrace->cb_fn_name;
        char* fnName = pCmdTrace->fnName();
        if (fnName[0] == '^')
        {
            fnName++;
            cb_fn_name--;
        }
        TRACE_NODE* pNode = (TRACE_NODE*)m_pNodes->Add(sizeof(TRACE_NODE) + cb_fn_name + sizeof(TRACE_CHANK) + cb, true);
        if (!pNode)
            return nullptr;

        pNode->data_type = TRACE_DATA_TYPE;

        pNode->log_type = LOG_TYPE_TRACE;
		pNode->tickCount = pCmdTrace->tickCount;
		pNode->SetCallLine(pCmdTrace->call_line);
		pNode->nn = ++nn;

        pNode->cb_fn_name = cb_fn_name;
        memcpy(pNode->fnName(), fnName, cb_fn_name);

        TRACE_CHANK* pChank = pNode->getFirestChank();
        pChank->len = cb;
        pChank->next_chank = 0;
        memcpy(pChank->trace, pCmdTrace->trace() + prcessed, cb);
        pChank->trace[cb] = 0;

        pNode->threadNode = pThreadNode;
        if (endsWithNewLine)
            pNode->hasNewLine = 1;

        if (pThreadNode->curentFlow && pThreadNode->curentFlow->isOpenEnter())
        {
            pNode->parent = pThreadNode->curentFlow;
        }
        else
        {
            pNode->parent = pThreadNode;
        }

        pThreadNode->latestTrace = pNode;
    }

    //update other fieds
    if (!pThreadNode->latestTrace->color)
        pThreadNode->latestTrace->color = color;

    prcessed += i;
    pThreadNode->latestTrace->lengthCalculated = 0;
    return pThreadNode->latestTrace;
}

void Archive::RegisterApp(DbgInfo* pDbgInfo)
{
	EnterCriticalSection(&cs);
	if (!getApp(pDbgInfo->Pid))
		addApp(pDbgInfo);
	LeaveCriticalSection(&cs);
}

bool Archive::append(DWORD pid, CmdLog* pLog)
{
	bool ret = false;

	EnterCriticalSection(&cs);
	do
	{
		APP_NODE* pAppNode = getApp(pid);
		if (!pAppNode)
			break;
		if (pLog->cmd != CMD_FLOW && pLog->cmd != CMD_TRACE)
			break;
		THREAD_NODE* pThreadNode = getThread(pAppNode, pLog);
		if (!pThreadNode)
			break;
		if (pLog->cmd == CMD_FLOW)
		{
			if (!addFlow(pThreadNode, (CmdFlow*)pLog))
				break;
		}
		else if (pLog->cmd == CMD_TRACE)
		{
			int prcessed = 0;
			CmdTrace *pCmdTrace = (CmdTrace*)pLog;
			while (prcessed < pCmdTrace->cb_trace)
			{
				int prcessed0 = prcessed;
				int cb_trace0 = pCmdTrace->cb_trace;
				addTrace(pThreadNode, pCmdTrace, prcessed);
				if (prcessed0 >= prcessed && cb_trace0 <= pCmdTrace->cb_trace && prcessed < pCmdTrace->cb_trace)
				{
					ATLASSERT(FALSE);
					break;
				}
			}
		}
		ret = !m_pNodes->IsFull();
	} while (false);
	LeaveCriticalSection(&cs);

	return ret;
}


DWORD Archive::getCount()
{
    return m_pNodes ? m_pNodes->Count() : 0;
}



void ListedNodes::updateList(BOOL flowTraceHiden)
{
    DWORD count = gArchive.getCount();
    for (DWORD i = archiveCount; i < count; i++)
    {
        addNode(gArchive.getNode(i), flowTraceHiden);
    }
    archiveCount = count;
}

void ListedNodes::applyFilter(BOOL flowTraceHiden)
{
    Free();
    archiveCount = gArchive.getCount();
    //stdlog("%u\n", GetTickCount());
    for (DWORD i = 0; i < archiveCount; i++)
    {
        addNode(gArchive.getNode(i), flowTraceHiden);
    }
    //stdlog("%u\n", GetTickCount());
}

void SNAPSHOT::update()
{
    first = 0, last = INFINITE;
    if (curSnapShot)
    {
        if (curSnapShot == 1)
        {
            size_t c = snapShots.size();
            first = snapShots[c - 1].pos;
        }
        else
        {
            first = (curSnapShot == 2) ? 0 : snapShots[curSnapShot - 3].pos;
            last = snapShots[curSnapShot - 2].pos;
        }
    }
}
