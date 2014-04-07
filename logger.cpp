#include "logger.h"

// static member variables
CCritSection	g_clogger_m_lock;
CCritSection*	CLogger::m_lock = &g_clogger_m_lock;

// 
VOID
CLogger::Log( CHAR* msg )
{
	CCritSectionAutoLock l( CLogger::m_lock );

	// open/create file
	HANDLE f = CreateFile( LOGGER_FILENAME
		,	GENERIC_WRITE
		,	FILE_SHARE_READ | FILE_SHARE_WRITE
		,	NULL
		,	OPEN_ALWAYS
		,	FILE_ATTRIBUTE_NORMAL
		,	NULL
		);
	if( f == INVALID_HANDLE_VALUE )
		return;
	
	// goto eof
	SetFilePointer( f, 0, NULL, FILE_END );

	// get time
	tm* t;
	{
		time_t tt;
		time( &tt );
		t = localtime( &tt );
	}

	// write msg
	{
		CHAR* b = new CHAR[ strlen( msg ) + 200 + 1 ]; // 0000/00/00 00:00:00 + msg + \r\n + \0
		sprintf( b, "%04d/%02d/%02d %02d:%02d:%02d %s\r\n"
			, t->tm_year +1900, t->tm_mon +1, t->tm_mday
			, t->tm_hour, t->tm_min, t->tm_sec
			, msg
			);

#ifdef _DEBUG
		OutputDebugString( b );
#endif

		DWORD dw;
		WriteFile( f, b, (DWORD)strlen(b), &dw, NULL );

		delete [] b;
	}

	// close
	if( f != INVALID_HANDLE_VALUE )
		CloseHandle( f );
}
