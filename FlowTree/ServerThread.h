#pragma once
#include "WorkerThread.h"
#include "Dbg.h"

class TraceThread : public WorkerThread
{
public:
	TraceThread();
	virtual void Terminate();
protected:
	void Work(LPVOID pWorkParam);
	SOCKET traceSocket;
};

struct TraceFileMap {
    HANDLE hTempFile = INVALID_HANDLE_VALUE;
    HANDLE hMapFile = NULL;
    char* pBuf = NULL;

    ~TraceFileMap() {
        close();
    }

    bool initWriter(LPCSTR szMapingName, bool useTempFile, DWORD size) {
        // Initialize security attributes
        SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), NULL, FALSE };
        SECURITY_DESCRIPTOR sd;
        if (!InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION)) {
            printf("InitializeSecurityDescriptor failed (%d)\n", GetLastError());
            return false;
        }
        if (!SetSecurityDescriptorDacl(&sd, TRUE, NULL, FALSE)) {
            printf("SetSecurityDescriptorDacl failed (%d)\n", GetLastError());
            return false;
        }
        sa.lpSecurityDescriptor = &sd;

        if (useTempFile) {
            //CHAR szTempFileNameOld[MAX_PATH] = {0};
            //CHAR szTempFileName[MAX_PATH];
            //CHAR szTempPath[MAX_PATH];
            //if (GetTempPathA(MAX_PATH, szTempPath) == 0) {
            //    printf("GetTempPath failed (%d)\n", GetLastError());
            //    return false;
            //}
            //if (GetTempFileNameA(szTempPath, "FTR", 0, szTempFileName) == 0) {
            //    printf("GetTempFileName failed (%d)\n", GetLastError());
            //    return false;
            //}
            //sprintf_s(szTempFileNameOld, _countof(szTempFileNameOld) - 1, "%s\\FTR*.*", szTempPath);

            DeleteFileA("FlowTreeDataSharedMemory.tmp");
            hTempFile = CreateFileA(
                "FlowTreeDataSharedMemory.tmp",                     // File name
                GENERIC_READ | GENERIC_WRITE,       // Read/write access
                FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, // Full sharing
                &sa,                                // Permissive security
                CREATE_ALWAYS,                      // Create or overwrite
                FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, // Temp and auto-delete
                NULL                                // No template
            );
            if (hTempFile == INVALID_HANDLE_VALUE) {
                printf("CreateFile failed (%d)\n", GetLastError());
                return false;
            }

            //// Verify file accessibility
            //DWORD bytesWritten;
            //if (!WriteFile(hTempFile, "TEST", 4, &bytesWritten, NULL)) {
            //    printf("WriteFile to verify file failed (%d)\n", GetLastError());
            //    CloseHandle(hTempFile);
            //    hTempFile = INVALID_HANDLE_VALUE;
            //    return false;
            //}
        }

        // Check for existing mapping to avoid conflicts
        HANDLE hExistingMap = OpenFileMappingA(FILE_MAP_READ | FILE_MAP_WRITE, FALSE, szMapingName);
        if (hExistingMap != NULL) {
            printf("Warning: File mapping already exists, closing it.\n");
            CloseHandle(hExistingMap);
        }

        // Create file mapping
        hMapFile = CreateFileMappingA(
            useTempFile ? hTempFile : INVALID_HANDLE_VALUE, // File handle or paging file
            &sa,                                            // Permissive security
            PAGE_READWRITE,                                 // Read/write protection
            0,                                              // High-order size
            size,                                           // Low-order size
            szMapingName                                    // Mapping name
        );
        if (hMapFile == NULL) {
            printf("CreateFileMapping failed (%d)\n", GetLastError());
            close();
            return false;
        }

        // Map view of file
        pBuf = (char*)MapViewOfFile(
            hMapFile,                           // Mapping handle
            FILE_MAP_READ | FILE_MAP_WRITE,     // Match writer's access
            0,                                  // High-order offset
            0,                                  // Low-order offset
            size                                // Map entire size
        );
        if (pBuf == NULL) {
            printf("MapViewOfFile failed (%d)\n", GetLastError());
            close();
            return false;
        }

        return true;
    }

    bool initRader(LPCSTR szMapingName, DWORD size) {
        // Open the file mapping
        hMapFile = OpenFileMappingA(
            FILE_MAP_READ | FILE_MAP_WRITE,     // Match writer's access
            FALSE,                              // Do not inherit
            szMapingName                        // Mapping name
        );
        if (hMapFile == NULL) {
            printf("OpenFileMapping failed (%d)\n", GetLastError());
            close();
            return false;
        }

        // Map view of file
        pBuf = (char*)MapViewOfFile(
            hMapFile,                           // Mapping handle
            FILE_MAP_READ | FILE_MAP_WRITE,     // Match writer's access
            0,                                  // High-order offset
            0,                                  // Low-order offset
            size                                // Map entire size
        );
        if (pBuf == NULL) {
            printf("MapViewOfFile failed (%d)\n", GetLastError());
            close();
            return false;
        }

        return true;
    }

    void close() {
        if (pBuf != NULL) {
            UnmapViewOfFile(pBuf);
            pBuf = NULL;
        }
        if (hMapFile != NULL) {
            CloseHandle(hMapFile);
            hMapFile = NULL;
        }
        if (hTempFile != INVALID_HANDLE_VALUE) {
            CloseHandle(hTempFile);
            hTempFile = INVALID_HANDLE_VALUE;
        }
    }
};

struct MapSync {
    DWORD* m_sendPos;
    DWORD* m_recvPos;
    DWORD* m_blocked;
    HANDLE hReadyEvent = NULL;
    HANDLE hMutex = NULL;
    DWORD& sendPos() { return *m_sendPos; }
    DWORD& recvPos() { return *m_recvPos; }
    DWORD& blocked() { return *m_blocked; }
    ~MapSync()
    {
        close();
    }
    bool initWriter(char* mapBuf)
    {
        // Initialize security attributes
        SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), NULL, FALSE };
        SECURITY_DESCRIPTOR sd;
        if (!InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION)) {
            printf("InitializeSecurityDescriptor failed (%d)\n", GetLastError());
            return false;
        }
        if (!SetSecurityDescriptorDacl(&sd, TRUE, NULL, FALSE)) {
            printf("SetSecurityDescriptorDacl failed (%d)\n", GetLastError());
            return false;
        }
        sa.lpSecurityDescriptor = &sd;
        hMutex = CreateMutex(&sa, FALSE, TEXT("FlowTreeMutex"));
        if (hMutex == NULL) {
            printf("CreateMutex failed (%d)\n", GetLastError());
            return false;
        }
        if (hMutex == NULL) {
            printf("CreateMutex failed (%d)\n", GetLastError());
            return false;
        }
        hReadyEvent = CreateEvent(NULL,
            FALSE, //bManualReset
            FALSE, //bInitialState is signaled
            TEXT("FlowTreeSharedMemoryEvent")
        );
        if (hReadyEvent == NULL) {
            close();
            return false;
        }
        m_sendPos = (DWORD*)mapBuf;
        m_recvPos = m_sendPos + 1;
        m_blocked = m_recvPos + 1;
        *m_sendPos = 0;
        *m_recvPos = 0;
        *m_blocked = 0;
        return true;
    }
    bool initReader(char* mapBuf)
    {
        hMutex = OpenMutex(MUTEX_ALL_ACCESS, FALSE, TEXT("FlowTreeMutex"));
        if (hMutex == NULL) {
            printf("OpenMutex failed (%d)\n", GetLastError());
            close();
            return false;
        }
        hReadyEvent = OpenEvent(EVENT_ALL_ACCESS, FALSE, TEXT("FlowTreeSharedMemoryEvent"));
        if (hReadyEvent == NULL) {
            close();
            return false;
        }
        m_sendPos = (DWORD*)mapBuf;
        m_recvPos = m_sendPos + 1;
        m_blocked = m_recvPos + 1;
        return true;
    }
    bool enterCriticalSection(DWORD timeoutMs = INFINITE) {
        if (hMutex == NULL) {
            printf("Mutex not initialized\n");
            return false;
        }
        DWORD result = WaitForSingleObject(hMutex, timeoutMs);
        if (result == WAIT_OBJECT_0) {
            return true;
        }
        else if (result == WAIT_TIMEOUT) {
            printf("Mutex wait timed out\n");
            return false;
        }
        else {
            printf("WaitForSingleObject failed (%d)\n", GetLastError());
            return false;
        }
    }
    void leaveCriticalSection() {
        if (hMutex != NULL) {
            ReleaseMutex(hMutex);
        }
    }
    void close()
    {
        if (hReadyEvent) {
            CloseHandle(hReadyEvent);
            hReadyEvent = NULL;
        }
        if (hMutex != NULL) {
            CloseHandle(hMutex);
            hMutex = NULL;
        }
    }
};

class ServerThread : public WorkerThread
{
public:
	ServerThread();
	virtual void Terminate();
#ifdef USE_PIPE
	bool Paused() { return paused; };
	void Pause(bool b) { paused = b; }
#endif //USE_PIPE
protected:
	void Work(LPVOID pWorkParam);
	bool Send(void* sendbuf, DWORD cb);
	bool Recv(void* recvbuf, DWORD cb);
#ifdef USE_PIPE
	void RecvTraces();
	bool paused = true;
	bool InitWorker();
	HANDLE hPipe = INVALID_HANDLE_VALUE;
#else
	void RecvTraceViaTCP();
	TraceThread traceThread;
	SOCKET clientSocket;
	SOCKET listenSocket;
#endif //USE_PIPE
	void IpcErr(CHAR* lpFormat, ...);
};
