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

//CRosaSerial ����ͨ����(�첽����ͨ��)

//------------------------------------------------------------------
// @Function:	 CRosaSerial()
// @Purpose: CRosaSerial���캯��
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

	m_dwSendCount = 0;
	m_dwRecvCount = 0;
	memset(m_chSendBuf, 0, sizeof(m_chSendBuf));
	memset(m_chRecvBuf, 0, sizeof(m_chRecvBuf));

	InitializeCriticalSection(&m_csCOMSync);
}

//------------------------------------------------------------------
// @Function:	 ~CRosaSerial()
// @Purpose: CRosaSerial��������
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
// @Purpose: CRosaSerial��ȡ��ǰ����״̬
// @Since: v1.00a
// @Para: None
// @Return: bool bRet (true:���ڿ���, false:���ڹر�)
//------------------------------------------------------------------
bool ROSASERIAL_CALLMODE CRosaSerial::CRosaSerialGetStatus() const
{
	CThreadSafe ThreadSafe(&m_csCOMSync);
	return m_bOpen;
}

//------------------------------------------------------------------
// @Function:	 CRosaSerialGetRecv()
// @Purpose: CRosaSerial��ȡ���ڽ���״̬
// @Since: v1.00a
// @Para: None
// @Return: bool bRet (true:���ڽ���, false:�������)
//------------------------------------------------------------------
bool ROSASERIAL_CALLMODE CRosaSerial::CRosaSerialGetRecv() const
{
	CThreadSafe ThreadSafe(&m_csCOMSync);
	return m_bRecv;
}

//------------------------------------------------------------------
// @Function:	 CRosaSerialGetRecv()
// @Purpose: CRosaSerial���ô��ڽ���״̬
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
// @Purpose: CRosaSerial���÷��ͻ���
// @Since: v1.00a
// @Para: unsigned char * pBuff(���ͻ��������ַ)
// @Para: int nSize(���ͻ������鳤��)
// @Return: None
//------------------------------------------------------------------
void ROSASERIAL_CALLMODE CRosaSerial::CRosaSerialSetSendBuf(unsigned char * pBuff, int nSize, DWORD& dwSendCount)
{
	EnterCriticalSection(&m_csCOMSync);
	m_dwSendCount = dwSendCount;
	memcpy_s(m_chSendBuf, sizeof(m_chSendBuf), pBuff, nSize);
	LeaveCriticalSection(&m_csCOMSync);
}

//------------------------------------------------------------------
// @Function:	 CRosaSerialGetRecvBuf()
// @Purpose: CRosaSerial��ȡ���ջ���
// @Since: v1.00a
// @Para: unsigned char * pBuff(���ջ��������ַ)
// @Para: int nSize(���ջ������鳤��)
// @Return: None
//------------------------------------------------------------------
void ROSASERIAL_CALLMODE CRosaSerial::CRosaSerialGetRecvBuf(unsigned char * pBuff, int nSize, DWORD& dwRecvCount)
{
	EnterCriticalSection(&m_csCOMSync);
	dwRecvCount = m_dwRecvCount;
	memcpy_s(pBuff, nSize, m_chRecvBuf, sizeof(m_chRecvBuf));
	LeaveCriticalSection(&m_csCOMSync);
}

//------------------------------------------------------------------
// @Function:	 CRosaSerialOpenPort()
// @Purpose: CRosaSerial�򿪴���
// @Since: v1.00a
// @Para: S_SERIALPORT_PROPERTY sCommProperty(������Ϣ�ṹ��)
// @Return: bool bRet (true:�ɹ�, false:ʧ��)
//------------------------------------------------------------------
bool ROSASERIAL_CALLMODE CRosaSerial::CRosaSerialOpenPort(S_SERIALPORT_PROPERTY sCommProperty)
{
	bool bRet = false;

	// ��ʼ������
	bRet = CRosaSerialInit(sCommProperty);
	if (!bRet)
	{
		return false;
	}

	// ��ʼ�����ڼ���
	bRet = CRosaSerialInitListen();
	if (!bRet)
	{
		return false;
	}

	return true;
}

//------------------------------------------------------------------
// @Function:	 CRosaSerialClosePort()
// @Purpose: CRosaSerial�رմ���
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
// @Purpose: CRosaSerial���ڷ����߳�
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

	bStatus = WriteFile(m_hCOM, chSendBuf, m_dwSendCount, &dwBytes, &m_ovWrite);
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
// @Purpose: CRosaSerial���ڽ����߳�
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
			pCSerialPortBase->m_dwRecvCount = dwBytes;
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
// @Purpose: CRosaSerialö�ٴ���
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
// @Purpose: CRosaSerial�򿪴���
// @Since: v1.00a
// @Para: const char * szPort(��������)
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
// @Purpose: CRosaSerial���ô���
// @Since: v1.00a
// @Para: S_SERIALPORT_PROPERTY sCommProperty(������Ϣ�ṹ��)
// @Return: None
//------------------------------------------------------------------
bool ROSASERIAL_CALLMODE CRosaSerial::CRosaSerialConfig(S_SERIALPORT_PROPERTY sCommProperty)
{
	BOOL bRet = FALSE;

	EnterCriticalSection(&m_csCOMSync);

	// �����������������
	bRet = SetupComm(m_hCOM, SERIALPORT_COMM_INPUT_BUFFER_SIZE, SERIALPORT_COMM_OUTPUT_BUFFER_SIZE);
	if (!bRet)
	{
		LeaveCriticalSection(&m_csCOMSync);
		return false;
	}

	// ����DCB�ṹ��
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

	// ���ô��ڳ�ʱʱ��
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

	// ��մ��ڻ�����
	bRet = PurgeComm(m_hCOM, PURGE_TXABORT | PURGE_RXABORT | PURGE_TXCLEAR | PURGE_RXCLEAR);
	if (!bRet)
	{
		LeaveCriticalSection(&m_csCOMSync);
		return false;
	}

	// �����¼�����
	m_ovRead.hEvent = CreateEvent(NULL, false, false, NULL);
	m_ovWrite.hEvent = CreateEvent(NULL, false, false, NULL);
	m_ovWait.hEvent = CreateEvent(NULL, false, false, NULL);

	SetCommMask(m_hCOM, EV_ERR | EV_RXCHAR);

	LeaveCriticalSection(&m_csCOMSync);
	return true;
}

//------------------------------------------------------------------
// @Function:	 CRosaSerialInit()
// @Purpose: CRosaSerial��ʼ������
// @Since: v1.00a
// @Para: S_SERIALPORT_PROPERTY sCommProperty(������Ϣ�ṹ��)
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
// @Purpose: CRosaSerial��ʼ�����ڼ���
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
// @Purpose: CRosaSerial�رմ���
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
// @Purpose: CRosaSerial�رմ��ڼ���
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
