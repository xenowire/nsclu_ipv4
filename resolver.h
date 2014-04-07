#pragma once

#include <winsock.h>
#include <windows.h>

class CResolver
{
public:
	static HRESULT	ResolveHost( CHAR** addr, const CHAR* name );
};
