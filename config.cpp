#include "config.h"

CConfig::CConfig( HRESULT *hr, const CHAR* fn )
:	m_fn( NULL )
,	m_weightControlThread( INVALID_HANDLE_VALUE )
,	m_weightControlThreadStopEvent( INVALID_HANDLE_VALUE )
,	HostsSize( 0 )
{
	// base arg check
	if( hr == NULL )
		DebugBreak();

	*hr = S_OK;

	// args check
	if( fn == NULL )
	{
		*hr = E_INVALIDARG;
		return;
	}

	// find file
	HANDLE f = INVALID_HANDLE_VALUE;
	f = CreateFile(	fn
		,	0
		,	FILE_SHARE_READ
		,	NULL
		,	OPEN_EXISTING
		,	FILE_ATTRIBUTE_NORMAL
		,	NULL
		);
	if( f == INVALID_HANDLE_VALUE )
	{
		CLogger::Log("failed at CConfig::CConfig(): file not found.");
		*hr = E_UNEXPECTED;
		return;
	}
	CloseHandle( f );

	// store filename
	m_fn = new CHAR[ strlen( fn ) + 1 ];
	if( m_fn == NULL )
	{
		*hr = E_OUTOFMEMORY;
		return;
	}
	strcpy( m_fn, fn );

	//
	this->reload();
#ifdef _DEBUG
	// reload test
	this->reload();
#endif
	return;	
}
CConfig::~CConfig()
{
	// stop threads
	this->stopWeightControlThread();

	//
	this->clearHosts();

	//
	if( m_fn != NULL )
	{
		delete [] m_fn;
		m_fn = NULL;
	}
}

void
CConfig::clearHostEntry( HOST* h )
{
	if( h->hostname != NULL )
	{
		delete [] h->hostname;
		h->hostname = NULL;
	}
	if( h->ipaddr != NULL )
	{
		delete [] h->ipaddr;
		h->ipaddr = NULL;
	}
	if( h->url != NULL )
	{
		delete [] h->url;
		h->url = NULL;
	}
	if( h->lock != NULL )
	{
		delete h->lock;
		h->lock = NULL;
	}
}

void
CConfig::clearHosts()
{
	while( this->Hosts.size() > 0 )
	{
		HOST* h = this->Hosts.back();
		if( h != NULL )
		{
			this->clearHostEntry( h );
			delete h;
		}

		this->Hosts.pop_back();
	}

	this->Hosts.clear();
	this->HostsSize = 0;
}

HRESULT
CConfig::reload()
{
	HRESULT hr = S_OK;

	// control thread: 1
	// - stop existing one
	if(FAILED( hr = this->stopWeightControlThread() ))
		return hr;

	// load
	m_NSCLU_CHECK_INTERVAL = GetPrivateProfileInt( CONFIG_APPNAME,
		"NSCLU_CHECK_INTERVAL", NSCLU_CHECK_INTERVAL, m_fn );
	m_NSCLU_HOSTS_AT_LEAST = GetPrivateProfileInt( CONFIG_APPNAME,
		"NSCLU_HOSTS_AT_LEAST", NSCLU_HOSTS_AT_LEAST, m_fn );
	m_NSCLU_SERVER_TIMEOUT = GetPrivateProfileInt( CONFIG_APPNAME,
		"NSCLU_SERVER_TIMEOUT", NSCLU_SERVER_TIMEOUT, m_fn );
	m_NSCLU_WEIGHTREDUCE_PROCFREQ = GetPrivateProfileInt( CONFIG_APPNAME,
		"NSCLU_WEIGHTREDUCE_PROCFREQ", NSCLU_WEIGHTREDUCE_PROCFREQ, m_fn );
	m_NSCLU_FAILCOUNT_STOP = GetPrivateProfileInt( CONFIG_APPNAME,
		"NSCLU_FAILCOUNT_STOP", NSCLU_FAILCOUNT_STOP, m_fn );

	// - hosts
	// -- cleanup
	this->clearHosts();

	CHAR sns[ 0x10000 ];
	DWORD snsl = GetPrivateProfileSectionNames( sns, sizeof(sns), m_fn );
	for( size_t p = 0; p < snsl ; p += strlen( sns + p ) + 1 )
	{
		if( strncmp( sns + p, CONFIG_HOSTPREFIX, strlen(CONFIG_HOSTPREFIX) ) != 0 )
			continue;

		// alloc host
		HOST* h = new HOST;

		// - init pointer
		h->hostname = NULL;
		h->ipaddr = NULL;
		h->url = NULL;
		h->lock = NULL;

		// - create lock
		h->lock = new CCritSection();
		if( h->lock == NULL )
			return E_OUTOFMEMORY;

		// - read
		hr = this->getPrivateProfileHOST( sns + p, h );
		if(FAILED( hr ))
		{
			this->clearHostEntry( h );
			delete h;

			this->clearHosts();
			return hr;
		}

		this->Hosts.push_back( h );
	}		
	
	// check
	if( this->Hosts.size() == 0 )
		return E_UNEXPECTED;

	// cache hosts size
	this->HostsSize = this->Hosts.size();

	// control thread: 2
	// - create thread
	m_weightControlThreadStopEvent = CreateEvent( NULL, TRUE, FALSE, NULL );
	if( m_weightControlThreadStopEvent == INVALID_HANDLE_VALUE )
		return E_FAIL;
	DWORD tid;
	m_weightControlThread = CreateThread( NULL, 0, this->initialThreadProc, this, 0, &tid );
	if( m_weightControlThread == INVALID_HANDLE_VALUE )
		return HRESULT_FROM_WIN32( GetLastError() ); 

	return hr;
}

DWORD
CConfig::weightControlProc()
{
	for(;;)
	{
		for( UINT i=0; i<this->HostsSize; i++ )
		{
			HOST* h = (HOST*)this->Hosts.at( i );
			if( h->weight > h->weightMin )
			{
				// get time
				time_t now;
				time( &now );

				if( now > h->weightLastReduced + h->weightKeepDuration )
				{
					// reduce weight
					CCritSectionAutoLock l( h->lock );

					h->weight = max( h->weight - h->weightReduceRate, h->weightMin );
					h->weightLastReduced = now;

					CHAR s[256];
					sprintf( s, "[ INFO ] %s change weight(cur:%f, min:%f, max:%f)", h->ipaddr, h->weight, h->weightMin, h->weightMax );
					CLogger::Log( s );
				}
			}
		}

		if( WaitForSingleObject( this->m_weightControlThreadStopEvent, this->m_NSCLU_WEIGHTREDUCE_PROCFREQ ) != WAIT_TIMEOUT )
			break;
	}

	return 0;
}

HRESULT
CConfig::stopWeightControlThread()
{
	if( m_weightControlThreadStopEvent != INVALID_HANDLE_VALUE )
	{
		SetEvent( m_weightControlThreadStopEvent );
		
		if( m_weightControlThread != INVALID_HANDLE_VALUE )
		{
			WaitForSingleObject( m_weightControlThread, INFINITE );
			//
			CloseHandle( m_weightControlThread );
			m_weightControlThread = INVALID_HANDLE_VALUE;
		}

		//
		CloseHandle( m_weightControlThreadStopEvent );
		m_weightControlThreadStopEvent = INVALID_HANDLE_VALUE;
	}

	return S_OK;
}

HRESULT
CConfig::getPrivateProfileHOST( const CHAR* appName, HOST* h )
{
	HRESULT hr = S_OK;

	// buffer
	CHAR b[ 4096 ];
	DWORD l;

	l = GetPrivateProfileString( appName, "url", "", b, sizeof(b), m_fn );
	if( l == 0 )
	{
		hr = E_UNEXPECTED;
		return hr;
	}
	h->url = new CHAR[ l+1 ];
	strcpy( h->url, b );

	l = GetPrivateProfileString( appName, "hostname", "", b, sizeof(b), m_fn );
	if( l == 0 )
	{
		hr = E_UNEXPECTED;
		return hr;
	}
	h->hostname = new CHAR[ l+1 ];
	strcpy( h->hostname, b );

	// time avoidance
	h->useTimeAvoidance = GetPrivateProfileInt( appName, "useTimeAvoidance", 0, m_fn );
	h->avoidBeginInMinute = GetPrivateProfileInt( appName, "avoidBeginInMinute", 0, m_fn );
	h->avoidEndInMinute = GetPrivateProfileInt( appName, "avoidEndInMinute", 0, m_fn );

	// pv limit
	h->pvLimit = GetPrivateProfileInt( appName, "pvLimit", 0, m_fn );
	h->pvLimitNoted = FALSE;

	// weight
	//
	l = GetPrivateProfileString( appName, "weightMin", "", b, sizeof(b), m_fn );
	if( l == 0 )
		return E_UNEXPECTED;
	h->weightMin = strtod( b, NULL );
	h->weight = h->weightMin;
	//
	l = GetPrivateProfileString( appName, "weightMax", "", b, sizeof(b), m_fn );
	if( l == 0 )
		return E_UNEXPECTED;
	h->weightMax = strtod( b, NULL );
	//
	l = GetPrivateProfileString( appName, "weightGainRate", "", b, sizeof(b), m_fn );
	if( l == 0 )
		return E_UNEXPECTED;
	h->weightGainRate = strtod( b, NULL );
	//
	l = GetPrivateProfileString( appName, "weightReduceRate", "", b, sizeof(b), m_fn );
	if( l == 0 )
		return E_UNEXPECTED;
	h->weightReduceRate = strtod( b, NULL );
	h->weightKeepDuration = GetPrivateProfileInt( appName, "weightKeepDuration", 0, m_fn );
	//
	time_t t;
	time( &t );
	h->weightLastReduced = t;

	return hr;
}

UINT
CConfig::get_NSCLU_CHECK_INTERVAL()
{
	return m_NSCLU_CHECK_INTERVAL;
}
UINT
CConfig::get_NSCLU_HOSTS_AT_LEAST()
{
	return m_NSCLU_HOSTS_AT_LEAST;
}
UINT
CConfig::get_NSCLU_SERVER_TIMEOUT()
{
	return m_NSCLU_SERVER_TIMEOUT;
}
UINT
CConfig::get_NSCLU_WEIGHTREDUCE_PROCFREQ()
{
	return m_NSCLU_WEIGHTREDUCE_PROCFREQ;
}
UINT
CConfig::get_NSCLU_FAILCOUNT_STOP()
{
	return m_NSCLU_FAILCOUNT_STOP;
}
