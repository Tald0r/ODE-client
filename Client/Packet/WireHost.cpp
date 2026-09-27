//----------------------------------------------------------------------
// WireHost.cpp
//----------------------------------------------------------------------
#include "Client_PCH.h"
#include "WireHost.h"
#include "Exception.h"
#include "Player.h"
#include "Cpackets/CGSay.h"
#include "DebugLog.h"		// DEBUG_ADD_FORMAT, for the __DEBUG_OUTPUT__ block below

#include <stdio.h>
#include <string.h>

const WireHost *	Wire::s_pHost = NULL;

//----------------------------------------------------------------------
// The three tuning values, and the defaults ClientConfig's constructor
// sets for them (Client/ClientConfig.cpp).
//----------------------------------------------------------------------
int
Wire::MaxProcessPacket ()
{
	if (s_pHost==NULL || s_pHost->MaxProcessPacket==NULL)
	{
		return WIRE_DEFAULT_MAX_PROCESS_PACKET;
	}

	return s_pHost->MaxProcessPacket();
}

int
Wire::MaxRequestService ()
{
	if (s_pHost==NULL || s_pHost->MaxRequestService==NULL)
	{
		return WIRE_DEFAULT_MAX_REQUEST_SERVICE;
	}

	return s_pHost->MaxRequestService();
}

uint
Wire::ClientCommunicationUDPPort ()
{
	if (s_pHost==NULL || s_pHost->ClientCommunicationUDPPort==NULL)
	{
		return WIRE_DEFAULT_UDP_PORT;
	}

	return s_pHost->ClientCommunicationUDPPort();
}

Player *
Wire::BugReportTarget ()
{
	if (s_pHost==NULL || s_pHost->BugReportTarget==NULL)
	{
		return NULL;
	}

	return s_pHost->BugReportTarget();
}

//----------------------------------------------------------------------
// The encrypt-seed inputs.
//----------------------------------------------------------------------
// Zone 0 and server 0 with no host, which is what the seed would be
// built from before the player is in a zone. There is no better answer
// to invent: a binary with no host has no zone and no account, and the
// one caller runs only after MoveZone/LoadZone (see WireEncryptSeed).
//----------------------------------------------------------------------
ZoneID_t
Wire::EncryptZoneID ()
{
	if (s_pHost==NULL || s_pHost->EncryptZoneID==NULL)
	{
		return 0;
	}

	return s_pHost->EncryptZoneID();
}

int
Wire::EncryptServerID ()
{
	if (s_pHost==NULL || s_pHost->EncryptServerID==NULL)
	{
		return 0;
	}

	return s_pHost->EncryptServerID();
}

bool
Wire::EncryptUsesEnglishSeed ()
{
	if (s_pHost==NULL || s_pHost->EncryptUsesEnglishSeed==NULL)
	{
		return false;
	}

	return s_pHost->EncryptUsesEnglishSeed();
}

//----------------------------------------------------------------------
// The request-service family's seams.
//----------------------------------------------------------------------
// The epoch with no host, and no file transfer registered - the
// three calls answer false, which is what a caller asking whether it
// still has one needs to hear so that it cleans up instead of waiting
// on a manager that is not there.
//----------------------------------------------------------------------
MonotonicClock::TimePoint
Wire::CurrentTime ()
{
	if (s_pHost==NULL || s_pHost->CurrentTime==NULL)
	{
		return MonotonicClock::TimePoint();
	}

	return s_pHost->CurrentTime();
}





bool
Wire::SendOtherRequest ( const std::string & name , RequestServerPlayer * pPlayer )
{
	if (s_pHost==NULL || s_pHost->SendOtherRequest==NULL)
	{
		return false;
	}

	return s_pHost->SendOtherRequest(name, pPlayer);
}

bool
Wire::HasOtherRequest ( const std::string & name )
{
	if (s_pHost==NULL || s_pHost->HasOtherRequest==NULL)
	{
		return false;
	}

	return s_pHost->HasOtherRequest(name);
}

bool
Wire::RemoveOtherRequest ( const std::string & name )
{
	if (s_pHost==NULL || s_pHost->RemoveOtherRequest==NULL)
	{
		return false;
	}

	return s_pHost->RemoveOtherRequest(name);
}

//----------------------------------------------------------------------
// The stream cipher's seed.
//----------------------------------------------------------------------
// Preserved from ClientPlayer::setEncryptCode() unchanged, including
// the parentheses, so the two formulas can be compared with the
// server's without reading past a rewrite. What is gone is the
// duplication: the Netmarble, Chinese and default branches were the
// same expression written three times.
//----------------------------------------------------------------------
uchar
WireEncryptSeed ( ZoneID_t zoneID , int serverID , bool bEnglishSeed ) noexcept
{
	if (bEnglishSeed)
	{
		return (uchar)( ( ( ( zoneID ) >> 8 ) ^ ( zoneID ) ) ^ ( ( ( serverID ) + 1 ) * 51 ) );
	}

	return (uchar)( ( ( ( zoneID ) >> 8 ) ^ ( zoneID ) ) ^ ( ( ( serverID ) + 1 ) << 4 ) );
}

//----------------------------------------------------------------------
// Send Bug Report
//----------------------------------------------------------------------
// Moved here verbatim from Client/PacketFunction.cpp, with the
// executable's own connection replaced by the host's target. The
// bound on the format buffer is the one an earlier hardening pass
// left in place.
//
// (The global that connection lives in is deliberately not named
// here: R4 greps every line of a library source for g_p* and does not
// skip comments, so writing it down would read as a seam.)
//----------------------------------------------------------------------

namespace {

// The chat command the server dispatches a report on, and what a CGSay
// leaves for the report once the prefix has had its bytes.
const char	BUG_REPORT_PREFIX[]	= "*bug_report ";

const int	BUG_REPORT_PREFIX_LEN	= (int)sizeof(BUG_REPORT_PREFIX) - 1;

const int	BUG_REPORT_TEXT_MAX	= (int)CGSay::MAX_MESSAGE_SIZE - BUG_REPORT_PREFIX_LEN;

static_assert(BUG_REPORT_TEXT_MAX > 0, "the bug report prefix does not fit in a CGSay message");

} // namespace

namespace {

//----------------------------------------------------------------------
// The report's text: an optional site prefix, then the caller's format,
// into one bounded buffer, cut where the packet ends.
//----------------------------------------------------------------------
void
SendBugReportV(const char* prefix, const char* bug, const SafeFormat::Arg* args, size_t count)
{
	char Buffer[256];
	int at = 0;

	if( prefix != NULL )
	{
		at = snprintf(Buffer, sizeof(Buffer), "%s", prefix);

		if (at < 0)
			return;

		if (at >= (int)sizeof(Buffer))
			at = (int)sizeof(Buffer) - 1;
	}

	SafeFormat::FormatV(Buffer + at, sizeof(Buffer) - at, bug, args, count);

	// And the cut has to land inside what was formatted.
	static_assert(BUG_REPORT_TEXT_MAX < (int)sizeof(Buffer), "the cut is outside the format buffer");

#ifdef __DEBUG_OUTPUT__
	DEBUG_ADD_FORMAT("[BUG_REPORT] %s",Buffer);
#endif

	int len = static_cast<int>(strlen(Buffer));

	// The rule is on the caller's text, prefix or none.
	if( len - at <= 1 )
		return;

	// Cut where the packet ends, not short of it.
	if( len > BUG_REPORT_TEXT_MAX )
		Buffer[BUG_REPORT_TEXT_MAX] = '\0';

	std::string message;

	message = BUG_REPORT_PREFIX;
	message += Buffer;

	CGSay _CGSay;

	_CGSay.setMessage( message );
	_CGSay.setColor( 0 );

	Player * pTarget = Wire::BugReportTarget();

	if( pTarget != NULL )
		pTarget->sendPacket( &_CGSay );
}

} // namespace

void SendBugReportArgs(const DiagnosticSite* site, const char* bug,
	const SafeFormat::Arg* args, size_t count)
{
	if (bug == NULL) return;
	if (site == NULL) {
		SendBugReportV(NULL, bug, args, count);
		return;
	}
	char prefix[256];
	snprintf(prefix, sizeof(prefix), "[%s,%d] ", site->file != NULL ? site->file : "", site->line);
	SendBugReportV(prefix, bug, args, count);
}
