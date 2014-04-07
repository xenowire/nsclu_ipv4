#pragma once

#include <windows.h>

#include <vector>
using namespace std;

#include "generic.h"
#include "logger.h"
#include "scm.h"

class CBIND
{
public:
	CBIND( HRESULT* hr );
	~CBIND();

	// BIND control
public:
	HRESULT	RestartBIND();
	HRESULT	RNDCReload();
private:
	HRESULT IsFileExists();

	// zone control
private:
	vector<CHAR*>		m_hosts;			// additional hosts of zone-file
public:
	HRESULT	ClearHosts();
	HRESULT	WriteHosts();
	HRESULT	AddHost( CHAR* ipaddr );
	BOOL	IsChanged();
	BOOL	IsExists( CHAR* ipaddr );
private:
	vector<CHAR*>		m_oldhosts;			// 
public:
	HRESULT				ClearOldHosts();
private:
	HRESULT				AddOldHosts( CHAR* ipaddr );
};

