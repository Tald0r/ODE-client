#include "test_framework.h"
#include "MWorkNode.h"
#include "WinLib/CWinUpdate.h"
#include <cstring>
#include <new>
#include <string>
#include <type_traits>
#include <vector>

// Two small classes the game builds on:
// - CWinUpdate, the base of the game's update loops (waiting, game),
//   does nothing itself; its subclasses override Init and Update.
// - MWorkNode, a job for the loading thread: of type 0 at first and not
//   running; its type can be set and asked for, and Stop marks it stopped.
namespace {

std::vector<std::string> calls;

struct Loop : CWinUpdate
{
	void Init() override { calls.push_back("init"); }
	void Update() override { calls.push_back("update"); }
};

bool jobGone = false;

struct Job : MWorkNode
{
	~Job() override { jobGone = true; }
	BOOL Execute(MWorkNode*& pNode) override { pNode = nullptr; return TRUE; }
	void Run() { m_bExecute = TRUE; }
};

// The loading thread deletes jobs through MWorkNode*.
static_assert(std::has_virtual_destructor_v<MWorkNode>);

// Builds a T over storage filled with a non-zero byte, so a member the
// constructor leaves unset shows.
template <typename T>
struct Dirty
{
	alignas(T) unsigned char storage[sizeof(T)];
	T* object;
	// Default-initialised: T() would zero a class whose constructor is implicit.
	Dirty() { std::memset(storage, 0xA5, sizeof(storage)); object = new (storage) T; }
	~Dirty() { object->~T(); }
};

} // namespace

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
	Dirty<Job> dirty;
	Job& job = *dirty.object;
	CHECK_EQ(0, job.GetType());
	CHECK(job.IsTypeOf(0));
	CHECK(!job.IsExecute() && job.IsStop());
	job.SetType(7);
	CHECK(job.GetType() == 7 && job.IsTypeOf(7) && !job.IsTypeOf(0));
	job.Run();
	CHECK(job.IsExecute() && !job.IsStop());
	job.Stop();
	CHECK(!job.IsExecute() && job.IsStop());
	// A job runs and is deleted through the base.
	MWorkNode& asNode = job;
	MWorkNode* pRemain = &job;
	CHECK(asNode.Execute(pRemain) && pRemain == nullptr);
	jobGone = false;
	MWorkNode* pOwned = new Job;
	delete pOwned;
	CHECK(jobGone);
}
