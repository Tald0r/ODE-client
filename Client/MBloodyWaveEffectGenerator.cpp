// MBloodyWaveEffectGenerator.cpp
#include "Client_PCH.h"
#include "MBloodyWaveEffectGenerator.h"
#include "MEffect.h"
#include "EffectSpriteTypeDef.h"
#include "WorldTileGeometry.h"
#include <cstdlib>
#include <utility>
#include <vector>

const MBloodyWaveEffectHost* MBloodyWaveEffectGenerator::s_pHost = nullptr;

const MBloodyWaveEffectHost* MBloodyWaveEffectGenerator::SetHost(const MBloodyWaveEffectHost* host)
{
	const auto* previous = s_pHost;
	s_pHost = host;
	return previous;
}

bool MBloodyWaveEffectGenerator::ReadSprite(TYPE_EFFECTSPRITETYPE type, MBloodyWaveEffectSprite& sprite)
{
	sprite = {};
	return s_pHost && s_pHost->Sprite && s_pHost->Sprite(type, sprite);
}

bool MBloodyWaveEffectGenerator::ReadMaxFrames(BYTE blt, TYPE_FRAMEID frameID, int& count)
{
	count = 0;
	return s_pHost && s_pHost->MaxFrames && s_pHost->MaxFrames(blt, frameID, count);
}

bool MBloodyWaveEffectGenerator::QueueEffect(std::unique_ptr<MEffect> effect)
{
	return s_pHost && s_pHost->Queue && s_pHost->Queue(std::move(effect));
}

bool MBloodyWaveEffectGenerator::Generate(const EFFECTGENERATOR_INFO& egInfo)
{
	bool bOK = false;
	int est = egInfo.effectSpriteType;
	if (est >= EFFECTSPRITETYPE_BLOODY_WALL_1 && est <= EFFECTSPRITETYPE_BLOODY_WALL_3)
		est = EFFECTSPRITETYPE_BLOODY_WALL_1 + std::rand() % 3;

	MBloodyWaveEffectSprite sprite;
	if (!ReadSprite(static_cast<TYPE_EFFECTSPRITETYPE>(est), sprite)) return false;
	const BYTE bltType = sprite.bltType;
	TYPE_FRAMEID frameID = sprite.frameID;
	const bool repeatFrame = sprite.repeatFrame;

	// Calls without a target continue the phase selected by the preceding call.
	// All phases above four share the final pattern, so saturation avoids overflow.
	static int lastPhase = 0;
	const int currentPhase = egInfo.pEffectTarget == nullptr
		? (lastPhase < 5 ? lastPhase + 1 : 5) : egInfo.pEffectTarget->GetCurrentPhase();
	lastPhase = currentPhase;

	std::vector<POINT> v_cp;
	POINT p;

	switch (currentPhase)
	{
		// Cardinal neighbors.
		case 1 :
		{
			p.x = 0; p.y = -1;
			v_cp.push_back(p);
			p.x = 0; p.y = 1;
			v_cp.push_back(p);
			p.x = 1; p.y = 0;
			v_cp.push_back(p);
			p.x = -1; p.y = 0;
			v_cp.push_back(p);
		}
		break;

		// Diagonal neighbors.
		case 2 :
		{
			p.x = 1; p.y = -1;
			v_cp.push_back(p);
			p.x = 1; p.y = 1;
			v_cp.push_back(p);
			p.x = -1; p.y = -1;
			v_cp.push_back(p);
			p.x = -1; p.y = 1;
			v_cp.push_back(p);
		}
		break;

		// Cardinal neighbors two tiles away.
		case 3 :
		{
			p.x = 0; p.y = -2;
			v_cp.push_back(p);
			p.x = 0; p.y = 2;
			v_cp.push_back(p);
			p.x = 2; p.y = 0;
			v_cp.push_back(p);
			p.x = -2; p.y = 0;
			v_cp.push_back(p);
		}
		break;

		// Eight intervening neighbors.
		case 4 :
		{
			p.x = 1; p.y = -2;
			v_cp.push_back(p);
			p.x = 2; p.y = -1;
			v_cp.push_back(p);
			p.x = 1; p.y = 2;
			v_cp.push_back(p);
			p.x = 2; p.y = 1;
			v_cp.push_back(p);
			p.x = -1; p.y = -2;
			v_cp.push_back(p);
			p.x = -2; p.y = -1;
			v_cp.push_back(p);
			p.x = -1; p.y = 2;
			v_cp.push_back(p);
			p.x = -2; p.y = 1;
			v_cp.push_back(p);
		}
		break;

		// Cardinal neighbors three tiles away.
		default:
		{
			p.x = 0; p.y = -3;
			v_cp.push_back(p);
			p.x = 0; p.y = 3;
			v_cp.push_back(p);
			p.x = 3; p.y = 0;
			v_cp.push_back(p);
			p.x = -3; p.y = 0;
			v_cp.push_back(p);
		}
		break;
	}


	const TYPE_SECTORPOSITION tX = static_cast<TYPE_SECTORPOSITION>(WorldTileGeometry::PixelToTileX(egInfo.x1));
	const TYPE_SECTORPOSITION tY = static_cast<TYPE_SECTORPOSITION>(WorldTileGeometry::PixelToTileY(egInfo.y1));
	int maxFrame;
	if (!ReadMaxFrames(bltType, frameID, maxFrame)) return false;

	for (const POINT& offset : v_cp)
	{
		auto effect = std::make_unique<MEffect>(bltType);
		MEffect* pEffect = effect.get();
		pEffect->SetFrameID(frameID, static_cast<BYTE>(maxFrame));
		pEffect->SetPosition(static_cast<TYPE_SECTORPOSITION>(tX + offset.x),
			static_cast<TYPE_SECTORPOSITION>(tY + offset.y));
		pEffect->SetZ(egInfo.z1);
		pEffect->SetStepPixel(egInfo.step);
		pEffect->SetCount(egInfo.count, egInfo.linkCount);
		pEffect->SetDirection(egInfo.direction);
		pEffect->SetPower(egInfo.power);

		const bool bAdd = QueueEffect(std::move(effect));
		if (bAdd)
		{
			pEffect->SetLink(egInfo.nActionInfo, bOK ? nullptr : egInfo.pEffectTarget);
			bOK = true;
		}
		if (bAdd && repeatFrame && maxFrame > 0)
		{
			const int num = std::rand() % maxFrame;
			for (int nf = 0; nf < num; ++nf) pEffect->NextFrame();
		}

		if (est >= EFFECTSPRITETYPE_BLOODY_WALL_1 && est <= EFFECTSPRITETYPE_BLOODY_WALL_3)
			if (++est > EFFECTSPRITETYPE_BLOODY_WALL_3) est = EFFECTSPRITETYPE_BLOODY_WALL_1;

		// Preserve the original blit/repeat policy while refreshing every frame ID.
		if (!ReadSprite(static_cast<TYPE_EFFECTSPRITETYPE>(est), sprite)) return bOK;
		frameID = sprite.frameID;
		if (!ReadMaxFrames(bltType, frameID, maxFrame)) return bOK;
	}
	return bOK;
}
