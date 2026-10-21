//----------------------------------------------------------------------
// test_crwhisper.cpp
//----------------------------------------------------------------------
//
// CRWhisper, the whisper one client sends another over a direct
// connection. This client no longer sends one - the outbound peer side
// was compiled out upstream and is deleted (docs/reviews/RESTRUCTURING.md task
// 5.2, seventh and eighth slices) - but it still receives them, the
// class is pinned by tests/wire-layout.txt, and two defects found in it
// on the way are kept from returning here.
//
//----------------------------------------------------------------------

#include "test_framework.h"

#include "packet_stream_access.h"		// EnsureSocketsInitialised, the stream seams
#include "Rpackets/CRWhisper.h"
#include "Socket.h"
#include "SocketImpl.h"
#include "SocketOutputStream.h"

TEST(CRWhisperRace, TheThreePredicatesNameTheThreeRaces)
{
	// Found while reading the packet back in a manager test: isSlayer()
	// compared the race against RACE_VAMPIRE, so a vampire's whisper
	// answered both isVampire() and isSlayer() and a slayer's answered
	// neither. Nothing calls the predicates - CRWhisperHandler reads
	// getRace() - which is why it survived; it is fixed because a
	// predicate that lies is worse than one nobody calls yet.
	CRWhisper	whisper;

	whisper.setRace(RACE_SLAYER);
	CHECK_EQ(true, whisper.isSlayer());
	CHECK_EQ(false, whisper.isVampire());
	CHECK_EQ(false, whisper.isOusters());

	whisper.setRace(RACE_VAMPIRE);
	CHECK_EQ(false, whisper.isSlayer());
	CHECK_EQ(true, whisper.isVampire());
	CHECK_EQ(false, whisper.isOusters());

	whisper.setRace(RACE_OUSTERS);
	CHECK_EQ(false, whisper.isSlayer());
	CHECK_EQ(false, whisper.isVampire());
	CHECK_EQ(true, whisper.isOusters());
}

TEST(CRWhisperRace, TheWriterRefusesARaceThatIsNotOne)
{
	// write() checked every length but not the race, so a byte outside
	// 0..2 could reach the wire. The name checks fail closed; this makes
	// the race check match them.
	EnsureSocketsInitialised();
	Socket			socket(new SocketImpl());
	SocketOutputStream	out(&socket, 4096);

	CRWhisper	whisper;
	whisper.setName("Alice");
	whisper.setTargetName("bob");
	WHISPER_MESSAGE	m;
	m.msg = "hello";
	m.color = 1;
	whisper.addMessage(m);

	whisper.setRace(RACE_MAX);

	bool	refused = false;
	try {
		whisper.write(out);
	} catch (InvalidProtocolException&) {
		refused = true;
	}
	CHECK_EQ(true, refused);

	// A real race writes, and getPacketSize() agrees with the bytes.
	whisper.setRace(RACE_OUSTERS);
	whisper.write(out);
	CHECK_EQ((size_t)whisper.getPacketSize(), SocketOutputStreamTestAccess::Bytes(out).size());
}
