#include "scm.h"

CSCM::CSCM( HRESULT* hr )
:	m_scm( NULL )
{
#ifdef _DEBUG
	if( hr == NULL )
		DebugBreak();
#endif

	// open ServiceControlManager
	m_scm = ::OpenSCManager( NULL, NULL, GENERIC_EXECUTE );
	if( m_scm == NULL )
	{
		*hr = E_ACCESSDENIED;
		return;
	}

	*hr = S_OK;
}
CSCM::~CSCM()
{
	if( m_scm != NULL )
	{
		CloseServiceHandle( m_scm );
		m_scm = NULL;
	}
}


HRESULT
CSCM::Start( CHAR* svcname )
{
	HRESULT hr = S_OK;

	if( m_scm == NULL )
		return E_UNEXPECTED;

	// open
	SC_HANDLE s = ::OpenService( m_scm, svcname, SERVICE_QUERY_STATUS | SERVICE_START );
	if( s == NULL )
	{
		return E_FAIL;
	}
	
	//CLogger::Log("begin: CSCM::Start()");

	// start
	if( ::StartService( s, 0, NULL ) == 0 )
	{
		DWORD e = GetLastError();
		if( e != ERROR_SERVICE_ALREADY_RUNNING )
		{
			CLogger::Log("failed at CSCM::Start() - StartService().");
			hr = E_FAIL;
		}
	}

	if(SUCCEEDED( hr ))
	{
		// wait for started
		for(;;)
		{
			SERVICE_STATUS ss;
			if( ::QueryServiceStatus( s, &ss ) == 0 )
			{
				CLogger::Log("failed at CSCM::Start() - QueryServiceStatus().");
				hr = E_FAIL;
				break;
			}

			if( ss.dwCurrentState = SERVICE_RUNNING )
			{
				break;
			}

			::Sleep( 500 );
		}
	}

	// close
	CloseServiceHandle( s );

	//CLogger::Log("end: CSCM::Start()");

	return hr;
}

HRESULT
CSCM::Stop( CHAR* svcname )
{
	HRESULT hr = S_OK;

	if( m_scm == NULL )
		return E_UNEXPECTED;

	// open
	SC_HANDLE s = ::OpenService( m_scm, svcname, SERVICE_QUERY_STATUS | SERVICE_STOP );
	if( s == NULL )
	{
		return E_FAIL;
	}

	//CLogger::Log("begin: CSCM::Stop()");

	// stop
	SERVICE_STATUS ss;
	if( ControlService( s, SERVICE_CONTROL_STOP, &ss ) == 0 )
	{
		if( ss.dwCurrentState == SERVICE_STOPPED )
		{
			hr = S_OK;
		}
		else
		{
			hr = E_FAIL;
		}
	}

	if(SUCCEEDED( hr ))
	{
		// wait for stopped
		for(;;)
		{
			SERVICE_STATUS ss;
			if( ::QueryServiceStatus( s, &ss ) == 0 )
			{
				CLogger::Log("failed at CSCM::Stop() - QueryServiceStatus().");
				hr = E_FAIL;
				break;
			}

			if( ss.dwCurrentState == SERVICE_STOPPED )
			{
				break;
			}

			::Sleep( 500 );
		}
	}

	// close
	CloseServiceHandle( s );

	//CLogger::Log("end: CSCM::Stop()");

	return hr;
}
