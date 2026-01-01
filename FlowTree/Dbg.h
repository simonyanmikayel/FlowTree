#pragma once

#include "FileMap.h"

#define MAX_TRCAE_LEN (1024*2)
#define MAX_FUNC_NAME_LEN (1024*3)
#define MAX_SRC_NAME_LEN (4 * MAX_PATH)

#define CMD_BUF(cmd) (char*)(&cmd)+1
#define CMD_SIZE(cmd) sizeof(cmd)-1
#define CMD_BUF_AND_SIZE(cmd) (char*)(&cmd)+1, sizeof(cmd)-1

enum PROFILER_CMD { CMD_START, CMD_SETTINGS, CMD_MODULE, CMD_PREP_INFO, CMD_INFO_SIZE, CMD_FLOW, CMD_TRACE, CMD_STARTING, CMD_LAST };
#define LOG_ATTR_SKIP_FUNC 1
#define LOG_ATTR_SHOW_FUNC 2
#define LOG_ATTR_HIDE_FUNC 3
#define LOG_ATTR_EXPAND_FUNC 4

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
struct CmdStarting : Cmd
{
	CmdStarting() : Cmd(CMD_STARTING) {}
	DWORD tickCount;
	DWORD TRACE_MAP_FILE_SIZE;
};
struct CmdSettings : Cmd
{
	CmdSettings() : Cmd(CMD_SETTINGS) {}
	DWORD DissableBufferization;
	DWORD UseTcpForLog;
	DWORD LogOnlyTraces;
};
struct CmdModule : Cmd
{
    CmdModule() : Cmd(CMD_MODULE) {}
    DWORD64 startAddr;
	DWORD64 endAddr;
	DWORD FuncCount;
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
	DWORD cb;
	DWORD NN;
	DWORD Pid;
	DWORD tid;
    DWORD tickCount;
};
struct CmdFlow : CmdLog
{
    CmdFlow() : CmdLog(CMD_FLOW) {}
    BYTE enter;
	WORD moduleId;
    DWORD funcId;
	DWORD64 funcAddr;
	DWORD64 callAddr;
};
struct CmdTrace : CmdLog
{
    CmdTrace() : CmdLog(CMD_TRACE) {}
	int MsgType; //enum QtMsgType { QtDebugMsg, QtWarningMsg, QtCriticalMsg, QtFatalMsg, QtInfoMsg};
    int call_line;
	int cb_src_name;
    int cb_fn_name;
    int cb_trace;
	char* srcName() { return ((char*)(this)) + sizeof(CmdTrace); }
	char* fnName() { return srcName() + cb_src_name; }
	char* trace() { return fnName() + cb_fn_name; }
	DWORD CmdTraceSize() { return sizeof(CmdTrace) + CmdTraceDadaSize(); }
	DWORD CmdTraceDadaSize() { return cb_src_name + cb_fn_name + cb_trace; }
};
#define UDP_BUF_SIZE (1400) // this size of UDP guaranteed to be send as single package

struct DbgLineInfo
{
    DWORD   line;
    DWORD64 addr;
    DWORD   dwLength;
	DWORD   dwSrcId;
	DWORD   srcNameOffset;
};

struct DbgFuncInfo
{
	DWORD fnNameOffset;
    DWORD shortFnNameSize;
	BYTE  attr;
    DWORD  cLine = 0;
	DWORD64 addrStart = 0;
	DWORD64 addrEnd = 0;
	DWORD lineInfoOffset;
};

struct FUNC_ADDR
{
    DWORD64 addrStart;
    DWORD64 addrEnd;
	BYTE    attr;
};

constexpr const char* DbgInfExtention = ".inf";
constexpr const char* DbgDatExtention = ".dat";
constexpr DWORD DbgFileMapHeaderVersion = 1;
constexpr DWORD DbgFileMapHeaderMagic = 0xF0F0F0F0;
struct DbgFileMapHeader
{
	DWORD version = DbgFileMapHeaderVersion;
	DWORD magic = 0; //will set correct value after successful load
	uint32_t crcDbgSettings = 0;
	FILETIME ftLastWriteTime = {0,0};
};
#pragma pack(pop)

struct DbgLoadStatus
{
	bool stop;
	DWORD cModulesTotal;
	DWORD cModulesLoading;
	DWORD cFunctionsLoaded;
	DWORD cSettingsChecked;
	char szCurrentModule[MAX_PATH];
};
extern DbgLoadStatus gDbgLoadStatus;

struct ModuleData
{
	ModuleData(const CmdModule*);
	~ModuleData();
	DWORD64 startAddr;
	DWORD64 endAddr;
	char szModName[MAX_PATH];
	ARRAY_IN_FILE<DbgFuncInfo> info;
	BUFFER_IN_FILE data;
	DbgFuncInfo* GetDbgFuncInfo(DWORD funcId) { 
		return funcId < info.Count() ? &info[funcId] : nullptr; 
	}
	DbgLineInfo* GetFirstLineInfo(DWORD funcId);
	DbgLineInfo* GetLineInfo(DWORD funcId, DWORD64 funcAddr);
	char* fnName(DWORD funcId, bool forceFullName);
	// DWORD cbFnName(DWORD funcId, bool forceFullName);
	char* getFuncSrc(DWORD funcId, DWORD64 funcAddr, bool fullPath);
	DWORD64 GetAddrStart(DWORD funcId);
	DWORD64 GetAddrEnd(DWORD funcId);
	bool GetFuncAddresses(DWORD funcId, FUNC_ADDR& funcAdd);
	bool ready = false;
};

class DbgInfo
{
public:
	DbgInfo(const char* _appPath);
    ~DbgInfo();
	std::vector<ModuleData*> arModuleData;
	bool AddModule(const CmdModule* module);
	bool OpenFileMaps(ModuleData* moduleData, const char* szModName, bool openExisting);
	bool DbgFileMapHeaderValid(const WIN32_FIND_DATAA& moduleFileData, const DbgFileMapHeader& hdr);
	void CheckDbgSettings(ModuleData* moduleData);
	DWORD Pid = 0;
	uint32_t crcDbgSettings;
	char appPath[MAX_PATH + 1];
};


