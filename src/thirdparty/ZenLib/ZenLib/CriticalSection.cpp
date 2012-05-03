// ZenLib::CriticalSection - CriticalSection functions
// Copyright (C) 2007-2011 MediaArea.net SARL, Info@MediaArea.net
//
// This software is provided 'as-is', without any express or implied
// warranty.  In no event will the authors be held liable for any damages
// arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would be
//    appreciated but is not required.
// 2. Altered source versions must be plainly marked as such, and must not be
//    misrepresented as being the original software.
// 3. This notice may not be removed or altered from any source distribution.
//
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

//---------------------------------------------------------------------------
#include "ZenLib/PreComp.h"
#ifdef __BORLANDC__
    #pragma hdrstop
#endif
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
#include "ZenLib/Conf_Internal.h"
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
#include "ZenLib/CriticalSection.h"
//---------------------------------------------------------------------------

//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// ZENLIB_USEWX
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
#ifdef ZENLIB_USEWX
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

//---------------------------------------------------------------------------
#include <wx/thread.h>
//---------------------------------------------------------------------------

namespace ZenLib
{

//***************************************************************************
// Constructor/Destructor
//***************************************************************************

//---------------------------------------------------------------------------
CriticalSection::CriticalSection()
{
    CritSect=new wxCriticalSection();
}

//---------------------------------------------------------------------------
CriticalSection::~CriticalSection()
{
    delete ((wxCriticalSection*)CritSect); //CritSect=NULL;
}

//***************************************************************************
// Enter/Leave
//***************************************************************************

//---------------------------------------------------------------------------
void CriticalSection::Enter()
{
    ((wxCriticalSection*)CritSect)->Enter();
}

//---------------------------------------------------------------------------
void CriticalSection::Leave()
{
    ((wxCriticalSection*)CritSect)->Leave();
}

} //Namespace

//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// WINDOWS
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
#else //ZENLIB_USEWX
#ifdef WINDOWS
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

//---------------------------------------------------------------------------
#undef __TEXT
#include <windows.h>
//---------------------------------------------------------------------------


namespace ZenLib
{

//***************************************************************************
// Constructor/Destructor
//***************************************************************************

//---------------------------------------------------------------------------
CriticalSection::CriticalSection()
{
    CritSect=new CRITICAL_SECTION;
    InitializeCriticalSection((CRITICAL_SECTION*)CritSect);
}

//---------------------------------------------------------------------------
CriticalSection::~CriticalSection()
{
    DeleteCriticalSection((CRITICAL_SECTION*)CritSect);
    delete ((CRITICAL_SECTION*)CritSect); //CritSect=NULL;
}

//***************************************************************************
// Enter/Leave
//***************************************************************************

//---------------------------------------------------------------------------
void CriticalSection::Enter()
{
    EnterCriticalSection((CRITICAL_SECTION*)CritSect);
}

//---------------------------------------------------------------------------
void CriticalSection::Leave()
{
    LeaveCriticalSection((CRITICAL_SECTION*)CritSect);
}

} //Namespace

//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// UNIX
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
#else //WINDOWS
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

//---------------------------------------------------------------------------
#include <pthread.h>
//---------------------------------------------------------------------------

namespace ZenLib
{

//***************************************************************************
// Constructor/Destructor
//***************************************************************************

//---------------------------------------------------------------------------
CriticalSection::CriticalSection()
{
    CritSect=new pthread_mutex_t;
    pthread_mutex_init((pthread_mutex_t*)CritSect, NULL);
}

//---------------------------------------------------------------------------
CriticalSection::~CriticalSection()
{
    pthread_mutex_destroy((pthread_mutex_t*)CritSect);
    delete (pthread_mutex_t*)CritSect;
}

//***************************************************************************
// Enter/Leave
//***************************************************************************

//---------------------------------------------------------------------------
void CriticalSection::Enter()
{
    pthread_mutex_lock((pthread_mutex_t*)CritSect);
}

//---------------------------------------------------------------------------
void CriticalSection::Leave()
{
    pthread_mutex_unlock((pthread_mutex_t*)CritSect);
}

} //Namespace

//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
#endif //WINDOWS
#endif //ZENLIB_USEWX
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

