/*
*     COPYRIGHT NOTICE
*     Copyright(c) 2017~2018, Team Shanghai Dream Equinox
*     All rights reserved.
*
* @file		CThreadSafeEx.cpp
* @brief	This File is CThreadSafeEx File.
* @author	alopex
* @version	v1.00a
* @date		2018-10-08	v1.00a	alopex	Create This File.
*/
#include "CThreadSafeEx.h"

CThreadSafeEx::CThreadSafeEx()
{
	InitializeCriticalSection(&m_CriticalSection);
}

CThreadSafeEx::~CThreadSafeEx()
{
	DeleteCriticalSection(&m_CriticalSection);
}

void CThreadSafeEx::Enter()
{
	EnterCriticalSection(&m_CriticalSection);
}

void CThreadSafeEx::Leave()
{
	LeaveCriticalSection(&m_CriticalSection);
}

CRITICAL_SECTION * CThreadSafeEx::GetCriticalSection()
{
	CRITICAL_SECTION* pCriticalSection = &m_CriticalSection;
	return pCriticalSection;
}
