#include "resolver.h"

HRESULT
CResolver::ResolveHost( CHAR** addr, const CHAR* name )
{
#ifdef _DEBUG
	if(	addr == NULL
	||	name == NULL
	)
		return E_POINTER;
#endif

	// lookup
	LPHOSTENT he;

	he = ::gethostbyname( name );
	if( he == NULL 
	||	he->h_addrtype != AF_INET
	||	he->h_length != 4
	)
		return E_FAIL;

	if( he->h_addr_list[ 0 ] == NULL )
		return E_FAIL;

	in_addr* pinaddr = (LPIN_ADDR)he->h_addr_list[ 0 ];
	if( pinaddr == NULL )
		return E_FAIL;

	// convert
	CHAR* cipaddr = inet_ntoa( *pinaddr );
	if( cipaddr == NULL )
		return E_FAIL;

	// alloc
	*addr = new CHAR[ strlen(cipaddr) + 1 ];
	if( *addr == NULL )
		return E_OUTOFMEMORY;

	// store
	strcpy( *addr, cipaddr );

	return S_OK;
}