/*
*     COPYRIGHT NOTICE
*     Copyright(c) 2017~2018, Team Shanghai Dream Equinox
*     All rights reserved.
*
* @file		CRosaSerial.cpp
* @brief	This File is RosaSerial Source File.
* @author	alopex
* @version	v1.00a
* @date		2018-09-17	v1.00a	alopex	Create This File.
*/
#include "CRosaSerial.h"
#include "CThreadSafe.h"

//CRosaSerial 串口通信类(异步串行通信)

//------------------------------------------------------------------
// @Function:	 CRosaSerial()
// @Purpose: CRosaSerial构造函数
// @Since: v1.00a
// @Para: None
// @Return: None
//------------------------------------------------------------------
CRosaSerial::CRosaSerial()
{
	m_bOpen = false;
	m_bRecv = false;
	m_hCOM = INVALID_HANDLE_VALUE;
	m_hListenThread = INVALID_HANDLE_VALUE;

	memset(&m_ovWrite, 0, sizeof(m_ovWrite));
	memset(&m_ovRead, 0, sizeof(m_ovRead));
	memset(&m_ovWait, 0, sizeof(m_ovWait));

	memset(m_chSendBuf, 0, sizeof(m_chSendBuf));
	memset(m_chRecvBuf, 0, sizeof(m_chRecvBuf));

	InitializeCriticalSection(&m_csCOMSync);
}

//------------------------------------------------------------------
// @Function:	 ~CRosaSerial()
// @Purpose: CRosaSerial析构函数
// @Since: v1.00a
// @Para: None
// @Return: None
//------------------------------------------------------------------
CRosaSerial::~CRosaSerial()
{
	EnterCriticalSection(&m_csCOMSync);
	m_bOpen = false;
	LeaveCriticalSection(&m_csCOMSync);

	if (INVALID_HANDLE_VALUE != m_hCOM)
	{
		::CloseHandle(m_hCOM);
		m_hCOM = INVALID_HANDLE_VALUE;
	}

	if (NULL != m_ovWrite.hEvent)
	{
		::CloseHandle(m_ovWrite.hEvent);
		m_ovWrite.hEvent = NULL;
	}

	if (NULL != m_ovRead.hEvent)
	{
		::CloseHandle(m_ovRead.hEvent);
		m_ovRead.hEvent = NULL;
	}

	if (NULL != m_ovWait.hEvent)
	{
		::CloseHandle(m_ovWait.hEvent);
		m_ovWait.hEvent = NULL;
	}

	if (INVALID_HANDLE_VALUE != m_hListenThread)
	{
		::WaitForSingleObject(m_hListenThread, INFINITE);
		::CloseHandle(m_hListenThread);
		m_hListenThread = INVALID_HANDLE_VALUE;
	}

	DeleteCriticalSection(&m_csCOMSync);
}

//------------------------------------------------------------------
// @Function:	 CRosaSerialGetStatus()
// @Purpose: CRosaSerial获取当前串口状态
// @Since: v1.00a
// @Para: None
// @Return: bool bRet (true:串口开启, false:串口关闭)
//------------------------------------------------------------------
bool ROSASERIAL_CALLMODE CRosaSerial::CRosaSerialGetStatus() const
{
	CThreadSafe ThreadSafe(&m_csCOMSync);
	return m_bOpen;
}

//------------------------------------------------------------------
// @Function:	 CRosaSerialGetRecv()
// @Purpose: CRosaSerial获取串口接收状态
// @Since: v1.00a
// @Para: None
// @Return: bool bRet (true:正在接收, false:接收完毕)
//------------------------------------------------------------------
bool ROSASERIAL_CALLMODE CRosaSerial::CRosaSerialGetRecv() const
{
	CThreadSafe ThreadSafe(&m_csCOMSync);
	return m_bRecv;
}

//------------------------------------------------------------------
// @Function:	 CRosaSerialGetRecv()
// @Purpose: CRosaSerial设置串口接收状态
// @Since: v1.00a
// @Para: bool bRecv
// @Return: None
//------------------------------------------------------------------
void ROSASERIAL_CALLMODE CRosaSerial::CRosaSerialSetRecv(bool bRecv)
{
	CThreadSafe ThreadSafe(&m_csCOMSync);
	m_bRecv = bRecv;
}

//------------------------------------------------------------------
// @Function:	 CRosaSerialSetSendBuf()
// @Purpose: CRosaSerial设置发送缓冲
// @Since: v1.00a
// @Para: unsigned char * pBuff(发送缓冲数组地址)
// @Para: int nSize(发送缓冲数组长度)
// @Return: None
//------------------------------------------------------------------
void ROSASERIAL_CALLMODE CRosaSerial::CRosaSerialSetSendBuf(unsigned char * pBuff, int nSize)
{
	EnterCriticalSection(&m_csCOMSync);
	memcpy_s(m_chSendBuf, sizeof(m_chSendBuf), pBuff, nSize);
	LeaveCriticalSection(&m_csCOMSync);
}

//------------------------------------------------------------------
// @Function:	 CRosaSerialGetRecvBuf()
// @Purpose: CRosaSerial获取接收缓冲
// @Since: v1.00a
// @Para: unsigned char * pBuff(接收缓冲数组地址)
// @Para: int nSize(接收缓冲数组长度)
// @Return: None
//------------------------------------------------------------------
void ROSASERIAL_CALLMODE CRosaSerial::CRosaSerialGetRecvBuf(unsigned char * pBuff, int nSize)
{
	EnterCriticalSection(&m_csCOMSync);
	memcpy_s(pBuff, nSize, m_chRecvBuf, sizeof(m_chRecvBuf));
	LeaveCriticalSection(&m_csCOMSync);
}

//------------------------------------------------------------------
// @Function:	 CRosaSerialOpenPort()
// @Purpose: CRosaSerial打开串口
// @Since: v1.00a
// @Para: S_SERIALPORT_PROPERTY sCommProperty(串口信息结构体)
// @Return: bool bRet (true:成功, false:失败)
//------------------------------------------------------------------
bool ROSASERIAL_CALLMODE CRosaSerial::CRosaSerialOpenPort(S_SERIALPORT_PROPERTY sCommProperty)
{
	bool bRet = false;

	// 初始化串口
	bRet = CRosaSerialInit(sCommProperty);
	if (!bRet)
	{
		return false;
	}

	// 初始化串口监听
	bRet = CRosaSerialInitListen();
	if (!bRet)
	{
		return false;
	}

	return true;
}

//------------------------------------------------------------------
// @Function:	 CRosaSerialClosePort()
// @Purpose: CRosaSerial关闭串口
// @Since: v1.00a
// @Para: None
// @Return: None
//------------------------------------------------------------------
void ROSASERIAL_CALLMODE CRosaSerial::CRosaSerialClosePort()
{
	CRosaSerialClose();
	CRosaSerialCloseListen();
}

//------------------------------------------------------------------
// @Function:	 OnTranslateBuffer()
// @Purpose: CRosaSerial串口发送线程
// @Since: v1.00a
// @Para: None
// @Return: None
//------------------------------------------------------------------
bool ROSASERIAL_CALLMODE CRosaSerial::OnTranslateBuffer()
{
	BOOL bStatus = FALSE;
	DWORD dwError = 0;
	COMSTAT cs = { 0 };
	DWORD dwBytes = 0;
	BYTE chSendBuf[SERIALPORT_COMM_INPUT_BUFFER_SIZE] = { 0 };

	//ClearCommError(m_hCOM, &dwError, &cs);
	PurgeComm(m_hCOM, PURGE_TXCLEAR | PURGE_TXABORT);
	m_ovWrite.Offset = 0;

	EnterCriticalSection(&m_csCOMSync);
	memset(chSendBuf, 0, sizeof(chSendBuf));
	memcpy_s(chSendBuf, sizeof(chSendBuf), m_chSendBuf, sizeof(m_chSendBuf));
	LeaveCriticalSection(&m_csCOMSync);

	bStatus = WriteFile(m_hCOM, chSendBuf, sizeof(chSendBuf), &dwBytes, &m_ovWrite);
	if (FALSE == bStatus && GetLastError() == ERROR_IO_PENDING)
	{
		if (FALSE == ::GetOverlappedResult(m_hCOM, &m_ovWrite, &dwBytes, TRUE))
		{
			return false;
		}
	}

	return true;
}

//------------------------------------------------------------------
// @Function:	 OnReceiveBuffer()
// @Purpose: CRosaSerial串口接收线程
// @Since: v1.00a
// @Para: None
// @Return: None
//------------------------------------------------------------------
unsigned int CRosaSerial::OnReceiveBuffer(LPVOID lpParameters)
{
	CRosaSerial* pCSerialPortBase = reinterpret_cast<CRosaSerial*>(lpParameters);
	BOOL bStatus = FALSE;
	DWORD dwWaitEvent = 0;
	DWORD dwBytes = 0;
	DWORD dwError = 0;
	COMSTAT cs = { 0 };
	BYTE chReadBuf[SERIALPORT_COMM_OUTPUT_BUFFER_SIZE] = { 0 };

	while (true)
	{
		EnterCriticalSection(&pCSerialPortBase->m_csCOMSync);
		if (!pCSerialPortBase->m_bOpen)
		{
			LeaveCriticalSection(&pCSerialPortBase->m_csCOMSync);
			break;
		}
		LeaveCriticalSection(&pCSerialPortBase->m_csCOMSync);

		dwWaitEvent = 0;
		pCSerialPortBase->m_ovWait.Offset = 0;

		bStatus = ::WaitCommEvent(pCSerialPortBase->m_hCOM, &dwWaitEvent, &pCSerialPortBase->m_ovWait);
		if (FALSE == bStatus && GetLastError() == ERROR_IO_PENDING)
		{
			bStatus = ::GetOverlappedResult(pCSerialPortBase->m_hCOM, &pCSerialPortBase->m_ovWait, &dwBytes, TRUE);
		}

		ClearCommError(pCSerialPortBase->m_hCOM, &dwError, &cs);

		if (TRUE == bStatus && (dwWaitEvent & EV_RXCHAR) && cs.cbInQue > 0)
		{
			dwBytes = 0;
			pCSerialPortBase->m_ovRead.Offset = 0;

			memset(chReadBuf, 0, sizeof(chReadBuf));
			bStatus = ReadFile(pCSerialPortBase->m_hCOM, chReadBuf, sizeof(chReadBuf), &dwBytes, &pCSerialPortBase->m_ovRead);
			PurgeComm(pCSerialPortBase->m_hCOM, PURGE_RXCLEAR | PURGE_RXABORT);

			EnterCriticalSection(&pCSerialPortBase->m_csCOMSync);
			memset(pCSerialPortBase->m_chRecvBuf, 0, sizeof(pCSerialPortBase->m_chRecvBuf));
			memcpy_s(pCSerialPortBase->m_chRecvBuf, sizeof(pCSerialPortBase->m_chRecvBuf), chReadBuf, sizeof(chReadBuf));
			pCSerialPortBase->m_bRecv = true;
			LeaveCriticalSection(&pCSerialPortBase->m_csCOMSync);
		}

	}

	return 0;
}

//------------------------------------------------------------------
// @Function:	 EnumSerialPort()
// @Purpose: CRosaSerial枚举串口
// @Since: v1.00a
// @Para: None
// @Return: None
//------------------------------------------------------------------
void ROSASERIAL_CALLMODE CRosaSerial::EnumSerialPort()
{
	HKEY hKey;
	LSTATUS ls;

	ls = RegOpenKeyExW(HKEY_LOCAL_MACHINE, _T("Hardware\\DeviceMap\\SerialComm"), NULL, KEY_READ, &hKey);
	if (ERROR_SUCCESS == ls)
	{
		TCHAR szPortName[256] = { 0 };
		TCHAR szComName[256] = { 0 };
		DWORD dwLong = 0;
		DWORD dwSize = 0;
		int nCount = 0;

		m_mapEnumCOM.clear();

		while (true)
		{
			LSTATUS ls2;

			dwLong = dwSize = 256;
			ls2 = RegEnumValueW(hKey, nCount, szPortName, &dwLong, NULL, NULL, (PUCHAR)szComName, &dwSize);
			if (ERROR_NO_MORE_ITEMS == ls2)
			{
				break;
			}

			int iLen = WideCharToMultiByte(CP_ACP, 0, szComName, -1, NULL, 0, NULL, NULL);
			char* chRtn = new char[iLen + 1];
			memset(chRtn, 0, iLen + 1);
			WideCharToMultiByte(CP_ACP, 0, szComName, -1, chRtn, iLen, NULL, NULL);

			string str(chRtn);
			m_mapEnumCOM.insert(pair<int, string>(nCount, str));
			SafeDeleteArray(chRtn);
			nCount++;
		}

		RegCloseKey(hKey);
	}

}

//------------------------------------------------------------------
// @Function:	 CRosaSerialCreate()
// @Purpose: CRosaSerial打开串口
// @Since: v1.00a
// @Para: const char * szPort(串口名称)
// @Return: None
//------------------------------------------------------------------
bool ROSASERIAL_CALLMODE CRosaSerial::CRosaSerialCreate(const char * szPort)
{
	DWORD dwError;

	EnterCriticalSection(&m_csCOMSync);

	m_hCOM = CreateFileA(szPort, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, NULL);
	if (INVALID_HANDLE_VALUE == m_hCOM)
	{
		dwError = GetLastError();
		LeaveCriticalSection(&m_csCOMSync);
		return false;
	}

	LeaveCriticalSection(&m_csCOMSync);
	return true;
}

//------------------------------------------------------------------
// @Function:	 CRosaSerialConfig()
// @Purpose: CRosaSerial配置串口
// @Since: v1.00a
// @Para: S_SERIALPORT_PROPERTY sCommProperty(串口信息结构体)
// @Return: None
//------------------------------------------------------------------
bool ROSASERIAL_CALLMODE CRosaSerial::CRosaSerialConfig(S_SERIALPORT_PROPERTY sCommProperty)
{
	BOOL bRet = FALSE;

	EnterCriticalSection(&m_csCOMSync);

	// 设置输入输出缓冲区
	bRet = SetupComm(m_hCOM, SERIALPORT_COMM_INPUT_BUFFER_SIZE, SERIALPORT_COMM_OUTPUT_BUFFER_SIZE);
	if (!bRet)
	{
		LeaveCriticalSection(&m_csCOMSync);
		return false;
	}

	// 设置DCB结构体
	DCB dcb = { 0 };

	bRet = GetCommState(m_hCOM, &dcb);
	if (!bRet)
	{
		LeaveCriticalSection(&m_csCOMSync);
		return false;
	}

	dcb.DCBlength = sizeof(dcb);
	dcb.BaudRate = sCommProperty.dwBaudRate;
	dcb.ByteSize = sCommProperty.byDataBits;
	dcb.StopBits = sCommProperty.byStopBits;
	dcb.Parity = sCommProperty.byCheckBits;
	bRet = SetCommState(m_hCOM, &dcb);
	if (!bRet)
	{
		LeaveCriticalSection(&m_csCOMSync);
		return false;
	}

	// 设置串口超时时间
	COMMTIMEOUTS ct = { 0 };
	ct.ReadIntervalTimeout = MAXDWORD;
	ct.ReadTotalTimeoutMultiplier = 0;
	ct.ReadTotalTimeoutConstant = 0;
	ct.WriteTotalTimeoutMultiplier = 500;
	ct.WriteTotalTimeoutConstant = 5000;
	bRet = SetCommTimeouts(m_hCOM, &ct);
	if (!bRet)
	{
		LeaveCriticalSection(&m_csCOMSync);
		return false;
	}

	// 清空串口缓冲区
	bRet = PurgeComm(m_hCOM, PURGE_TXABORT | PURGE_RXABORT | PURGE_TXCLEAR | PURGE_RXCLEAR);
	if (!bRet)
	{
		LeaveCriticalSection(&m_csCOMSync);
		return false;
	}

	// 创建事件对象
	m_ovRead.hEvent = CreateEvent(NULL, false, false, NULL);
	m_ovWrite.hEvent = CreateEvent(NULL, false, false, NULL);
	m_ovWait.hEvent = CreateEvent(NULL, false, false, NULL);

	SetCommMask(m_hCOM, EV_ERR | EV_RXCHAR);

	LeaveCriticalSection(&m_csCOMSync);
	return true;
}

//------------------------------------------------------------------
// @Function:	 CRosaSerialInit()
// @Purpose: CRosaSerial初始化串口
// @Since: v1.00a
// @Para: S_SERIALPORT_PROPERTY sCommProperty(串口信息结构体)
// @Return: None
//------------------------------------------------------------------
bool ROSASERIAL_CALLMODE CRosaSerial::CRosaSerialInit(S_SERIALPORT_PROPERTY sCommProperty)
{
	bool bRet = false;

	bRet = CRosaSerialCreate(sCommProperty.chPort);
	if (!bRet)
	{
		return false;
	}

	bRet = CRosaSerialConfig(sCommProperty);
	if (!bRet)
	{
		return false;
	}

	EnterCriticalSection(&m_csCOMSync);
	m_bOpen = true;
	LeaveCriticalSection(&m_csCOMSync);

	return true;
}

//------------------------------------------------------------------
// @Function:	 CRosaSerialInitListen()
// @Purpose: CRosaSerial初始化串口监听
// @Since: v1.00a
// @Para: None
// @Return: None
//------------------------------------------------------------------
bool ROSASERIAL_CALLMODE CRosaSerial::CRosaSerialInitListen()
{
	if (INVALID_HANDLE_VALUE != m_hListenThread)
	{
		return false;
	}

	unsigned int uThreadID;

	m_hListenThread = (HANDLE)::_beginthreadex(NULL, 0, (_beginthreadex_proc_type)OnReceiveBuffer, this, 0, &uThreadID);
	if (!m_hListenThread)
	{
		return false;
	}

	BOOL bRet = FALSE;
	bRet = ::SetThreadPriority(m_hListenThread, THREAD_PRIORITY_ABOVE_NORMAL);
	if (!bRet)
	{
		return false;
	}

	return true;
}

//------------------------------------------------------------------
// @Function:	 CRosaSerialClose()
// @Purpose: CRosaSerial关闭串口
// @Since: v1.00a
// @Para: None
// @Return: None
//------------------------------------------------------------------
void ROSASERIAL_CALLMODE CRosaSerial::CRosaSerialClose()
{
	EnterCriticalSection(&m_csCOMSync);
	m_bOpen = false;
	LeaveCriticalSection(&m_csCOMSync);

	if (INVALID_HANDLE_VALUE != m_hCOM)
	{
		::CloseHandle(m_hCOM);
		m_hCOM = INVALID_HANDLE_VALUE;
	}

	if (NULL != m_ovWrite.hEvent)
	{
		::CloseHandle(m_ovWrite.hEvent);
		m_ovWrite.hEvent = NULL;
	}

	if (NULL != m_ovRead.hEvent)
	{
		::CloseHandle(m_ovRead.hEvent);
		m_ovRead.hEvent = NULL;
	}

	if (NULL != m_ovWait.hEvent)
	{
		::CloseHandle(m_ovWait.hEvent);
		m_ovWait.hEvent = NULL;
	}

}

//------------------------------------------------------------------
// @Function:	 CRosaSerialCloseListen()
// @Purpose: CRosaSerial关闭串口监听
// @Since: v1.00a
// @Para: None
// @Return: None
//------------------------------------------------------------------
void ROSASERIAL_CALLMODE CRosaSerial::CRosaSerialCloseListen()
{
	if (INVALID_HANDLE_VALUE != m_hListenThread)
	{
		::WaitForSingleObject(m_hListenThread, INFINITE);
		::CloseHandle(m_hListenThread);
		m_hListenThread = INVALID_HANDLE_VALUE;
	}

}
