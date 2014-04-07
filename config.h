#pragma once

#include <windows.h>

#include "generic.h"
#include "logger.h"

#include <vector>
using namespace std;

class
CConfig
{
public:
	CConfig( HRESULT *hr, const CHAR* fn );
	~CConfig();

private:
	CHAR*		m_fn;

	// order
public:
	HRESULT		reload();

	// ini control
private:
	HRESULT		getPrivateProfileHOST( const CHAR* appName, HOST* h );

	// weight control proc
	DWORD		weightControlProc();
	HANDLE		m_weightControlThread;
	HANDLE		m_weightControlThreadStopEvent;
	static DWORD WINAPI
	initialThreadProc(LPVOID pv)	// thread dispatcher
	{
		CConfig* pThis = (CConfig*)pv;
		return pThis->weightControlProc();
	};
	HRESULT		stopWeightControlThread();

	// vars
	// - checks
public:
	UINT		get_NSCLU_CHECK_INTERVAL();
private:
	UINT		m_NSCLU_CHECK_INTERVAL;
public:
	UINT		get_NSCLU_WEIGHTREDUCE_PROCFREQ();
private:
	UINT		m_NSCLU_WEIGHTREDUCE_PROCFREQ;

	// - others
public:
	UINT		get_NSCLU_HOSTS_AT_LEAST();
private:
	UINT		m_NSCLU_HOSTS_AT_LEAST;
public:
	UINT		get_NSCLU_SERVER_TIMEOUT();
private:
	UINT		m_NSCLU_SERVER_TIMEOUT;
public:
	UINT		get_NSCLU_FAILCOUNT_STOP();
private:
	UINT		m_NSCLU_FAILCOUNT_STOP;

	// - hosts
public:
	vector< HOST* >	Hosts;
	size_t			HostsSize;
private:
	void			CConfig::clearHosts();
	void			CConfig::clearHostEntry( HOST* h );
};
