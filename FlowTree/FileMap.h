// FileMap.h

#pragma once

struct FILE_MAP
{
public:
	HANDLE m_hFile;
	HANDLE m_hMap;
	char* m_ptr;

	FILE_MAP(HANDLE hFile = INVALID_HANDLE_VALUE)
	{
		ZeroMemory(this, sizeof(*this));
		m_hFile = hFile;
	}
	FILE_MAP(LPCTSTR lpFileName,  bool bFullAccess = true,  DWORD dwCreationDisposition = OPEN_EXISTING, DWORD dwMapSize = INFINITE)
	{
		ZeroMemory(this, sizeof(*this));
		Create(lpFileName, bFullAccess,  dwCreationDisposition);
		Map(bFullAccess,  dwMapSize);
	}

	virtual ~FILE_MAP()
	{
		UnMap(true);
	}

	DWORD Size()
	{
		DWORD dwLen = INVALID_FILE_SIZE;
		if (IsOpened())
			dwLen = GetFileSize(m_hFile, 0);
		if (dwLen == INVALID_FILE_SIZE)
			dwLen = 0;
		 
		return dwLen;
	}

	bool IsOpened()
	{
		return m_hFile != INVALID_HANDLE_VALUE;
	}

	bool Create(const char* lpFileName,  bool bFullAccess = true, DWORD dwCreationDisposition = OPEN_EXISTING)
	{
		m_hFile = CreateFileA(lpFileName, bFullAccess ? GENERIC_READ | GENERIC_WRITE : GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, dwCreationDisposition, FILE_ATTRIBUTE_NORMAL, 0);
		return INVALID_HANDLE_VALUE != m_hFile;
	}

	bool Map(bool bFullAccess = true, DWORD dwMapSize = INFINITE)
	{
		DWORD dwLen = Size();
		if (dwMapSize == INFINITE)
			dwMapSize = dwLen;
		if (dwMapSize <= dwLen)
		{
			if (NULL != (m_hMap = CreateFileMapping(m_hFile, 0, bFullAccess ? PAGE_READWRITE : PAGE_READONLY,  0, dwMapSize, NULL)))
				m_ptr = (char*)MapViewOfFile(m_hMap, bFullAccess ? FILE_MAP_ALL_ACCESS : FILE_MAP_READ, 0, 0, 0);
		}
		return m_ptr != NULL;
	}

	void UnMap(bool bCloseFile)
	{

		if (m_ptr){
			UnmapViewOfFile(m_ptr);
			m_ptr = NULL;
		}
		if (m_hMap){
			CloseHandle(m_hMap);
			m_hMap = NULL;
		}
		if (m_hFile != INVALID_HANDLE_VALUE && bCloseFile){
			CloseHandle(m_hFile);
			m_hFile = INVALID_HANDLE_VALUE;
		}
	}

	bool SetPos(DWORD dwPos)
	{
		DWORD dw = SetFilePointer(m_hFile, dwPos, 0, FILE_BEGIN);
		return dw == dwPos;
	}
	DWORD GetPos()
	{
		// Get the current file position
		LARGE_INTEGER liDistanceToMove = { 0 };
		LARGE_INTEGER liNewFilePointer;
		SetFilePointerEx(m_hFile, liDistanceToMove, &liNewFilePointer, FILE_CURRENT);
		return (DWORD)liNewFilePointer.QuadPart;
	}
	bool Write(const void* pBuf, DWORD cb)
	{
		//DWORD dwPos = GetPos();
		DWORD dwWriten;
		return WriteFile(m_hFile, pBuf, cb, &dwWriten, NULL) && dwWriten == cb;
	}

	bool Read(void* pBuf, DWORD cb, DWORD* pdwRead = NULL)
	{
		DWORD dwRead;
		if (!pdwRead)
			pdwRead = &dwRead;
		*pdwRead = 0;
		return ReadFile(m_hFile, pBuf, cb, pdwRead, NULL) && *pdwRead == cb;
	}

	FILE_MAP& operator << (char ch) 
	{ 
		Write(&ch, sizeof(ch)); 
		return *this;
	}
	FILE_MAP& operator << (int i)
	{
		Write(&i, sizeof(i));
		return *this;
	}
	FILE_MAP& operator << (const char* sz)
	{ 
		Write((void*)sz, (DWORD)strlen(sz)); 
		return *this;
	}
	FILE_MAP& operator << (int* i) 
	{
		char sz[32]; 
		_itoa_s(*i, sz, 32, 10);
		Write(sz, (DWORD)strlen(sz)); 
		return *this;
	}
};

template <typename Type> struct ARRAY_IN_FILE : FILE_MAP
{
	void AdjustCount()
	{
		m_Count = (Size() - m_headerSize) / sizeof(Type);
	}
	void SetHeaderSize(DWORD headerSize)
	{
		m_headerSize = headerSize;
		AdjustCount();
	}
	bool Add(const Type* type)
	{
		if (!Write(type, sizeof(Type)))
			return false;
		m_Count++;
		return true;;
	}
	Type& operator[](size_t index)
	{
		return ((Type*)(m_ptr + m_headerSize))[index];
	}
	size_t Count() 
	{
		return m_Count;
	}

private:
	size_t m_Count = 0;
	DWORD m_headerSize = 0;
};

struct BUFFER_IN_FILE : FILE_MAP
{
	DWORD Add(const void* buf, DWORD size)
	{
		DWORD ret = Size();
		if (!Write(&size, sizeof(size)))
			return INFINITE;
		if (!Write(buf, size))
			return INFINITE;
		return ret;
	}
	DWORD GetSize(size_t offset)
	{
		if (m_ptr)
			return *((DWORD*)(m_ptr + offset));
		else
			return INFINITE;
	}
	char* GetVal(size_t offset)
	{
		if (m_ptr)
			return m_ptr + offset + sizeof(DWORD);
		else
			return nullptr;
	}
	char* GetValEnd(size_t offset)
	{
		if (m_ptr)
			return m_ptr + offset + sizeof(DWORD) + *((DWORD*)(m_ptr + offset));
		else
			return nullptr;
	}
};
