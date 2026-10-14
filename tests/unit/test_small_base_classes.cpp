#include "test_framework.h"
#include "CAvi.h"
#include "MWorkNode.h"
#include "WinLib/CWinUpdate.h"
#include <string>
#include <vector>

// Three small classes the game builds on:
// - CAVI, the opening video player, is a stub in this client: nothing
//   opens or plays, and the end flag starts clear.
// - CWinUpdate, the base of the game's update loops (opening, waiting,
//   game), does nothing itself; its subclasses override Init and Update.
// - MWorkNode, a job for the loading thread: of type 0 at first and not
//   running; its type can be set and asked for, and Stop marks it stopped.
namespace {

std::vector<std::string> calls;

struct Loop : CWinUpdate
{
	void Init() override { calls.push_back("init"); }
	void Update() override { calls.push_back("update"); }
};

struct Job : MWorkNode
{
	BOOL Execute(MWorkNode*& pNode) override { pNode = nullptr; return TRUE; }
	void Run() { m_bExecute = TRUE; }
};

} // namespace

TEST(SmallBaseClasses, TheVideoPlayerOpensAndPlaysNothing)
{
	CAVI video;
	CHECK_EQ(0, video.bEndFlag);
	CHECK(!video.OpenMPG(nullptr, "opening.mpg"));
	CHECK(!video.OpenAVI(nullptr, "opening.avi", 640, 480));
	CHECK(!video.Play());
	video.Stop();
	video.Close();
	CHECK_EQ(0, video.bEndFlag);
}

TEST(SmallBaseClasses, AnUpdateLoopRunsItsOwnInitAndUpdate)
{
	calls.clear();
	// The base does nothing.
	CWinUpdate base;
	base.Init();
	base.Update();
	CHECK(calls.empty());
	// A subclass's own, through the base.
	Loop loop;
	CWinUpdate& asBase = loop;
	asBase.Init();
	asBase.Update();
	CHECK(calls == (std::vector<std::string>{"init", "update"}));
}

TEST(SmallBaseClasses, AWorkNodeStartsUntypedAndStopped)
{
	Job job;
	CHECK_EQ(0, job.GetType());
	CHECK(job.IsTypeOf(0));
	CHECK(!job.IsExecute() && job.IsStop());
	job.SetType(7);
	CHECK(job.GetType() == 7 && job.IsTypeOf(7) && !job.IsTypeOf(0));
	job.Run();
	CHECK(job.IsExecute() && !job.IsStop());
	job.Stop();
	CHECK(!job.IsExecute() && job.IsStop());
	MWorkNode* pRemain = &job;
	CHECK(job.Execute(pRemain) && pRemain == nullptr);
}
