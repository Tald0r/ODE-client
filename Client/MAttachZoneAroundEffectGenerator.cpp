// MAttachZoneAroundEffectGenerator.cpp
#include "Client_PCH.h"
#include "MAttachZoneAroundEffectGenerator.h"
#include "MEffect.h"
#include "MEventQueue.h"
#include "EffectSpriteTypeDef.h"
#include "WorldTileGeometry.h"
#include <algorithm>
#include <utility>

const MAroundGroundEffectHost* MAttachZoneAroundEffectGenerator::s_pHost = nullptr;

const MAroundGroundEffectHost* MAttachZoneAroundEffectGenerator::SetHost(const MAroundGroundEffectHost* host)
{
	const auto* previous = s_pHost;
	s_pHost = host;
	return previous;
}

bool MAttachZoneAroundEffectGenerator::ReadSprite(TYPE_EFFECTSPRITETYPE type, MFixedZoneEffectSprite& sprite)
{
	sprite = {};
	return s_pHost && s_pHost->Sprite && s_pHost->Sprite(type, sprite);
}

bool MAttachZoneAroundEffectGenerator::QueueEffect(std::unique_ptr<MEffect> effect)
{
	return s_pHost && s_pHost->Queue && s_pHost->Queue(std::move(effect));
}

void MAttachZoneAroundEffectGenerator::AddEvent(MEvent& event)
{
	if (s_pHost && s_pHost->AddEvent) s_pHost->AddEvent(event);
}

bool MAttachZoneAroundEffectGenerator::Generate(const EFFECTGENERATOR_INFO& egInfo)
{
	MFixedZoneEffectSprite sprite;
	if (!ReadSprite(egInfo.effectSpriteType, sprite)) return false;
	bool accepted = false;
	const TYPE_SECTORPOSITION tX = static_cast<TYPE_SECTORPOSITION>(WorldTileGeometry::PixelToTileX(egInfo.x1));
	const TYPE_SECTORPOSITION tY = static_cast<TYPE_SECTORPOSITION>(WorldTileGeometry::PixelToTileY(egInfo.y1));

	if (egInfo.effectSpriteType == EFFECTSPRITETYPE_GREAT_RUFFIAN_1_AXE_GROUND)
	{
		if (egInfo.direction >= 8) return false;
		const int offsetX[8][2] = {{0,0},{0,7},{-7,7},{-7,0},{0,0},{-7,0},{-7,7},{0,7}};
		const int offsetY[8][2] = {{-7,7},{-7,0},{0,0},{0,-7},{-7,7},{0,7},{0,0},{7,0}};
		for (int i = 0; i < 3; ++i)
		{
			auto effect = std::make_unique<MEffect>(sprite.bltType);
			MEffect* pEffect = effect.get();
			pEffect->SetFrameID(sprite.frameID, static_cast<BYTE>(sprite.maxFrames));
			pEffect->SetStepPixel(egInfo.step);
			if (i > 0)
				pEffect->SetPosition(static_cast<TYPE_SECTORPOSITION>(tX + offsetX[egInfo.direction][i - 1]),
					static_cast<TYPE_SECTORPOSITION>(tY + offsetY[egInfo.direction][i - 1]));
			else
				pEffect->SetPosition(tX, tY);
			pEffect->SetCount(egInfo.count, egInfo.linkCount);
			pEffect->SetDirection(egInfo.direction);
			pEffect->SetPower(egInfo.power);
			if (QueueEffect(std::move(effect)))
			{
				if (!accepted)
					pEffect->SetLink(egInfo.nActionInfo, egInfo.pEffectTarget);
				else if (egInfo.pEffectTarget != nullptr)
					pEffect->SetLink(egInfo.nActionInfo, new MEffectTarget(*egInfo.pEffectTarget));
				accepted = true;
			}
		}
		MEvent event;
		event.eventID = EVENTID_METEOR_SHAKE;
		event.eventType = EVENTTYPE_ZONE;
		event.eventDelay = 500;
		event.eventFlag = EVENTFLAG_SHAKE_SCREEN;
		event.parameter3 = 3;
		AddEvent(event);
		return accepted;
	}

	for (int y = -1; y <= 1; ++y)
	{
		for (int x = -1; x <= 1; ++x)
		{
			if (x == 0 && y == 0) continue;
			const BYTE direction = static_cast<BYTE>(y == -1 ? 6 - x : y == 0 ? std::max(0, x) * 4 : x + 2);
			auto effect = std::make_unique<MEffect>(sprite.bltType);
			MEffect* pEffect = effect.get();
			pEffect->SetFrameID(sprite.frameID, static_cast<BYTE>(sprite.maxFrames));
			pEffect->SetStepPixel(egInfo.step);
			pEffect->SetPosition(static_cast<TYPE_SECTORPOSITION>(tX + x), static_cast<TYPE_SECTORPOSITION>(tY + y));
			pEffect->SetCount(egInfo.count, egInfo.linkCount);
			pEffect->SetDirection(direction);
			pEffect->SetPower(egInfo.power);
			if (QueueEffect(std::move(effect)))
			{
				// The first retained effect owns the original; other branches need
				// independent continuation targets, never aliases of the same owner.
				MEffectTarget* target = egInfo.pEffectTarget;
				if (accepted && target != nullptr) target = new MEffectTarget(*target);
				pEffect->SetLink(egInfo.nActionInfo, target);
				accepted = true;
			}
		}
	}
	return accepted;
}
