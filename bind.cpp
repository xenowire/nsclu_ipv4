#include "bind.h"

// ----------------------------------------------
// CBIND
//
// some NOTEs in this file. check it before mod.
// ----------------------------------------------

CBIND::CBIND( HRESULT* hr )
{
#ifdef _DEBUG
	if( hr == NULL )
		DebugBreak();
#endif

	// check existence of file
	*hr = this->IsFileExists();
	if(FAILED( *hr ))
		return;

	*hr = S_OK;
}
CBIND::~CBIND()
{
	ClearHosts();
	ClearOldHosts();
}

HRESULT
CBIND::IsFileExists()
{
	HANDLE f;

	// zone file (static)
	f = CreateFile(	BIND_ZONEPATH  BIND_ZONEFILE_DXENO_STATIC
		,	0
		,	FILE_SHARE_READ | FILE_SHARE_WRITE
		,	NULL
		,	OPEN_EXISTING
		,	FILE_ATTRIBUTE_NORMAL
		,	NULL
		);
	if( f == INVALID_HANDLE_VALUE )
	{
		CLogger::Log("failed at CBIND::IsFileExists().");
		return E_UNEXPECTED;
	}
	CloseHandle( f );

	return S_OK;	
}

// ----------------------------------------------
// NOTE-0002:
// according to NOTE-0001,
// m_oldhosts no longer stores copy of pointer
// that allocated in AddHost() for m_hosts.
// so, same as m_hosts,
// m_oldhosts must alloc, copy & release it self.
// ----------------------------------------------
HRESULT
CBIND::ClearOldHosts()
{
	// walk
	while( m_oldhosts.size() > 0 )
	{
		// release
		CHAR* b = m_oldhosts.back();
		if( b != NULL )
		{
			delete [] b;
			b = NULL;
		}

		m_oldhosts.pop_back();
	}

// now, we cant use this. ref.: NOTE-0002.
//	m_oldhosts.clear();

	return S_OK;
}
HRESULT
CBIND::AddOldHosts( CHAR* ipaddr )
{
// now, we cant use this. ref.: NOTE-0002.
// m_oldhosts.push_back( ipaddr );

	// alloc
	CHAR* b = new CHAR[ strlen( ipaddr ) + 1 ];
	if( b == NULL )
	{
		return E_OUTOFMEMORY;
	}
	// copy
	strcpy( b, ipaddr );
	// store pointer
	m_oldhosts.push_back( b );

	return S_OK;
}

// ----------------------------------------------
// NOTE-0001:
// when ipaddr was static mem,
// we're ok to simply store their pointer.
// but, ipaddr is now changed into dynamic mem.
// so we must alloc, copy & release our self.
// ----------------------------------------------
HRESULT
CBIND::AddHost( CHAR* ipaddr )
{
	// alloc
	CHAR* b = new CHAR[ strlen( ipaddr ) + 1 ];
	if( b == NULL )
	{
		return E_OUTOFMEMORY;
	}
	// copy
	strcpy( b, ipaddr );
	// store pointer
	m_hosts.push_back( b );

// now, we cant use this. ref.: NOTE-0001.
//	m_hosts.push_back( ipaddr );

	return S_OK;
}

HRESULT
CBIND::ClearHosts()
{
	// walk
	while( m_hosts.size() > 0 )
	{
		// release
		CHAR* b = m_hosts.back();
		if( b != NULL )
		{
			delete [] b;
			b = NULL;
		}

		m_hosts.pop_back();
	}

// now, we cant use this. ref.: NOTE-0001.
//	m_hosts.clear();

	return S_OK;
}

HRESULT
CBIND::WriteHosts()
{
	HANDLE f;

	//CLogger::Log("begin: CBIND::WriteHosts().");

	// create new zone-file
	if( ::CopyFile(	BIND_ZONEPATH  BIND_ZONEFILE_DXENO_STATIC
		,			BIND_ZONEPATH  BIND_ZONEFILE_DXENO
		,			FALSE
		) == 0 
	)
	{
		CLogger::Log("failed at CBIND::WriteHosts() - CopyFile().");
		return E_UNEXPECTED;
	}

	// add hosts to zone-file
	// - open file
	f = ::CreateFile(	BIND_ZONEPATH     BIND_ZONEFILE_DXENO
		,				GENERIC_WRITE
		,				FILE_SHARE_READ | FILE_SHARE_WRITE
		,				NULL
		,				OPEN_EXISTING
		,				FILE_ATTRIBUTE_NORMAL
		,				NULL
		);
	if( f == INVALID_HANDLE_VALUE )
	{
		CLogger::Log("failed at CBIND::WriteHosts() - CreateFile().");
		return E_UNEXPECTED;
	}
	// - goto eof
	::SetFilePointer( f, 0, NULL, FILE_END );
	// - append hosts
	//
	this->ClearOldHosts();
	//
	vector<CHAR*>::iterator ib = m_hosts.begin();
	vector<CHAR*>::iterator ie = m_hosts.end();
	while( ib != ie )
	{
		// generate new line
		CHAR line[1024];
		::sprintf( line, BIND_ZONETEMP_IN_A, (*ib) );

		// write to file
		DWORD dw;
		if( ::WriteFile( f, line, (DWORD)strlen(line), &dw, NULL ) == 0 )
		{
			CLogger::Log("failed at CBIND::WriteHosts() - WriteFile().");

			::CloseHandle( f );
			return E_UNEXPECTED;
		}
	
		//
		this->AddOldHosts( (*ib) );
		//
		ib++;
	}
	// - close file
	::CloseHandle( f );

	//CLogger::Log("succeeded: CBIND::WriteHosts().");

	return S_OK;
}

HRESULT
CBIND::RestartBIND()
{
	HRESULT hr;

	CSCM scm( &hr );
	if(FAILED( hr ))
		return hr;

	scm.Stop( BIND_SERVICENAME );
	scm.Start( BIND_SERVICENAME );

	return S_OK;
}

HRESULT
CBIND::RNDCReload()
{
	STARTUPINFO si;
	memset( &si, 0, sizeof(si) );
	si.cb = sizeof(si);
	PROCESS_INFORMATION pi;

	if( ::CreateProcess( NULL, BIND_RNDC " " BIND_RNDC_COMMAND_RELOAD, NULL, NULL, FALSE, 0 // CREATE_NO_WINDOW
		, NULL, NULL, &si, &pi ) != 0 )
	{
		WaitForSingleObject( pi.hProcess, INFINITE );

		::CloseHandle( pi.hThread );
		::CloseHandle( pi.hProcess );

		return S_OK;
	}

	return E_FAIL;
}

BOOL
CBIND::IsChanged()
{
	// ÉTÉCÉYî‰är
	if( m_oldhosts.size() != m_hosts.size() )
		return TRUE;

	for( UINT i=0; i<m_oldhosts.size(); i++ )
	{
		// ì‡óeî‰är
		if( strcmp( m_oldhosts.at(i), m_hosts.at(i) ) != 0 )
			return TRUE;
	}

	return FALSE;
}

BOOL
CBIND::IsExists( CHAR* ipaddr )
{
	if( m_hosts.size() == 0 )
		return FALSE;

	for( UINT i=0; i<m_hosts.size(); i++ )
	{
#ifdef _DEBUG
		if( m_hosts.at(i) == NULL )
			return FALSE;
#endif

		// ì‡óeî‰är
		if( strcmp( m_hosts.at(i), ipaddr ) == 0 )
			return TRUE;
	}

	return FALSE;
}
