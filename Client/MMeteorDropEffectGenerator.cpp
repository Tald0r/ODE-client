// MMeteorDropEffectGenerator.cpp
#include "Client_PCH.h"
#include "MMeteorDropEffectGenerator.h"
#include "MLinearEffect.h"
#include "MEventQueue.h"
#include <utility>

const MMeteorDropEffectHost* MMeteorDropEffectGenerator::s_pHost = nullptr;

const MMeteorDropEffectHost* MMeteorDropEffectGenerator::SetHost(const MMeteorDropEffectHost* host)
{
	const auto* previous = s_pHost;
	s_pHost = host;
	return previous;
}

bool MMeteorDropEffectGenerator::ReadSprite(TYPE_EFFECTSPRITETYPE type, MFixedZoneEffectSprite& sprite)
{
	sprite = {};
	return s_pHost && s_pHost->Sprite && s_pHost->Sprite(type, sprite);
}

bool MMeteorDropEffectGenerator::QueueEffect(std::unique_ptr<MEffect> effect)
{
	return s_pHost && s_pHost->Queue && s_pHost->Queue(std::move(effect));
}

void MMeteorDropEffectGenerator::AddEvent(MEvent& event)
{
	if (s_pHost && s_pHost->AddEvent) s_pHost->AddEvent(event);
}

bool MMeteorDropEffectGenerator::Generate(const EFFECTGENERATOR_INFO& egInfo)
{
	MFixedZoneEffectSprite sprite;
	if (!ReadSprite(egInfo.effectSpriteType, sprite)) return false;

	auto effect = std::make_unique<MLinearEffect>(sprite.bltType);
	effect->SetFrameID(sprite.frameID, static_cast<BYTE>(sprite.maxFrames));
	// The meteor starts to the right of and above its destination.
	effect->SetPixelPosition(egInfo.x1 + 100, egInfo.y1, egInfo.z1 + 400);
	effect->SetDirection(egInfo.direction);
	// Target selection retains its calculated facing, overriding the input.
	effect->SetTarget(egInfo.x1, egInfo.y1, egInfo.z1, egInfo.step);
	effect->SetCount(egInfo.count, egInfo.linkCount);
	effect->SetPower(egInfo.power);

	MLinearEffect* queued = effect.get();
	if (!QueueEffect(std::move(effect))) return false;
	queued->SetLink(egInfo.nActionInfo, egInfo.pEffectTarget);

	MEvent event;
	event.eventID = EVENTID_METEOR;
	event.eventType = EVENTTYPE_ZONE;
	event.eventDelay = 1000;
	event.eventFlag = EVENTFLAG_FADE_SCREEN;
	event.parameter2 = 30 << 16;
	AddEvent(event);
	return true;
}
