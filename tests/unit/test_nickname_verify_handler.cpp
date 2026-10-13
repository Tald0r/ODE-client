#include "test_framework.h"
#include "gamemodel_world.h"
#include "packet_stream_access.h"
#include "CommunicationPresentationHost.h"
#include "MGameStringTable.h"
#include "TempInformation.h"
#include "VsUiPresentationHost.h"
#include "Socket.h"
#include "SocketImpl.h"
#include "SocketInputStream.h"
#include "SocketOutputStream.h"
#include "Gpackets/GCNicknameVerify.h"
#include <string>
#include <vector>

// The answer to a nickname change. An accepted custom change shows the
// new nickname in the naming window (for a nickname item, 22 or the timed
// 25), closes the window and says it changed; the other answers only say
// why. Every answer ends the pending request.
namespace {

std::vector<std::string> calls;

std::string N(long long value) { return std::to_string(value); }

const CommunicationPresentation::Host communication = []() {
	CommunicationPresentation::Host host{};
	host.PopupMessage = [](int gameStringID) { calls.push_back("popup|" + N(gameStringID)); };
	return host;
}();

const VsUiPresentation::Host ui = []() {
	VsUiPresentation::Host host{};
	host.ChangeCustomNaming = [](int id, const char* name) { calls.push_back("naming|" + N(id) + "|" + name); };
	host.CloseNamingChange = []() { calls.push_back("close"); };
	return host;
}();

struct World : GameModelWorld
{
	TempInformation temp;
	TempInformation* oldTemp = g_pTempInformation;
	const CommunicationPresentation::Host* oldCommunication = CommunicationPresentation::SetHost(&communication);
	const VsUiPresentation::Host* oldUi = VsUiPresentation::SetHost(&ui);
	World()
	{
		g_pTempInformation = &temp;
		calls.clear();
	}
	~World()
	{
		VsUiPresentation::SetHost(oldUi);
		CommunicationPresentation::SetHost(oldCommunication);
		g_pTempInformation = oldTemp;
	}
	void Pending(intptr_t itemType)
	{
		temp.SetMode(TempInformation::MODE_NICKNAME_CHANGE_CUSTOM);
		temp.Value2 = itemType;
		temp.StrValue1 = "Lantern";
	}
};

void Receive(BYTE code)
{
	GCNicknameVerify original;
	original.setCode(code);
	original.setParameter(0);
	Socket out((EnsureSocketsInitialised(), new SocketImpl()));
	SocketOutputStream encoded(&out);
	original.write(encoded);
	const std::vector<unsigned char> bytes = SocketOutputStreamTestAccess::Bytes(encoded);
	Socket in((EnsureSocketsInitialised(), new SocketImpl()));
	SocketInputStream stream(&in, 1024);
	SocketInputStreamTestAccess::Preload(stream, bytes.data(), static_cast<unsigned int>(bytes.size()));
	GCNicknameVerify packet;
	packet.read(stream);
	CHECK(stream.isEmpty());
	GCNicknameVerifyHandler::execute(&packet, nullptr);
}

std::string Popup(int id) { return "popup|" + N(id); }

} // namespace

TEST(NicknameVerifyHandler, AnAcceptedNicknameItemShowsTheNameClosesTheWindowAndSaysSo)
{
	for (intptr_t itemType : {22, 25})
	{
		World world;
		world.Pending(itemType);
		Receive(NICKNAME_MODIFY_OK);
		CHECK(calls == std::vector<std::string>({"naming|0|Lantern", "close", Popup(UI_STRING_MESSAGE_NICKNAME_CHANGE_OK)}));
		CHECK_EQ(static_cast<int>(TempInformation::MODE_NULL), static_cast<int>(world.temp.GetMode()));
	}
}

TEST(NicknameVerifyHandler, AnAcceptedChangeOfAnotherKindOnlyClosesTheWindowAndSaysSo)
{
	for (intptr_t itemType : {0, 23, 24})
	{
		World world;
		world.Pending(itemType);
		Receive(NICKNAME_MODIFY_OK);
		CHECK(calls == std::vector<std::string>({"close", Popup(UI_STRING_MESSAGE_NICKNAME_CHANGE_OK)}));
	}
}

TEST(NicknameVerifyHandler, AnAcceptedChangeNobodyAskedForOnlyEndsTheRequest)
{
	World world;
	world.temp.SetMode(TempInformation::MODE_PARTY_REQUEST);
	world.temp.Value2 = 22;
	Receive(NICKNAME_MODIFY_OK);
	CHECK(calls.empty());
	CHECK_EQ(static_cast<int>(TempInformation::MODE_NULL), static_cast<int>(world.temp.GetMode()));
}

TEST(NicknameVerifyHandler, TheOtherAnswersSayWhyAndEndTheRequest)
{
	World world;
	const struct { BYTE code; int stringID; } rows[] = {
		{NICKNAME_MODIFY_FAIL_NO_ITEM, STRING_MESSAGE_CANNOT_BUY_NO_ITEM},
		{NICKNAME_SELECT_FAIL_NO_SUCH, UI_STRING_MESSAGE_SMS_FAIL_NO_SUCH_EID},
		{NICKNAME_SELECT_OK, UI_STRING_MESSAGE_NICKNAME_CHANGE_OK},
		{NICKNAME_SELECT_FAIL_FORCED, UI_STRING_MESSAGE_NICKNAME_SELECT_FAIL_FORCED},
	};
	for (const auto& row : rows)
	{
		calls.clear();
		world.Pending(22);
		Receive(row.code);
		CHECK(calls == std::vector<std::string>{Popup(row.stringID)});
		CHECK_EQ(static_cast<int>(TempInformation::MODE_NULL), static_cast<int>(world.temp.GetMode()));
	}
}

TEST(NicknameVerifyHandler, MissingServicesStillEndTheRequest)
{
	World world;
	CommunicationPresentation::SetHost(nullptr);
	VsUiPresentation::SetHost(nullptr);
	world.Pending(22);
	Receive(NICKNAME_MODIFY_OK);
	CHECK(calls.empty());
	CHECK_EQ(static_cast<int>(TempInformation::MODE_NULL), static_cast<int>(world.temp.GetMode()));
}
