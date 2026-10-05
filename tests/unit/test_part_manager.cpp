//----------------------------------------------------------------------
// test_part_manager.cpp
//----------------------------------------------------------------------
//
// CPartManager backs the sound cache (CSoundPartManager): a fixed pool
// of part slots serving a larger index space with LRU eviction. The
// indices it is fed come from data files (Sound.inf, zone sound
// tables), so an out-of-range index must be answered, not used to
// subscript m_pPartIndex. These tests pin that, the reserved-sentinel
// clamps in Init, the counter-rollover normalization, and the virtual
// Release() dispatch that CSoundPartManager's resource cleanup needs.
//
//----------------------------------------------------------------------

#include "test_framework.h"

#include "basic/Platform.h"
#include "CPartManager.h"

namespace {

typedef CPartManager<WORD, BYTE, int*> SoundLikeManager;

// Exposes the protected counter and timestamp state so the rollover
// path (m_Counter reaching 0xFFFFFFFF) can be driven directly.
struct TestManager : public SoundLikeManager {
	void SetCounter(DWORD counter)					{ m_Counter = counter; }
	DWORD GetCounter() const						{ return m_Counter; }
	void SetLastTime(int part, DWORD time)			{ m_pLastTime[part] = time; }
	DWORD GetLastTime(int part) const				{ return m_pLastTime[part]; }
};

// Counts the per-slot release hook so the virtual dispatch from the
// base class (CPartManager::Release, reached from Init) is observable.
struct ReleaseCountingManager : public SoundLikeManager {
	static int s_nReleases;
	virtual void OnReleaseData(int*& data)
	{
		if (data != NULL)
		{
			s_nReleases++;
			data = NULL;
		}
	}
};

int ReleaseCountingManager::s_nReleases = 0;

int g_Slots[16];

} // namespace

//----------------------------------------------------------------------
// Out-of-range indices
//----------------------------------------------------------------------

TEST(CPartManager, OutOfRangeIndexHasNoData)
{
	SoundLikeManager manager;
	manager.Init(10, 4);

	CHECK_EQ(true, manager.IsDataNULL(10));
	CHECK_EQ(false, manager.IsDataNotNULL(10));

	int* pData = NULL;
	CHECK_EQ(false, manager.GetData(10, pData));
}

TEST(CPartManager, SetDataOutOfRangeIsRejected)
{
	SoundLikeManager manager;
	manager.Init(10, 4);

	manager.SetData(10, &g_Slots[0]);
	CHECK_EQ(0, (int)manager.GetUsed());

	int* pOld = NULL;
	CHECK_EQ(0xFFFF, (int)manager.SetData(10, &g_Slots[0], pOld));
	CHECK_EQ(0, (int)manager.GetUsed());
}

TEST(CPartManager, UninitializedManagerAnswersInsteadOfCrashing)
{
	SoundLikeManager manager;

	CHECK_EQ(true, manager.IsDataNULL(0));

	int* pData = NULL;
	CHECK_EQ(false, manager.GetData(0, pData));

	manager.SetData(0, &g_Slots[0]);
	CHECK_EQ(0, (int)manager.GetUsed());
}

//----------------------------------------------------------------------
// Basic caching and eviction
//----------------------------------------------------------------------

TEST(CPartManager, StoresAndReturnsData)
{
	SoundLikeManager manager;
	manager.Init(10, 4);

	manager.SetData(3, &g_Slots[3]);
	CHECK_EQ(true, manager.IsDataNotNULL(3));

	int* pData = NULL;
	CHECK_EQ(true, manager.GetData(3, pData));
	CHECK(pData == &g_Slots[3]);
}

TEST(CPartManager, EvictsTheLeastRecentlyUsedEntry)
{
	SoundLikeManager manager;
	manager.Init(10, 2);

	int* pOld = NULL;
	CHECK_EQ(0xFFFF, (int)manager.SetData(0, &g_Slots[0], pOld));
	CHECK_EQ(0xFFFF, (int)manager.SetData(1, &g_Slots[1], pOld));

	// Touch 0 so 1 is the least recently used.
	int* pData = NULL;
	CHECK_EQ(true, manager.GetData(0, pData));

	// A third entry must evict index 1 and hand back its data.
	CHECK_EQ(1, (int)manager.SetData(2, &g_Slots[2], pOld));
	CHECK(pOld == &g_Slots[1]);
	CHECK_EQ(true, manager.IsDataNULL(1));
	CHECK_EQ(true, manager.IsDataNotNULL(0));
	CHECK_EQ(true, manager.IsDataNotNULL(2));
}

//----------------------------------------------------------------------
// Init clamps the counts below the reserved NULL sentinels
//----------------------------------------------------------------------

TEST(CPartManager, PartCountStaysBelowTheSentinel)
{
	// 0xFF is m_PartIndexNULL for a BYTE part index; a pool of that size
	// would make a real slot indistinguishable from "no data".
	SoundLikeManager manager;
	manager.Init(1000, 0xFF);

	CHECK_EQ(0xFE, (int)manager.GetMaxPartIndex());
}

TEST(CPartManager, IndexCountStaysBelowTheSentinel)
{
	SoundLikeManager manager;
	manager.Init(0xFFFF, 4);

	CHECK_EQ(0xFFFE, (int)manager.GetMaxIndex());
}

//----------------------------------------------------------------------
// Counter rollover
//----------------------------------------------------------------------

TEST(CPartManager, RolloverNormalizesEveryTimestamp)
{
	TestManager manager;
	manager.Init(8, 4);

	int* pOld = NULL;
	for (int i = 0; i < 4; i++)
	{
		manager.SetData((WORD)i, &g_Slots[i], pOld);
	}

	// The first four SetData calls map index i to part i. Arrange the
	// timestamps so the minimum sits in the middle: the old code
	// subtracted m_pLastTime[min] inside the loop, zeroing the slot on
	// its own iteration and leaving every later slot unnormalized.
	manager.SetLastTime(0, 100);
	manager.SetLastTime(1, 50);
	manager.SetLastTime(2, 200);
	manager.SetLastTime(3, 75);

	// GetData stamps the touched slot with m_Counter++ and then runs the
	// rollover once the counter reaches 0xFFFFFFFF.
	manager.SetCounter(0xFFFFFFFE);
	int* pData = NULL;
	CHECK_EQ(true, manager.GetData(0, pData));

	// Slot 0 was stamped 0xFFFFFFFE before normalization; 50 was the
	// minimum, so every slot must have moved down by 50.
	CHECK_EQ(0xFFFFFFFE - 50, manager.GetLastTime(0));
	CHECK_EQ((DWORD)0, manager.GetLastTime(1));
	CHECK_EQ((DWORD)150, manager.GetLastTime(2));
	CHECK_EQ((DWORD)25, manager.GetLastTime(3));

	// And the counter resumes from the largest surviving timestamp.
	CHECK_EQ(0xFFFFFFFE - 50, manager.GetCounter());
}

//----------------------------------------------------------------------
// Per-slot release hook
//----------------------------------------------------------------------

TEST(CPartManager, ReInitReleasesEveryStoredEntryThroughTheHook)
{
	// CSoundPartManager frees its cached sound buffers in
	// OnReleaseData(), which the base Release() calls per slot.
	// CPartManager::Init calls Release() before reallocating; before the
	// hook existed a re-Init skipped the derived cleanup entirely and
	// leaked every cached buffer.
	ReleaseCountingManager::s_nReleases = 0;

	SoundLikeManager* pManager = new ReleaseCountingManager;
	pManager->Init(4, 2);
	CHECK_EQ(0, ReleaseCountingManager::s_nReleases);

	int* pOld = NULL;
	pManager->SetData(0, &g_Slots[0], pOld);
	pManager->SetData(1, &g_Slots[1], pOld);

	pManager->Init(4, 2);
	CHECK_EQ(2, ReleaseCountingManager::s_nReleases);

	pManager->Release();
	CHECK_EQ(2, ReleaseCountingManager::s_nReleases);

	delete pManager;
}
