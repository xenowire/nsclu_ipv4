
#include "resolver.h"
#include "bind.h"
#include "logger.h"
#include "watchdog.h"
#include "generic.h"
//#include "hosts.h"
#include "config.h"

#include <windows.h>
#include <winnt.h>
#include <wininet.h>
#include <math.h>
#include <time.h>

#include <map>
using namespace std;

::WSADATA g_wsad;

HANDLE g_extevent = INVALID_HANDLE_VALUE;
SECURITY_DESCRIPTOR g_exteventDesc;
SECURITY_ATTRIBUTES g_exteventAttr;

int
main()
{
	HRESULT hr;

	CLogger::Log( "[ INFO ] starting nsclu service..." );

	// initialize:
	// - config
	// -- make absolute path
	CHAR cfpath[ 1024 ];
	GetCurrentDirectory( sizeof(cfpath), cfpath );
	if( cfpath[ strlen( cfpath ) - 1 ] != '\\' )
	{
		strcat( cfpath, "\\" );
	}
	strcat( cfpath, CONFIG_FILENAME );
	// -- create
	CConfig cfg( &hr, cfpath );
	if(FAILED( hr ))
	{
		CLogger::Log( "[ CRIT ] failed to create CConfig instance." );
		return -1;
	}
	// -- hosts
	for( UINT i=0; i<cfg.HostsSize; i++ )
		cfg.Hosts.at(i)->failStatus = 0;
	// - event
	InitializeSecurityDescriptor(&g_exteventDesc, SECURITY_DESCRIPTOR_REVISION);
	//
	DWORD psidl = 0;
	CHAR sdname[256] = "";
	DWORD sdnamel = sizeof( sdname );
	SID_NAME_USE suse;
	LookupAccountName( NULL, "Administrators", NULL, &psidl, sdname, &sdnamel, &suse );
	PSID* psid = (PSID*)new BYTE[ psidl ];	// ** must be released afeter
	if( psid == NULL )
	{
		CLogger::Log( "[ CRIT ] failed to memalloc." );
		return -1;
	}
	LookupAccountName( NULL, "Administrators", psid, &psidl, sdname, &sdnamel, &suse );
	//
	const UINT pacl = 1024;
	ACL* pac = (ACL*)new BYTE[ pacl ];				// ** must be released afeter
	if( pac == NULL )
	{
		CLogger::Log( "[ CRIT ] failed to memalloc." );
		return -1;
	}
	InitializeAcl( pac, pacl, ACL_REVISION);
    //
	AddAccessAllowedAce( pac, ACL_REVISION, GENERIC_ALL, psid );
    //
	SetSecurityDescriptorDacl( &g_exteventDesc, TRUE, pac, FALSE);
	//
	g_exteventAttr.lpSecurityDescriptor = &g_exteventDesc;
	g_exteventAttr.bInheritHandle = TRUE;
	g_exteventAttr.nLength = sizeof( g_exteventAttr );
	g_extevent = CreateEvent( &g_exteventAttr, TRUE, FALSE, NSCLU_EXTCALL_EVENTNAME );
	if( g_extevent == INVALID_HANDLE_VALUE )
	{
		CLogger::Log( "[ CRIT ] failed to CreateEvent." );
		return -1;
	}
	//
	delete [] psid;
	delete [] pac;

	// - winsock
	::WSAStartup( MAKEWORD(1,1), &g_wsad );
	// - bind manager
	CBIND b( &hr );
	if(FAILED( hr ))
	{
		CLogger::Log( "[ CRIT ] failed to create CBIND instance." );
		return -1;
	}
	// initialized.

	// start watching:
	// - create wininet
	HINTERNET inet = ::InternetOpen( INET_UA, INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0 );
	if( inet == NULL )
	{
		CLogger::Log( "[ CRIT ] failed at InternetOpen()." );
		return -1;
	}
	// - settings of wininet
	DWORD v;
	v = cfg.get_NSCLU_SERVER_TIMEOUT();
	::InternetSetOption( inet, INTERNET_OPTION_CONNECT_TIMEOUT, &v, sizeof(v) );
	::InternetSetOption( inet, INTERNET_OPTION_DATA_SEND_TIMEOUT, &v, sizeof(v) );
	::InternetSetOption( inet, INTERNET_OPTION_DATA_RECEIVE_TIMEOUT, &v, sizeof(v) );
	::InternetSetOption( inet, INTERNET_OPTION_CONTROL_RECEIVE_TIMEOUT, &v, sizeof(v) );
	::InternetSetOption( inet, INTERNET_OPTION_CONTROL_SEND_TIMEOUT, &v, sizeof(v) );
	v = 0;
	::InternetSetOption( inet, INTERNET_OPTION_CONNECT_RETRIES, &v, sizeof(v) );

	CLogger::Log( "[ INFO ] initialize completed." );

	//
	map<ULONGLONG, CHAR*, less<ULONGLONG> > availsvs;
	map<ULONGLONG, CHAR*, less<ULONGLONG> > subsvs;

	for(;;)
	{
		//CLogger::Log( "begin: check servers" );

		// * clear
		UINT avail = 0;
		availsvs.clear();
		subsvs.clear();

		// * get time
		time_t t = time( NULL );
		tm* now = localtime( &t );
		UINT nowInMinute = now->tm_hour * 60 + now->tm_min;

		// * check
		for( UINT i=0; i<cfg.HostsSize; i++ )
		{
			HOST*	h = cfg.Hosts.at(i);
			BOOL	failed = FALSE;

			// get host addr
			// - free memory
			if( h->ipaddr != NULL )
			{
				delete [] h->ipaddr;
				h->ipaddr = NULL;
			}
			// - resolve ("ipaddr" memory is allocated automatically)
			if(FAILED( CResolver::ResolveHost( &(h->ipaddr), h->hostname ) ))
			{
				// fail
				failed = TRUE;

				// log
				CHAR s[256];
				sprintf( s, "[ NOTE ] can't resolve %s (CResolver::ResolveHost).", h->hostname );
				CLogger::Log( s );
				goto next_failaction;
			}

			//
			CHAR errorLevel[64] = "INFO";
			if( b.IsExists( h->ipaddr ) )
			{
				// it was currently using addr,
				// so up the level to WARN.
				strcpy( errorLevel, "WARN" );
			}

			// connect, send request
			HINTERNET u = ::InternetOpenUrl( inet, h->url , NULL, 0, INTERNET_FLAG_RELOAD, 0 );
			if( u == NULL )
			{
				// fail
				failed = TRUE;

				// log
				CHAR s[256];
				sprintf( s, "[ %s ] %s is down (connect).", errorLevel, h->ipaddr );
				CLogger::Log( s );
				goto next_failaction;
			}

			// get response
			DWORD code;
			DWORD l = sizeof( code );
			::HttpQueryInfo( u
				, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER	// HTTP_QUERY_FLAG_NUMBER は文字列でなく数値で返す指示
				, &code, &l, NULL );

			// check response code
			if( code < 200
			||  code >=  300
			)
			{
				failed = TRUE;

				CHAR s[256];
				sprintf( s, "[ %s ] %s is down (response).", errorLevel, h->ipaddr );
				CLogger::Log( s );
				goto next_inetclean;
			}
			// acceptable status.

			// prepare header buffer
			LPVOID lpHeader = NULL;	// ヘッダバッファ; 要解放.
			l = 0;

			// get header size
			if( ::HttpQueryInfo( u, HTTP_QUERY_RAW_HEADERS_CRLF, lpHeader, &l, NULL ) )
			{
				// must be return "false"

				failed = TRUE;

				CHAR s[256];
				sprintf( s, "[ %s ] %s doesnt respond @HttpQueryInfo (get_header_length).", errorLevel, h->ipaddr );
				CLogger::Log( s );
				goto next_headerclean;
			}
			if( ::GetLastError() == ERROR_HTTP_HEADER_NOT_FOUND )
			{
				// header not found
				failed = TRUE;

				CHAR s[256];
				sprintf( s, "[ %s ] %s is down (header_not_found).", errorLevel, h->ipaddr );
				CLogger::Log( s );
				goto next_headerclean;
			}
			if( GetLastError() != ERROR_INSUFFICIENT_BUFFER )
			{
				// unexpected error
				failed = TRUE;

				CHAR s[256];
				sprintf( s, "[ %s ] %s is down (HttpQueryInfo_Phase1 : INSUFFICIENT_BUFFER).", errorLevel, h->ipaddr );
				CLogger::Log( s );
				goto next_headerclean;
			}

			// allocate header buffer
			lpHeader = new CHAR[ l ];
			
			// get header
			if( !::HttpQueryInfo( u, HTTP_QUERY_RAW_HEADERS_CRLF, lpHeader, &l, NULL ) )
			{
				failed = TRUE;

				CHAR s[256];
				sprintf( s, "[ %s ] %s is down (HttpQueryInfo_Phase3 : get_header).", errorLevel, h->ipaddr );
				CLogger::Log( s );
				goto next_headerclean;
			}

			// search signature
			if( strstr( (CHAR*)lpHeader, NSCLU_SIGNATURE ) == NULL )
			{
				failed = TRUE;

				CHAR s[256];
				sprintf( s, "[ %s ] %s is down (HttpQueryInfo_Phase2 : invalid signature).", errorLevel, h->ipaddr );
				CLogger::Log( s );
				goto next_headerclean;
			}

			// search nsclu pv-count
			CHAR* cpv = strstr( (CHAR*)lpHeader, NSCLU_PVCOUNT_HEADER );
			if( cpv == NULL )
			{
				failed = TRUE;

				CHAR s[256];
				sprintf( s, "[ %s ] %s is down (doesn't respond nsclu counter value).", errorLevel, h->ipaddr );
				CLogger::Log( s );
				goto next_headerclean;
			}

			// * passed.
			// its alive.

			// cutout pv-count
			cpv += strlen( NSCLU_PVCOUNT_HEADER );
			CHAR* crlf = strstr( cpv, "\r\n" );
			*crlf = '\0';
			// convert to unsigned 64bit integer
			ULONGLONG pv = 0;
			pv = _atoi64( cpv );
			// assign weight
			{	// lock scope
				CCritSectionAutoLock l( h->lock );
				// assign
				pv = (ULONGLONG)ceil( (DOUBLE)pv * h->weight );
			}

			// [protection] check failStatus
			if( h->failStatus > 0 )
			{
				// avoid.

				// + add to sub-server list
				while( !subsvs.insert( map<ULONGLONG, CHAR*>::value_type( pv, h->ipaddr ) ).second )
					pv++;
				// + increment avail
				avail++;
				// 
				CHAR s[256];
				sprintf( s, "[ %s ] %s is supressed; fail-count: %d, weight(cur:%f, min:%f, max:%f)", errorLevel, h->ipaddr, h->failStatus, h->weight, h->weightMin, h->weightMax );
				CLogger::Log( s );
				//
				goto next_success;
			}

			// [protection] time based avoidance
			if(	h->useTimeAvoidance )
			{
				if(	(	h->avoidBeginInMinute <= h->avoidEndInMinute
					&&	h->avoidBeginInMinute <= nowInMinute && nowInMinute <= h->avoidEndInMinute
					)
				||	(	h->avoidBeginInMinute > h->avoidEndInMinute
					&&	(	(	h->avoidBeginInMinute <= nowInMinute && nowInMinute <= 24*60	)
						||	(	0 <= nowInMinute && nowInMinute <= h->avoidEndInMinute			)
						)
					)
				)
				{
					// avoid.

					// + add to sub-server list
					while( !subsvs.insert( map<ULONGLONG, CHAR*>::value_type( pv, h->ipaddr ) ).second )
						pv++;
					// + increment avail
					avail++;
					//
					goto next_success;
				}
			}

			// [protection] pv limitter
			if( h->pvLimit > 0
			&&	pv > h->pvLimit
			)
			{
				// check pv limitter note status
				if( !h->pvLimitNoted )
				{
					// note limit
					CHAR s[256];
					sprintf( s, "[ NOTE ] %s is supressed; pv limit reached: %d", h->ipaddr, pv );
					CLogger::Log( s );

					// set pv limitter note status
					h->pvLimitNoted = TRUE;
				}

				// + add to sub-server list
				while( !subsvs.insert( map<ULONGLONG, CHAR*>::value_type( pv, h->ipaddr ) ).second )
					pv++;
				// + increment avail
				avail++;
				//
				goto next_success;
			}

			// reset pv limitter note status
			h->pvLimitNoted = FALSE;
			// + add to available-server list
			while( !availsvs.insert( map<ULONGLONG, CHAR*>::value_type( pv, h->ipaddr ) ).second )
				pv++;
			// + increment avail
			avail++;

next_success:

next_headerclean:
			if( lpHeader != NULL )
			{
				delete [] lpHeader;
				lpHeader = NULL;
			}

next_inetclean:
			// close conn.
			if( u != NULL )
			{
				InternetCloseHandle( u );
				u = NULL;
			}

next_failaction:
			if( failed )
			{
				// [protection] change failStatus
				h->failStatus = min( 5, (h->failStatus) + 1 );	// do not use ++ increment.

				// lock scope
				{
					CCritSectionAutoLock l( h->lock );
					// assign weights
					h->weight = min( h->weight + h->weightGainRate, h->weightMax );
					// 
					h->weightLastReduced = t;
				}

				CHAR s[256];
				sprintf( s, "[ INFO ] %s fail-count: %d, weight(cur:%f, min:%f, max:%f)", h->hostname, h->failStatus, h->weight, h->weightMin, h->weightMax );
				CLogger::Log( s );
			}
			else
			{
				if( h->failStatus > 0 )
					(h->failStatus)--;
			}
		}

		// * zone control
		if( avail < 1 )
		{
			CLogger::Log( "[ WARN ] no server available." );
		}
		else
		{
			// init bind zone
			b.ClearHosts();

			// add host to bind zone-file
			UINT addedsvs = 0;
			// - log
			CHAR logreload[0x10000];
			sprintf( logreload, "[ INFO ] BIND zones reloaded. " );
			CHAR* plogr = logreload + strlen(logreload); 
			// - create map
			map<ULONGLONG, CHAR*>::iterator ib;
			map<ULONGLONG, CHAR*>::iterator ie;
			// 
			if( availsvs.size() > 0 )
			{
				for( ib = availsvs.begin(), ie = availsvs.end() ; ib != ie; ib++ )
				{
					// log
					sprintf( plogr, "%s_%d ", (*ib).second, (*ib).first );
					plogr += strlen( plogr );

					// map
					if( addedsvs < cfg.get_NSCLU_HOSTS_AT_LEAST() )
					{
						// add
						b.AddHost( (CHAR*) (*ib).second );
						//
						addedsvs++;
					}
				}
			}
			// 
			if( subsvs.size() > 0 )
			{
				for( ib = subsvs.begin(), ie = subsvs.end() ; ib != ie; ib++ )
				{
					// log
					sprintf( plogr, "%s_%d ", (*ib).second, (*ib).first );
					plogr += strlen( plogr );

					// map
					if( addedsvs < cfg.get_NSCLU_HOSTS_AT_LEAST() )
					{
						// add
						b.AddHost( (CHAR*) (*ib).second );
						//
						addedsvs++;
					}
				}
			}

			// named control
			if( b.IsChanged() )
			{
				//CLogger::Log( "[ NOTE ] zone-file changed." );

				// commit zone-file
				if(SUCCEEDED( b.WriteHosts() ))
				{
					// restart BIND
					if(SUCCEEDED( b.RNDCReload() ))
					{
						CLogger::Log( logreload );
					}
					else
					{
						CLogger::Log( "[ WARN ] failed to reload zones." );
					}
				}
				else
				{
					CLogger::Log( "[ WARN ] failed to write zone-file." );
				}
			}
		}

		// * sleep
		if( WaitForSingleObject( g_extevent, (DWORD)cfg.get_NSCLU_CHECK_INTERVAL() ) != WAIT_TIMEOUT )
		{
			CLogger::Log( "[ INFO ] external re-entry request." );

			// init bind zone
			b.ClearOldHosts();

			ResetEvent( g_extevent );
		}
	}

	// close wininet
	InternetCloseHandle( inet );
	
	// cleanup winsock
	::WSACleanup();

	//
	CloseHandle( g_extevent );
	g_extevent = INVALID_HANDLE_VALUE;

	return 0;
}
