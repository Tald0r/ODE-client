#include "test_framework.h"
#include "VS_UI_Base.h"
#include "VS_UI_GameCommon.h"
#include "VS_UI_GlobalResource.h"
#include "VS_UI_Description.h"
#include "VS_UI_mouse_pointer.h"
#include "MGameStringTable.h"
#include "MStringArray.h"
#include "MZoneTable.h"
#include "MParty.h"
#include "UserInformation.h"
#include "RaceType.h"
#include "pi_core.h"
#include "u_window.h"
#include "timer2.h"
#include "Fl2.h"
#include "Ci.h"
#include "CSpriteSurface.h"
#include "SpriteLibBackendSDL.h"
#include "SpriteLib/SpriteLibBackend.h"

#include <cstdio>
#include <string>
#include <vector>

// Server values the UI windows use as an index or a zone id, past the
// packet layer that packet_indices reads: a guild member's rank
// (GCShowGuildMemberInfo, GCGuildMemberList), a blood bible's zone
// (GCBloodBibleStatus), a party member's zone (GCPartyPosition, and
// RCPositionInfo from other clients) and a vampire portal's target zone
// (GCAddVampirePortal). None is bounded on the way in; each window answers
// for a value it has no entry for.
namespace {

const int kKnownZone = 21;
const int kUnknownZone = 4321;

// The game's strings, made once and kept to the end: these windows cache
// pointers into the table in function statics the first time they run.
MStringArray& Strings()
{
	static MStringArray table;
	static const bool filled = [] {
		MStringArray* old = g_pGameStringTable;
		g_pGameStringTable = &table;
		InitGameStringTable();
		g_pGameStringTable = old;
		return true;
	}();
	(void)filled;
	return table;
}

void Received(DWORD, intptr_t, intptr_t, void*) {}
void NoTimer() {}

// What a window needs to be built, drawn and hovered without the UI's
// Init: the base (its back surface and fonts), the process runner and
// window managers it registers with, the global resource its buttons
// measure, the game's strings, the user's record and a zone table that
// knows one zone.
struct Scene
{
	Base* oldBase = gpC_base;
	CI* oldIme = gC_ci;
	RECT oldRect = g_GameRect;
	PI_ProcessRunner* oldRunner = gpC_process_runner;
	WindowManager* oldWindows = gpC_window_manager;
	C_VS_UI_WINDOW_MANAGER* oldUiWindows = gpC_vs_ui_window_manager;
	C_GLOBAL_RESOURCE* oldResource = gpC_global_resource;
	MStringArray* oldStrings = g_pGameStringTable;
	CZoneTable* oldZones = g_pZoneTable;
	UserInformation* oldUser = g_pUserInformation;
	CI_KOREAN ime;
	CSpriteSurface back;
	Base base;
	timer_id_t blink = INVALID_TID;
	PI_ProcessRunner runner;
	WindowManager windows;
	C_VS_UI_WINDOW_MANAGER uiWindows;
	C_GLOBAL_RESOURCE resource;
	CZoneTable zones;
	UserInformation user;
	Scene()
	{
		spritectl_init();
		g_GameRect = {0, 0, 400, 300};
		back.Init(400, 300);
		gC_ci = &ime;
		// Base::Init starts the IME's cursor blinker, the timer after this
		// one; it is deleted again when the test ends.
		timer_id_t probe = gC_timer2.Add(1000, NoTimer);
		blink = probe + 1;
		gC_timer2.Delete(probe);
		base.Init(&back, Received);
		gpC_base = &base;
		gpC_process_runner = &runner;
		gpC_window_manager = &windows;
		gpC_vs_ui_window_manager = &uiWindows;
		gpC_global_resource = &resource;
		g_pGameStringTable = &Strings();
		ZONETABLE_INFO* pZone = new ZONETABLE_INFO;
		pZone->ID = kKnownZone;
		pZone->Name = "Known Zone";
		zones.Add(pZone);
		g_pZoneTable = &zones;
		g_pUserInformation = &user;
	}
	~Scene()
	{
		g_pUserInformation = oldUser;
		g_pZoneTable = oldZones;
		g_pGameStringTable = oldStrings;
		gpC_global_resource = oldResource;
		gpC_vs_ui_window_manager = oldUiWindows;
		gpC_window_manager = oldWindows;
		gpC_process_runner = oldRunner;
		const bool blinkerGone = gC_timer2.Delete(blink);
		CHECK(blinkerGone);
		g_SetFL2Surface(nullptr);
		gpC_base = oldBase;
		gC_ci = oldIme;
		g_GameRect = oldRect;
	}
	// The back surface's pixels.
	std::vector<WORD> Pixels()
	{
		std::vector<WORD> pixels;
		spritectl_surface_info_t info{};
		if (spritectl_lock_surface(back.GetBackendSurface(), &info) != 0) return pixels;
		for (int y = 0; y < info.height; ++y)
		{
			const WORD* row = reinterpret_cast<const WORD*>(static_cast<const unsigned char*>(info.pixels) + y * info.pitch);
			pixels.insert(pixels.end(), row, row + info.width);
		}
		spritectl_unlock_surface(back.GetBackendSurface());
		return pixels;
	}
};

// What a hover hands the descriptor manager: a unit of the test's own
// stands in for the UI's (g_InstallDescriptor's) for each id it watches.
id_t describedId = 0;
void* described = NULL;

template <id_t ID>
void Describe(void (*)(Rect, void*, long, long), int, int, void* pVoid, long, long)
{
	describedId = ID;
	described = pVoid;
}
void NoShow(Rect, void*, long, long) {}

struct Watch
{
	id_t id;
	DESCRIBED_UNIT* installed = Find(id);
	template <id_t ID>
	static Watch Of() { return Watch(ID, Describe<ID>); }
	Watch(id_t which, void (*calculator)(void (*)(Rect, void*, long, long), int, int, void*, long, long)) : id(which)
	{
		if (installed != NULL) g_descriptor_manager.Delete(installed);
		g_descriptor_manager.AddDescribedUnit(id, calculator, NoShow);
		g_descriptor_manager.Unset();
		describedId = 0;
		described = NULL;
	}
	Watch(const Watch&) = delete;
	~Watch()
	{
		DESCRIBED_UNIT* mine = Find(id);
		g_descriptor_manager.Delete(mine);
		delete mine;
		if (installed != NULL) g_descriptor_manager.Add(installed);
	}
	static DESCRIBED_UNIT* Find(id_t which)
	{
		DESCRIBED_UNIT* unit = NULL;
		for (int i = 0; i < g_descriptor_manager.Size(); ++i)
			if (g_descriptor_manager.Data(i, unit) && unit->id == which) return unit;
		return NULL;
	}
};

std::string Text(const char* text) { return text != NULL ? text : "(null)"; }

bool Same(const std::string& expected, const std::string& actual)
{
	if (expected == actual) return true;
	std::printf("      got \"%s\", wanted \"%s\"\n", actual.c_str(), expected.c_str());
	return false;
}

// The member list keeps its members, and the mouse pointer its text, to
// itself; an explicit instantiation may name a private member, so these
// read them.
template <class Tag, typename Tag::type Member>
struct Expose
{
	friend typename Tag::type Get(Tag) { return Member; }
};
using MemberList = std::vector<C_VS_UI_TEAM_MEMBER_LIST::TEAM_MEMBER_LIST>;
struct MembersOf { using type = MemberList C_VS_UI_TEAM_MEMBER_LIST::*; friend type Get(MembersOf); };
template struct Expose<MembersOf, &C_VS_UI_TEAM_MEMBER_LIST::m_v_member_list>;
struct TextOf { using type = std::string C_VS_UI_MOUSE_POINTER::*; friend type Get(TextOf); };
template struct Expose<TextOf, &C_VS_UI_MOUSE_POINTER::m_string>;

} // namespace

//----------------------------------------------------------------------
// C_VS_UI_TEAM_MEMBER_INFO: the grade line names the member's rank from
// four grade names. A rank past them is shown as the member list shows
// it, with no name: the line keeps its label.
//----------------------------------------------------------------------
namespace {

std::vector<WORD> MemberInfoDrawn(Scene& scene, BYTE grade, const char* gradeLine)
{
	Strings().Set(UI_STRING_MESSAGE_TEAM_INFO_GRADE, gradeLine);
	C_VS_UI_TEAM_MEMBER_INFO::MEMBER_INFO info;
	info.guild_id = 1;
	info.NAME = "Someone";
	info.GRADE = grade;
	info.INTRODUCTION = "";
	scene.back.FillSurface(0xFFFF);	// the window writes black
	{
		C_VS_UI_TEAM_MEMBER_INFO window(&info);
		window.Show();
	}
	return scene.Pixels();
}

} // namespace

TEST(GuildMemberInfo, ARankPastTheFourGradesShowsTheGradeLabelAlone)
{
	Scene scene;
	const std::vector<WORD> labelAlone = MemberInfoDrawn(scene, 1, "Grade : ");
	// A rank the window knows draws its name after the label.
	CHECK(MemberInfoDrawn(scene, 1, "Grade : %s") != labelAlone);
	CHECK(MemberInfoDrawn(scene, 4, "Grade : %s") == labelAlone);
	CHECK(MemberInfoDrawn(scene, 255, "Grade : %s") == labelAlone);
	Strings().Set(UI_STRING_MESSAGE_TEAM_INFO_GRADE, "Grade : %s");
}

//----------------------------------------------------------------------
// C_VS_UI_TEAM_MEMBER_LIST::AddMemberList: members are kept master, sub
// master, member, waiting, by a five-entry priority table. A rank past
// the table sorts after every rank in it, in the order it came.
//----------------------------------------------------------------------
namespace {

C_VS_UI_TEAM_MEMBER_LIST::TEAM_MEMBER_LIST Member(const char* name, int grade)
{
	C_VS_UI_TEAM_MEMBER_LIST::TEAM_MEMBER_LIST member;
	member.MEMBER_NAME = name;
	member.member_grade = grade;
	member.bLogOn = false;
	return member;
}

std::string Names(C_VS_UI_TEAM_MEMBER_LIST& list)
{
	std::string names;
	for (const auto& member : list.*Get(MembersOf())) names += member.MEMBER_NAME + ";";
	return names;
}

} // namespace

TEST(GuildMemberList, ARankPastTheSortTableSortsLast)
{
	Scene scene;
	C_VS_UI_TEAM_MEMBER_LIST list;
	list.AddMemberList(Member("first unknown", 200), 0);
	list.AddMemberList(Member("member", 0), 0);
	list.AddMemberList(Member("master", 1), 0);
	list.AddMemberList(Member("waiting", 3), 0);
	list.AddMemberList(Member("last unknown", 255), 0);
	list.AddMemberList(Member("sub master", 2), 0);
	list.AddMemberList(Member("four", 4), 0);
	list.AddMemberList(Member("five", 5), 0);
	CHECK(Same("master;sub master;member;waiting;four;first unknown;last unknown;five;", Names(list)));
}

//----------------------------------------------------------------------
// C_VS_UI_BLOOD_BIBLE_STATUS::MouseControl: hovering a bible's row
// describes where it is. A zone the table does not know is described as
// a bible with no place, as one with no zone is.
//----------------------------------------------------------------------
TEST(BloodBibleStatus, AZoneTheTableLacksIsDescribedAsNoPlace)
{
	Scene scene;
	Watch strings = Watch::Of<DID_STRINGS>();
	Watch bible = Watch::Of<DID_BLOOD_BIBLE_STATUS>();
	C_VS_UI_BLOOD_BIBLE_STATUS window;
	C_VS_UI_BLOOD_BIBLE_STATUS::BLOOD_BIBLE_STATUS status;
	status.zone_id = kKnownZone;
	status.x = 10;
	status.y = 20;
	status.shrine_race = RACE_SLAYER;
	status.status = C_VS_UI_BLOOD_BIBLE_STATUS::BLOOD_BIBLE_HAS_SLAYER;
	status.name = "Holder";
	window.SetBloodBible(0, status);
	status.zone_id = kUnknownZone;
	window.SetBloodBible(1, status);

	// The rows start 20 pixels down, 20 apart.
	window.MouseControl(M_MOVING, window.x + 20, window.y + 25);
	CHECK_EQ(DID_BLOOD_BIBLE_STATUS, describedId);
	if (describedId == DID_BLOOD_BIBLE_STATUS)
		CHECK(Same("Known Zone (10,20)", Text(static_cast<S_BLOOD_BIBLE_DESCRIPTION*>(described)->sz_place)));

	g_descriptor_manager.Unset();
	describedId = 0;
	window.MouseControl(M_MOVING, window.x + 20, window.y + 45);
	CHECK_EQ(DID_STRINGS, describedId);
	if (describedId == DID_STRINGS)
		CHECK(Same(GetGameString(UI_STRING_MESSAGE_BLOOD_BIBLE_STATUS_NONE), Text(static_cast<char**>(described)[1])));
}

//----------------------------------------------------------------------
// C_VS_UI_PARTY_MANAGER::MouseControl: hovering a member's face names
// the member's zone and position. A zone the table does not know has
// nothing to name, and nothing is described.
//----------------------------------------------------------------------
TEST(PartyManager, AMemberInAZoneTheTableLacksHasNoFaceDescription)
{
	Scene scene;
	Watch help = Watch::Of<DID_HELP>();
	MParty* oldParty = g_pParty;
	MParty party;
	g_pParty = &party;
	PARTY_INFO* pNear = new PARTY_INFO;
	pNear->Name = "Near";
	pNear->zoneID = kKnownZone;
	pNear->zoneX = 3;
	pNear->zoneY = 4;
	party.AddMember(pNear);
	PARTY_INFO* pLost = new PARTY_INFO;
	pLost->Name = "Lost";
	pLost->zoneID = kUnknownZone;
	party.AddMember(pLost);
	{
		C_VS_UI_PARTY_MANAGER window;
		window.Start();
		// A member's row is 41 pixels under the 18-pixel title, the first
		// member's the second; the face is 9 across and 1 down in it (the
		// face pack does not load here, so it is 0 by 0).
		window.MouseControl(M_MOVING, window.x + 9, window.y + 18 + 41 + 1);
		CHECK_EQ(DID_HELP, describedId);
		if (describedId == DID_HELP)
			CHECK(Same("Known Zone", Text(static_cast<S_DEFAULT_HELP_STRING*>(described)->sz_main_str)));

		g_descriptor_manager.Unset();
		describedId = 0;
		window.MouseControl(M_MOVING, window.x + 9, window.y + 18 + 82 + 1);
		CHECK_EQ(id_t(0), describedId);
		window.Finish();
	}
	g_pParty = oldParty;
}

//----------------------------------------------------------------------
// C_VS_UI_MOUSE_POINTER::SetCursorPortal: a vampire portal's cursor
// names the zone it leads to. A zone the table does not know leaves the
// text empty, and the cursor draws no text box for an empty text.
//----------------------------------------------------------------------
TEST(PortalCursor, AZoneTheTableLacksLeavesTheCursorWithoutText)
{
	Scene scene;
	C_VS_UI_MOUSE_POINTER pointer;
	char owner[] = "Owner";
	pointer.SetCursorPortal(kKnownZone, 10, 20, owner);
	CHECK(Same("Known Zone", pointer.*Get(TextOf())));
	pointer.SetCursorPortal(kUnknownZone, 10, 20, owner);
	CHECK(Same("", pointer.*Get(TextOf())));
}
