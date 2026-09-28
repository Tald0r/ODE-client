#include "test_framework.h"
#include "Client/TalkBox.h"

// Three menu lines answering the script's answers 0, 2 and 5, the way
// the GCNPCAsk handlers fill the map: one entry per AddString.
static void FillAnswerMap(PCTalkBox& box)
{
	box.m_AnswerIDMap = {0, 2, 5};
}

TEST(PCTalkBox, MenuLinesMapToTheirAnswerIds)
{
	PCTalkBox box;
	FillAnswerMap(box);
	CHECK_EQ(1, box.MapMenuAnswer(1));
	CHECK_EQ(3, box.MapMenuAnswer(2));
	CHECK_EQ(6, box.MapMenuAnswer(3));
}

TEST(PCTalkBox, AnIdPastTheMapIsPassedThrough)
{
	PCTalkBox box;
	FillAnswerMap(box);
	CHECK_EQ(4, box.MapMenuAnswer(4));
}

TEST(PCTalkBox, DialogExecIdsArePassedThrough)
{
	// DIALOG_EXECID_EXIT and DIALOG_EXECID_OK (VS_UI_Dialog.h), which
	// ESC and RETURN produce, arrive as negative ints.
	PCTalkBox box;
	FillAnswerMap(box);
	const int execExit = static_cast<int>(0xFFFF0000u);
	const int execOK = static_cast<int>(0xFFFF0001u);
	CHECK_EQ(execExit, box.MapMenuAnswer(execExit));
	CHECK_EQ(execOK, box.MapMenuAnswer(execOK));
}

TEST(PCTalkBox, AnIdOfZeroIsPassedThrough)
{
	PCTalkBox box;
	FillAnswerMap(box);
	CHECK_EQ(0, box.MapMenuAnswer(0));
}
