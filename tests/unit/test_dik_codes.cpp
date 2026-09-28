//----------------------------------------------------------------------
// test_dik_codes.cpp
//----------------------------------------------------------------------
//
// VS_UI/DIK_Defines.h and basic/InputCodes.h both define DirectInput
// DIK_* key codes, and a translation unit may include both. Every name
// they share must have the DirectInput value in each header, so the
// value a TU sees does not depend on which header it includes last.
// The SDL backend produces these values (DXLibBackendSDL.cpp's table)
// and the key-name tables in VS_UI index by them.
//
// DIK_Defines.h is included first and its values captured before
// InputCodes.h can redefine them.
//
//----------------------------------------------------------------------

#include "test_framework.h"

#include "VS_UI/DIK_Defines.h"

namespace {

constexpr int kVsLwin = DIK_LWIN;
constexpr int kVsRwin = DIK_RWIN;
constexpr int kVsApps = DIK_APPS;
constexpr int kVsSysRq = DIK_SYSRQ;
constexpr int kVsKanji = DIK_KANJI;
constexpr int kVsPause = DIK_PAUSE;

// Names that are not DirectInput names and alias real keys
// (INSERT, DELETE and NUMPAD4).
#ifdef DIK_PRINT
constexpr bool kVsHasPrint = true;
#else
constexpr bool kVsHasPrint = false;
#endif
#ifdef DIK_BREAK
constexpr bool kVsHasBreak = true;
#else
constexpr bool kVsHasBreak = false;
#endif
#ifdef DIK_CANCEL
constexpr bool kVsHasCancel = true;
#else
constexpr bool kVsHasCancel = false;
#endif

}

#include "basic/InputCodes.h"

TEST(DikCodes, VsUiHeaderUsesDirectInputValues)
{
	CHECK_EQ(0xDB, kVsLwin);
	CHECK_EQ(0xDC, kVsRwin);
	CHECK_EQ(0xDD, kVsApps);
	CHECK_EQ(0xB7, kVsSysRq);
	CHECK_EQ(0x94, kVsKanji);
	CHECK_EQ(0xC5, kVsPause);
}

TEST(DikCodes, VsUiHeaderDefinesNoAliasesOfOtherKeys)
{
	CHECK(!kVsHasPrint);
	CHECK(!kVsHasBreak);
	CHECK(!kVsHasCancel);
}

TEST(DikCodes, BothHeadersAgreeWhicheverIsIncludedLast)
{
	CHECK_EQ(0xDB, DIK_LWIN);
	CHECK_EQ(0xDC, DIK_RWIN);
	CHECK_EQ(0xDD, DIK_APPS);
	CHECK_EQ(0xB7, DIK_SYSRQ);
	CHECK_EQ(0x94, DIK_KANJI);
	CHECK_EQ(kVsLwin, DIK_LWIN);
	CHECK_EQ(kVsRwin, DIK_RWIN);
	CHECK_EQ(kVsApps, DIK_APPS);
	CHECK_EQ(kVsSysRq, DIK_SYSRQ);
	CHECK_EQ(kVsKanji, DIK_KANJI);
}
