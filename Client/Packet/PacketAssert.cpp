//--------------------------------------------------------------------------------
//
// Filename   : PacketAssert.cpp
//
// Renamed from Assert.cpp - see PacketAssert.h for why.
//
// Written By : Reiot
//
//--------------------------------------------------------------------------------

// include files
#include "Client_PCH.h"
#include "Types.h"
#include "PacketAssert.h"
#include "Exception.h"
#include "CrtCompat.h"
#include <time.h>

//--------------------------------------------------------------------------------
//
// __assert__
//
// There is no need to wrap this function in __BEGIN_TRY / __END_CATCH.
//
//--------------------------------------------------------------------------------
void __assert__ ( const char * file , uint line , const char * func , const char * expr )
{
	StringStream msg;
	
	msg << eos
		<< "Assertion Failed : " << file << " : " << line;

	if ( func )
		msg << " : " << func;

	time_t currentTime = time(0);
	
	msg << expr << " at " << Basic::TimeText(currentTime).c_str();
	
	ofstream ofile("assertion_failed.log",ios::app);
	ofile << msg.toString().c_str() << endl;
	ofile.close();

	throw AssertionError( msg.toString() );
}

//--------------------------------------------------------------------------------
//
// __assert__ - C++20 entry point
//
// The captured function name is deliberately not used: which spelling this
// platform wants is decided by the Assert macro and passed in func.
//
//--------------------------------------------------------------------------------
void __assert__ ( const char * func , const char * expr , const DiagnosticSite & site )
{
	__assert__( site.file , (uint)site.line , func , expr );
}
