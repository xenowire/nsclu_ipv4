#pragma once

#include <windows.h>
#include <stdio.h>
#include <time.h>

#include "generic.h"

class CLogger
{
private:
	static CCritSection*	m_lock;
public:
	static VOID				Log( CHAR* msg );
};
