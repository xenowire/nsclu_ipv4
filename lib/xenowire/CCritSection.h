#pragma once

#include <windows.h>

namespace xenowire
{
	class CCritSection
	{
	private:
		CRITICAL_SECTION	m_cs;
	public:
		CCritSection()
		{
			InitializeCriticalSection( &m_cs );
		}
		~CCritSection()
		{
			DeleteCriticalSection( &m_cs );
		}
		void enter()
		{
			EnterCriticalSection( &m_cs );
		}
		void leave()
		{
			LeaveCriticalSection( &m_cs );
		}
	};

	class CCritSectionAutoLock
	{
	private:
		CCritSection* m_cs;
	public:
		CCritSectionAutoLock( CCritSection* cs )
		:	m_cs( cs )
		{
			if( m_cs == NULL )
				return;

			m_cs->enter();
		}
		~CCritSectionAutoLock()
		{
			if( m_cs == NULL )
				return;

			m_cs->leave();
		}
	};
}
