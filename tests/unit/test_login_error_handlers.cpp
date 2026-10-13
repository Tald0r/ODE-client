#include "test_framework.h"
#include "gamemodel_world.h"
#include "packet_stream_access.h"
#include "ClientMode.h"
#include "LoginPresentationHost.h"
#include "MGameStringTable.h"
#include "Socket.h"
#include "SocketImpl.h"
#include "SocketInputStream.h"
#include "SocketOutputStream.h"
#include "Types/PlayerTypes.h"
#include "Lpackets/LCLoginError.h"
#include "Lpackets/LCRegisterPlayerError.h"
#include <iterator>
#include <string>
#include <vector>

// A refused login shows its error and sends the client back to the login;
// a refused registration shows the message for its error and does the same.
// A client quitting to be updated (g_bNeedUpdate) shows nothing, and keeps
// its mode on a refused login.
namespace {

std::vector<std::string> calls;

std::string N(long long value) { return std::to_string(value); }

LoginPresentation::Host LoginHost()
{
	LoginPresentation::Host host{};
	host.PopupErrorMessage = [](int errorID) { calls.push_back("error|" + N(errorID)); };
	host.ShowMessage = [](const char* message) { calls.push_back("message|" + std::string(message ? message : "(null)")); };
	return host;
}
const LoginPresentation::Host login = LoginHost();

struct World : GameModelWorld
{
	const LoginPresentation::Host* oldLogin = LoginPresentation::SetHost(&login);
	CLIENT_MODE oldNext = g_ModeNext;
	int oldNeedUpdate = g_bNeedUpdate;
	World()
	{
		InitGameStringTable();
		calls.clear();
		g_ModeNext = MODE_WAIT_LOGINOK;
		g_bNeedUpdate = 0;
	}
	~World()
	{
		g_bNeedUpdate = oldNeedUpdate;
		g_ModeNext = oldNext;
		LoginPresentation::SetHost(oldLogin);
	}
};

template<class PacketType>
void Decode(PacketType& packet, const PacketType& original)
{
	Socket out((EnsureSocketsInitialised(), new SocketImpl()));
	SocketOutputStream encoded(&out);
	original.write(encoded);
	const std::vector<unsigned char> bytes = SocketOutputStreamTestAccess::Bytes(encoded);
	Socket in((EnsureSocketsInitialised(), new SocketImpl()));
	SocketInputStream stream(&in, 1024);
	SocketInputStreamTestAccess::Preload(stream, bytes.data(), static_cast<unsigned int>(bytes.size()));
	packet.read(stream);
	CHECK(stream.isEmpty());
}

void ReceiveLoginError(BYTE error)
{
	LCLoginError original;
	original.setErrorID(error);
	LCLoginError packet;
	Decode(packet, original);
	LCLoginErrorHandler::execute(&packet, nullptr);
}

void ReceiveRegisterError(BYTE error)
{
	LCRegisterPlayerError original;
	original.setErrorID(error);
	LCRegisterPlayerError packet;
	Decode(packet, original);
	LCRegisterPlayerErrorHandler::execute(&packet, nullptr);
}

std::string Message(int stringID) { return "message|" + std::string((*g_pGameStringTable)[stringID].GetString()); }

} // namespace

TEST(LoginErrorHandlers, ARefusedLoginShowsItsErrorAndGoesBackToTheLogin)
{
	World world;
	ReceiveLoginError(ALREADY_CONNECTED);
	CHECK_EQ(static_cast<int>(MODE_LOGIN_WRONG), static_cast<int>(g_ModeNext));
	// Any byte is passed on whole, past the last named error too.
	ReceiveLoginError(200);
	CHECK(calls == std::vector<std::string>({"error|" + N(ALREADY_CONNECTED), "error|200"}));
}

TEST(LoginErrorHandlers, AClientQuittingToBeUpdatedKeepsItsModeOnARefusedLogin)
{
	World world;
	g_bNeedUpdate = 1;
	g_ModeNext = MODE_QUIT;
	ReceiveLoginError(ALREADY_CONNECTED);
	CHECK(calls.empty());
	CHECK_EQ(static_cast<int>(MODE_QUIT), static_cast<int>(g_ModeNext));
}

TEST(LoginErrorHandlers, ARefusedRegistrationShowsTheMessageForItsError)
{
	World world;
	const struct { BYTE error; int stringID; } rows[] = {
		{ALREADY_REGISTER_ID, STRING_ERROR_ALREADY_REGISTER_ID},
		{ALREADY_REGISTER_SSN, STRING_ERROR_ALREADY_REGISTER_SSN},
		{EMPTY_ID, STRING_ERROR_EMPTY_ID},
		{SMALL_ID_LENGTH, STRING_ERROR_SMALL_ID_LENGTH},
		{EMPTY_PASSWORD, STRING_ERROR_EMPTY_PASSWORD},
		{SMALL_PASSWORD_LENGTH, STRING_ERROR_SMALL_PASSWORD_LENGTH},
		{EMPTY_NAME, STRING_ERROR_EMPTY_NAME},
		{EMPTY_SSN, STRING_ERROR_EMPTY_SSN},
		// Any other error, named or not, shows the general one.
		{ALREADY_CONNECTED, STRING_ERROR_ETC_ERROR},
		{ETC_ERROR, STRING_ERROR_ETC_ERROR},
		{250, STRING_ERROR_ETC_ERROR},
	};
	for (const auto& row : rows)
	{
		calls.clear();
		g_ModeNext = MODE_WAIT_REGISTERPLAYEROK;
		ReceiveRegisterError(row.error);
		CHECK(calls == std::vector<std::string>{Message(row.stringID)});
		CHECK_EQ(static_cast<int>(MODE_LOGIN_WRONG), static_cast<int>(g_ModeNext));
	}
	// The messages are distinct, so a wrong mapping shows.
	for (size_t i = 0; i < std::size(rows); i++)
		for (size_t j = 0; j < std::size(rows); j++)
			if (rows[i].stringID != rows[j].stringID)
				CHECK(Message(rows[i].stringID) != Message(rows[j].stringID));
}

TEST(LoginErrorHandlers, AClientQuittingToBeUpdatedShowsNoRegistrationMessageButStillLeaves)
{
	World world;
	g_bNeedUpdate = 1;
	ReceiveRegisterError(EMPTY_ID);
	CHECK(calls.empty());
	CHECK_EQ(static_cast<int>(MODE_LOGIN_WRONG), static_cast<int>(g_ModeNext));
}

TEST(LoginErrorHandlers, MissingServicesSkipTheMessagesNotTheMode)
{
	World world;
	const LoginPresentation::Host empty{};
	for (const LoginPresentation::Host* host : {static_cast<const LoginPresentation::Host*>(nullptr), &empty})
	{
		LoginPresentation::SetHost(host);
		g_ModeNext = MODE_WAIT_LOGINOK;
		ReceiveLoginError(ALREADY_CONNECTED);
		CHECK_EQ(static_cast<int>(MODE_LOGIN_WRONG), static_cast<int>(g_ModeNext));
		g_ModeNext = MODE_WAIT_REGISTERPLAYEROK;
		ReceiveRegisterError(EMPTY_ID);
		CHECK_EQ(static_cast<int>(MODE_LOGIN_WRONG), static_cast<int>(g_ModeNext));
	}
	CHECK(calls.empty());
	CHECK(LoginPresentation::SetHost(&login) == &empty);
}
