#pragma once

#include <windows.h>

#include "logger.h"

#include ".\lib\xenowire\CCritSection.h"
using namespace xenowire;

class CSCM
{
public:
	CSCM( HRESULT* hr );
	~CSCM();

private:
	SC_HANDLE		m_scm;
	CCritSection	m_csscm;

	//
public:
	HRESULT Start( CHAR* svcname );
	HRESULT Stop( CHAR* svcname );
};


