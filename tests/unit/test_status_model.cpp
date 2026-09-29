//----------------------------------------------------------------------
// The creature status array (MStatus) and the request/answer mode
// register (TempInformation), both in gamemodel since task 4.14.
//
// MStatus is the array every ModifyInfo packet writes through
// SetStatus. MCreature and MPlayer override SetStatus and bound the
// index first; the base class does not, and nothing here calls it with
// an index past MAX_MODIFY (docs/RESTRUCTURING.md task 4.14 has the
// trace of which callers can reach it). The overrides live in the
// executable and cannot be linked into this binary.
//----------------------------------------------------------------------
#include "test_framework.h"
#include "Client/MStatus.h"
#include "Client/TempInformation.h"

#include <cstdint>
#include <cstring>
#include <new>
#include <vector>

namespace
{
	// A distinct, recognisable value per slot, never MODIFY_NULL.
	DWORD SlotValue(DWORD n)
	{
		return 0x10000u + n * 0x101u;
	}

	// Records every SetStatus the base class dispatches, then stores
	// the value as the base does.
	class RecordingStatus : public MStatus
	{
	public:
		void SetStatus(DWORD n, DWORD value) override
		{
			calls.push_back(Call{n, value});
			MStatus::SetStatus(n, value);
		}

		struct Call
		{
			DWORD n;
			DWORD value;
		};
		std::vector<Call> calls;
	};

	typedef DWORD (MStatus::*Getter)() const;

	struct NamedSlot
	{
		Getter get;
		DWORD n;
	};

	// Every named accessor and the slot it is documented to read.
	const NamedSlot kNamedSlots[] = {
		{ &MStatus::GetBASIC_STR, MODIFY_BASIC_STR },
		{ &MStatus::GetSTR, MODIFY_CURRENT_STR },
		{ &MStatus::GetMAX_STR, MODIFY_MAX_STR },
		{ &MStatus::GetSTR_EXP, MODIFY_STR_EXP_REMAIN },
		{ &MStatus::GetBASIC_DEX, MODIFY_BASIC_DEX },
		{ &MStatus::GetDEX, MODIFY_CURRENT_DEX },
		{ &MStatus::GetMAX_DEX, MODIFY_MAX_DEX },
		{ &MStatus::GetDEX_EXP, MODIFY_DEX_EXP_REMAIN },
		{ &MStatus::GetBASIC_INT, MODIFY_BASIC_INT },
		{ &MStatus::GetINT, MODIFY_CURRENT_INT },
		{ &MStatus::GetMAX_INT, MODIFY_MAX_INT },
		{ &MStatus::GetINT_EXP, MODIFY_INT_EXP_REMAIN },
		{ &MStatus::GetHP, MODIFY_CURRENT_HP },
		{ &MStatus::GetMAX_HP, MODIFY_MAX_HP },
		{ &MStatus::GetMP, MODIFY_CURRENT_MP },
		{ &MStatus::GetMAX_MP, MODIFY_MAX_MP },
		{ &MStatus::GetMIN_DAMAGE, MODIFY_MIN_DAMAGE },
		{ &MStatus::GetMAX_DAMAGE, MODIFY_MAX_DAMAGE },
		{ &MStatus::GetDefense, MODIFY_DEFENSE },
		{ &MStatus::GetProtection, MODIFY_PROTECTION },
		{ &MStatus::GetTOHIT, MODIFY_TOHIT },
		{ &MStatus::GetVision, MODIFY_VISION },
		{ &MStatus::GetFAME, MODIFY_FAME },
		{ &MStatus::GetGold, MODIFY_GOLD },
		{ &MStatus::GetSWORD_DOMAIN_LEVEL, MODIFY_SWORD_DOMAIN_LEVEL },
		{ &MStatus::GetBLADE_DOMAIN_LEVEL, MODIFY_BLADE_DOMAIN_LEVEL },
		{ &MStatus::GetHEAL_DOMAIN_LEVEL, MODIFY_HEAL_DOMAIN_LEVEL },
		{ &MStatus::GetENCHANT_DOMAIN_LEVEL, MODIFY_ENCHANT_DOMAIN_LEVEL },
		{ &MStatus::GetGUN_DOMAIN_LEVEL, MODIFY_GUN_DOMAIN_LEVEL },
		{ &MStatus::GetETC_DOMAIN_LEVEL, MODIFY_ETC_DOMAIN_LEVEL },
		{ &MStatus::GetSWORD_DOMAIN_EXP, MODIFY_SWORD_DOMAIN_EXP_REMAIN },
		{ &MStatus::GetBLADE_DOMAIN_EXP, MODIFY_BLADE_DOMAIN_EXP_REMAIN },
		{ &MStatus::GetHEAL_DOMAIN_EXP, MODIFY_HEAL_DOMAIN_EXP_REMAIN },
		{ &MStatus::GetENCHANT_DOMAIN_EXP, MODIFY_ENCHANT_DOMAIN_EXP_REMAIN },
		{ &MStatus::GetGUN_DOMAIN_EXP, MODIFY_GUN_DOMAIN_EXP_REMAIN },
		{ &MStatus::GetETC_DOMAIN_EXP, MODIFY_ETC_DOMAIN_EXP_REMAIN },
		{ &MStatus::GetSkillLevel, MODIFY_SKILL_LEVEL },
		{ &MStatus::GetLEVEL, MODIFY_LEVEL },
		{ &MStatus::GetEFFECT_STAT, MODIFY_EFFECT_STAT },
		{ &MStatus::GetDURATION, MODIFY_DURATION },
		{ &MStatus::GetBullet, MODIFY_BULLET },
		{ &MStatus::GetBonusPoint, MODIFY_BONUS_POINT },
		{ &MStatus::GetDurability, MODIFY_DURABILITY },
		{ &MStatus::GetNotoriety, MODIFY_NOTORIETY },
		{ &MStatus::GetVampExp, MODIFY_VAMP_EXP_REMAIN },
		{ &MStatus::GetSilverDamage, MODIFY_SILVER_DAMAGE },
		{ &MStatus::GetAttackSpeed, MODIFY_ATTACK_SPEED },
		{ &MStatus::GetAlignment, MODIFY_ALIGNMENT },
		{ &MStatus::GetSilverDurability, MODIFY_SILVER_DURABILITY },
		{ &MStatus::GetGrade, MODIFY_RANK },
		{ &MStatus::GetGradeExpRemain, MODIFY_RANK_EXP_REMAIN },
		{ &MStatus::GetOustersExp, MODIFY_OUSTERS_EXP_REMAIN },
		{ &MStatus::GetSkillBounsPoint, MODIFY_SKILL_BONUS_POINT },
		{ &MStatus::GetElementalFire, MODIFY_ELEMENTAL_FIRE },
		{ &MStatus::GetElementalWater, MODIFY_ELEMENTAL_WATER },
		{ &MStatus::GetElementalEarth, MODIFY_ELEMENTAL_EARTH },
		{ &MStatus::GetElementalWind, MODIFY_ELEMENTAL_WIND },
		{ &MStatus::GetAdvancementClassLevel, MODIFY_ADVANCEMENT_CLASS_LEVEL },
		{ &MStatus::GetAdvancementClassGoalExp, MODIFY_ADVANCEMENT_CLASS_GOAL_EXP },
	};
}

//----------------------------------------------------------------------
// MStatus
//----------------------------------------------------------------------

TEST(MStatus, ConstructorLeavesEverySlotNullButTheAdvancementClassLevel)
{
	MStatus status;
	for (DWORD n = 0; n < MAX_MODIFY; ++n)
	{
		const DWORD expected = (n == MODIFY_ADVANCEMENT_CLASS_LEVEL) ? 0u : (DWORD)MODIFY_NULL;
		CHECK_EQ(expected, status.GetStatus(n));
	}
}

TEST(MStatus, ClearStatusZeroesEverySlot)
{
	MStatus status;
	for (DWORD n = 0; n < MAX_MODIFY; ++n)
		status.SetStatus(n, SlotValue(n));

	status.ClearStatus();

	for (DWORD n = 0; n < MAX_MODIFY; ++n)
		CHECK_EQ(0u, status.GetStatus(n));
}

TEST(MStatus, SetStatusStoresEverySlotWithoutTouchingAnother)
{
	for (DWORD target = 0; target < MAX_MODIFY; ++target)
	{
		MStatus status;
		status.ClearStatus();
		status.SetStatus(target, SlotValue(target));

		for (DWORD n = 0; n < MAX_MODIFY; ++n)
			CHECK_EQ(n == target ? SlotValue(target) : 0u, status.GetStatus(n));
	}
}

TEST(MStatus, EverySlotHoldsTheWholeDwordRange)
{
	const DWORD values[] = { 0u, 1u, 0x7FFFFFFFu, 0x80000000u, 0xFFFFFFFEu, 0xFFFFFFFFu };
	MStatus status;
	for (DWORD value : values)
	{
		for (DWORD n = 0; n < MAX_MODIFY; ++n)
			status.SetStatus(n, value);
		for (DWORD n = 0; n < MAX_MODIFY; ++n)
			CHECK_EQ(value, status.GetStatus(n));
	}
}

TEST(MStatus, TheLastSlotIsTheAdvancementClassGoalExperience)
{
	// The array is sized by MODIFY_MAX; the last slot the protocol names
	// sits directly below it.
	CHECK_EQ((DWORD)MODIFY_ADVANCEMENT_CLASS_GOAL_EXP + 1, (DWORD)MAX_MODIFY);

	MStatus status;
	status.SetStatus(MAX_MODIFY - 1, 0x12345678u);
	CHECK_EQ(0x12345678u, status.GetStatus(MAX_MODIFY - 1));
	CHECK_EQ(0x12345678u, status.GetAdvancementClassGoalExp());
}

TEST(MStatus, EveryNamedAccessorReadsItsOwnSlot)
{
	MStatus status;
	for (DWORD n = 0; n < MAX_MODIFY; ++n)
		status.SetStatus(n, SlotValue(n));

	for (const NamedSlot& slot : kNamedSlots)
		CHECK_EQ(SlotValue(slot.n), (status.*slot.get)());
}

TEST(MStatus, ApplyStatusCopiesOnlyTheSlotsThatAreNotNull)
{
	MStatus source;		// every slot MODIFY_NULL but the advancement level (0)
	source.SetStatus(MODIFY_CURRENT_HP, 120);
	source.SetStatus(MODIFY_GOLD, 0);
	source.SetStatus(MODIFY_ALIGNMENT, 0xFFFFFFFEu);

	MStatus target;
	for (DWORD n = 0; n < MAX_MODIFY; ++n)
		target.SetStatus(n, SlotValue(n));

	target.ApplyStatus(source);

	for (DWORD n = 0; n < MAX_MODIFY; ++n)
	{
		DWORD expected = SlotValue(n);
		if (n == MODIFY_CURRENT_HP)
			expected = 120;
		else if (n == MODIFY_GOLD || n == MODIFY_ADVANCEMENT_CLASS_LEVEL)
			expected = 0;
		else if (n == MODIFY_ALIGNMENT)
			expected = 0xFFFFFFFEu;
		CHECK_EQ(expected, target.GetStatus(n));
	}
}

TEST(MStatus, ApplyStatusSkipsAValueEqualToTheNullMarker)
{
	// MODIFY_NULL is a legal DWORD a slot can be set to, and ApplyStatus
	// cannot tell it from "not set": such a value is never copied.
	MStatus source;
	source.ClearStatus();
	source.SetStatus(MODIFY_FAME, MODIFY_NULL);

	MStatus target;
	target.SetStatus(MODIFY_FAME, 77);
	target.ApplyStatus(source);

	CHECK_EQ(77u, target.GetStatus(MODIFY_FAME));
}

TEST(MStatus, ApplyStatusWritesThroughTheOverride)
{
	// MCreature and MPlayer react to a status change in their SetStatus
	// (HP numbers, guild marks, the player's modify handlers), so the
	// base class must dispatch through the virtual, in slot order.
	MStatus source;
	source.SetStatus(MODIFY_MAX_HP, 300);
	source.SetStatus(MODIFY_CURRENT_HP, 250);

	RecordingStatus target;
	target.ApplyStatus(source);

	CHECK_EQ((size_t)3, target.calls.size());
	if (target.calls.size() == 3)
	{
		CHECK_EQ((DWORD)MODIFY_CURRENT_HP, target.calls[0].n);
		CHECK_EQ(250u, target.calls[0].value);
		CHECK_EQ((DWORD)MODIFY_MAX_HP, target.calls[1].n);
		CHECK_EQ(300u, target.calls[1].value);
		CHECK_EQ((DWORD)MODIFY_ADVANCEMENT_CLASS_LEVEL, target.calls[2].n);
		CHECK_EQ(0u, target.calls[2].value);
	}
	CHECK_EQ(250u, target.GetHP());
	CHECK_EQ(300u, target.GetMAX_HP());
}

TEST(MStatus, ADerivedStatusIsDestroyedThroughTheBase)
{
	MStatus* status = new RecordingStatus;
	status->SetStatus(MODIFY_LEVEL, 5);
	CHECK_EQ((size_t)1, static_cast<RecordingStatus*>(status)->calls.size());
	delete status;
}

//----------------------------------------------------------------------
// TempInformation
//----------------------------------------------------------------------

TEST(TempInformation, ConstructorStartsWithNoMode)
{
	TempInformation info;
	CHECK_EQ(TempInformation::MODE_NULL, info.GetMode());
	CHECK_EQ(TempInformation::MODE_NULL, info.Mode);
	CHECK(info.StrValue1.empty());
	CHECK(info.StrValue2.empty());
	CHECK(info.StrValue3.empty());
}

TEST(TempInformation, ConstructorClearsEveryParkedValue)
{
	// GameInit allocates the register on the heap. A reply handler that
	// reads a slot before any dialog wrote it must see a defined value:
	// GCPartyInviteHandler's GC_PARTY_INVITE_ACCEPT looks up
	// PartyInviter whether or not an invitation set it. Construct over
	// poisoned storage so an unset member shows as the poison.
	alignas(TempInformation) unsigned char storage[sizeof(TempInformation)];
	std::memset(storage, 0xAB, sizeof(storage));
	TempInformation* info = ::new (storage) TempInformation;

	CHECK_EQ(TempInformation::MODE_NULL, info->GetMode());
	CHECK_EQ((intptr_t)0, info->Value1);
	CHECK_EQ((intptr_t)0, info->Value2);
	CHECK_EQ((intptr_t)0, info->Value3);
	CHECK_EQ((intptr_t)0, info->Value4);
	CHECK_EQ(0, info->PartyInviter);
	CHECK(info->pValue == NULL);
	CHECK(info->TimeValue1 == MonotonicClock::TimePoint());

	info->~TempInformation();
}

TEST(TempInformation, SetModeRoundTripsEveryMode)
{
	TempInformation info;
	for (int mode = TempInformation::MODE_NULL; mode < TempInformation::MAX_MODE; ++mode)
	{
		info.SetMode((TempInformation::TEMP_MODE)mode);
		CHECK_EQ(mode, (int)info.GetMode());
		CHECK_EQ(mode, (int)info.Mode);
	}
	info.SetMode(TempInformation::MODE_NULL);
	CHECK_EQ(TempInformation::MODE_NULL, info.GetMode());
}

TEST(TempInformation, SetModeLeavesTheParkedValuesAlone)
{
	// The dialogs park their request's arguments before they set the
	// mode, and the reply handler reads them after it checks the mode:
	// changing the mode must not clear them.
	TempInformation info;
	info.Value1 = 1;
	info.Value2 = 2;
	info.Value3 = 3;
	info.Value4 = 4;
	info.PartyInviter = 5;
	info.StrValue1 = "name";
	info.pValue = &info;

	info.SetMode(TempInformation::MODE_SHOP_BUY);
	info.SetMode(TempInformation::MODE_NULL);

	CHECK_EQ((intptr_t)1, info.Value1);
	CHECK_EQ((intptr_t)2, info.Value2);
	CHECK_EQ((intptr_t)3, info.Value3);
	CHECK_EQ((intptr_t)4, info.Value4);
	CHECK_EQ(5, info.PartyInviter);
	CHECK(info.StrValue1 == "name");
	CHECK(info.pValue == &info);
}

TEST(TempInformation, AValueSlotHoldsAWholePointer)
{
	// Several modes park an item pointer in Value1..4; intptr_t keeps
	// all of it on a 64-bit target.
	TempInformation info;
	int object = 0;
	info.Value4 = reinterpret_cast<intptr_t>(&object);
	CHECK(reinterpret_cast<int*>(info.Value4) == &object);
}

TEST(TempInformation, TheGlobalIsDefinedInTheLibraryAndStartsEmpty)
{
	// GameInit creates the register; until then the pointer is NULL.
	CHECK(g_pTempInformation == NULL);
}
