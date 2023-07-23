#pragma once

#include "Buffer.h"

#define MAX_TRCAE_LEN (1024*2)
#define MAX_FUNC_NAME_LEN (1024*3)

#define CMD_BUF(cmd) (char*)(&cmd)+1
#define CMD_SIZE(cmd) sizeof(cmd)-1
#define CMD_BUF_AND_SIZE(cmd) (char*)(&cmd)+1, sizeof(cmd)-1

enum PROFILER_CMD { CMD_START, CMD_NEXT_MODULE, CMD_PREP_INFO, CMD_INFO_SIZE, CMD_FLOW, CMD_TRACE, CMD_LAST };
#define LOG_ATTR_SKIP_FUNC 1
#define LOG_ATTR_SHOW_FUNC 2

#pragma pack(push,1)
struct Cmd
{
    Cmd(PROFILER_CMD c) { cmd = c; }
    BYTE cmd;
};
struct CmdStart : Cmd
{
	CmdStart() : Cmd(CMD_START) {}
	DWORD Pid;
	char szAppName[MAX_PATH];
};
struct CmdNextModule : Cmd
{
    CmdNextModule() : Cmd(CMD_NEXT_MODULE) {}
    DWORD64 BaseOfDll;
    char szModName[MAX_PATH];
};
struct CmdInfoSize : Cmd
{
    CmdInfoSize() : Cmd(CMD_INFO_SIZE) {}
    DWORD infoSize;
};
struct CmdLog : Cmd
{
    CmdLog(PROFILER_CMD c) : Cmd(c) {}
    DWORD tid;
    DWORD tickCount;
};
struct CmdFlow : CmdLog
{
    CmdFlow() : CmdLog(CMD_FLOW) {}
    BYTE enter;
    DWORD funcId;
	DWORD64 funcAddr;
	DWORD64 callAddr;
};
struct CmdTrace : CmdLog
{
    CmdTrace() : CmdLog(CMD_TRACE) {}
	int MsgType; //enum QtMsgType { QtDebugMsg, QtWarningMsg, QtCriticalMsg, QtFatalMsg, QtInfoMsg};
    int call_line;
    int cb_fn_name;
    int cb_trace;
	char* fnName() { return ((char*)(this)) + sizeof(CmdTrace); }
	char* trace() { return fnName() + cb_fn_name; }
	DWORD CmdTraceSize() { return sizeof(CmdTrace) + cb_fn_name + cb_trace; }
};
#define UDP_BUF_SIZE (1400) // this size of UDP guaranteed to be send as single package
struct TRACE_BUF
{
	DWORD cb;
	DWORD Pid;
	DWORD NN;
	char buf[UDP_BUF_SIZE];
};

struct DbgLineInfo
{
    DWORD   line;
    DWORD64 addr;
    DWORD   dwLength;
	DWORD   dwSrcId;
	char*   srcName;
	char*   srcShortName;
};
struct DbgFuncInfo
{
    char* fnName;
    DWORD cb_fnName;
    char* shortFnName;
    DWORD cb_shortFnName;
	BYTE  attr;
    DWORD  cLine = 0;
	bool pathIncluded = true;
    DbgLineInfo* pLineInfo;
	DWORD64 GetAddrStart() {
		return (pLineInfo && cLine) ? pLineInfo[0].addr : 0;
	}
	DWORD64 GetAddrEnd() {
		return (pLineInfo && cLine) ? (pLineInfo[cLine - 1].addr + pLineInfo[cLine - 1].dwLength): 0;
	}
	bool les(DbgFuncInfo *p) {
		return (GetAddrStart() < p->GetAddrStart());
	}
	DbgLineInfo* GetLineInfo(DWORD64 address) {
		DbgLineInfo* p = pLineInfo;
		for (WORD i = 0; !(p->addr <= address && p->addr + p->dwLength >= address) && i < cLine - 1; i++)
			p++;
		return p;
	}
};
struct FUNC_ADDR
{
    DWORD64 addrStart;
    DWORD64 addrEnd;
	BYTE    attr;
};
#pragma pack(pop)

class DbgInfo
{
public:
    DbgInfo();
    ~DbgInfo();
    bool AddInfo(DWORD64 BaseOfDll, char* szModName);
    PtrArray<DbgFuncInfo>* FuncInfo() { return m_pDbgFuncInfo; }
	DWORD Pid;
	char appPath[MAX_PATH + 1];
private:
    MemBuf* m_pMemBuf;
    PtrArray<DbgFuncInfo>* m_pDbgFuncInfo;
};


