#include "test_framework.h"
#include "VS_UI_Base.h"
#include "VS_UI_GameCommon.h"
#include "VS_UI_GlobalResource.h"
#include "MGameStringTable.h"
#include "MStringArray.h"
#include "UserInformation.h"
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

// Server values the UI windows use as an index, past the packet layer
// that packet_indices reads: a guild member's rank (GCShowGuildMemberInfo,
// GCGuildMemberList). None is bounded on the way in; each window answers
// for a value it has no entry for.
namespace {

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

// What a window needs to be built and drawn without the UI's Init: the
// base (its back surface and fonts), the process runner and window
// managers it registers with, the global resource its buttons measure,
// the game's strings and the user's record.
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
	UserInformation* oldUser = g_pUserInformation;
	CI_KOREAN ime;
	CSpriteSurface back;
	Base base;
	timer_id_t blink = INVALID_TID;
	PI_ProcessRunner runner;
	WindowManager windows;
	C_VS_UI_WINDOW_MANAGER uiWindows;
	C_GLOBAL_RESOURCE resource;
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
		g_pUserInformation = &user;
	}
	~Scene()
	{
		g_pUserInformation = oldUser;
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

bool Same(const std::string& expected, const std::string& actual)
{
	if (expected == actual) return true;
	std::printf("      got \"%s\", wanted \"%s\"\n", actual.c_str(), expected.c_str());
	return false;
}

// The member list keeps its members to itself; an explicit instantiation
// may name a private member, so this reads them.
template <class Tag, typename Tag::type Member>
struct Expose
{
	friend typename Tag::type Get(Tag) { return Member; }
};
using MemberList = std::vector<C_VS_UI_TEAM_MEMBER_LIST::TEAM_MEMBER_LIST>;
struct MembersOf { using type = MemberList C_VS_UI_TEAM_MEMBER_LIST::*; friend type Get(MembersOf); };
template struct Expose<MembersOf, &C_VS_UI_TEAM_MEMBER_LIST::m_v_member_list>;

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
