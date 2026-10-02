//----------------------------------------------------------------------
// MAttachEffect.cpp
//----------------------------------------------------------------------
#include "Client_PCH.h"
#include "MAttachEffect.h"
#include "DebugLog.h"

const MAttachEffectHost* MAttachEffect::s_pHost = nullptr;

const MAttachEffectHost* MAttachEffect::SetHost(const MAttachEffectHost* host)
{
	const auto* previous = s_pHost;
	s_pHost = host;
	return previous;
}

MAttachEffect::Sprite MAttachEffect::ReadSprite(TYPE_EFFECTSPRITETYPE type)
{
	Sprite sprite;
	sprite.available = s_pHost && s_pHost->Sprite && s_pHost->Sprite(type, sprite.info);
	return sprite;
}

bool MAttachEffect::FindCreature(TYPE_OBJECTID id, MAttachCreaturePosition& position)
{
	position = {};
	if (s_pHost && s_pHost->LiveCreature && s_pHost->LiveCreature(id, position)) return true;
	position = {};
	if (s_pHost && s_pHost->FakeCreature && s_pHost->FakeCreature(id, position)) return true;
	position = {};
	return s_pHost && s_pHost->CorpseCreature && s_pHost->CorpseCreature(id, position);
}

bool MAttachEffect::ReadCreature(const MCreature* creature, MAttachCreaturePosition& position)
{
	position = {};
	return creature && s_pHost && s_pHost->CreaturePosition &&
		s_pHost->CreaturePosition(creature, position);
}

//----------------------------------------------------------------------
// 
// constructor/destructor
//
//----------------------------------------------------------------------

MAttachEffect::MAttachEffect(TYPE_EFFECTSPRITETYPE type, DWORD last, DWORD linkCount)
: MAttachEffect(type, last, linkCount, ReadSprite(type))
{
}

MAttachEffect::MAttachEffect(TYPE_EFFECTSPRITETYPE type, DWORD last, DWORD linkCount, const Sprite& sprite)
: MMovingEffect(sprite.available ? sprite.info.bltType : static_cast<BYTE>(BLT_EFFECT))
{
	m_CreatureID	= OBJECTID_NULL;

	m_bEffectSprite = true;

	m_EffectSpriteType	= type;
	
	SetAttachedLifetime(last, linkCount);
	
	// ADDON_NULL applies a color change to the whole creature.
	m_bEffectColorPart = ADDON_NULL;

	// Unknown sprite types retain the default frame and no light.
	if (sprite.available)
	{
		TYPE_FRAMEID frameID = sprite.info.frameID;
		BYTE maxFrame = static_cast<BYTE>(sprite.info.maxFrames);

		LOG_INFO("[EFFECT CREATE] type=%d, FrameID=%d, BltType=%d, maxFrame=%d",
			type, frameID, (int)sprite.info.bltType, (int)maxFrame);

		SetFrameID( frameID, maxFrame );

		// Alpha effects refresh their default light after SetFrameID.
		if (m_BltType == BLT_EFFECT)
		{
			RefreshLight();
		}
		else
		{
			m_Light = 0;
		}
	}
	else
	{
		SetFrameID( 0, 1 );
		m_Light = 0;
	}
}

MAttachEffect::~MAttachEffect()
{
}

//----------------------------------------------------------------------
//
// member functions
//
//----------------------------------------------------------------------

//----------------------------------------------------------------------
// Resolve an attachment by object ID
//----------------------------------------------------------------------
void				
MAttachEffect::SetAttachCreatureID(TYPE_OBJECTID id)
{ 
	MAttachCreaturePosition position;
	if (!FindCreature(id, position))
	{
		m_EndFrame = 0;
		return;
	}
	ApplyPosition(position);
}

//----------------------------------------------------------------------
// Apply the supplied creature position
//----------------------------------------------------------------------
bool
MAttachEffect::SetAttachCreature(MCreature* pCreature)
{	
	MAttachCreaturePosition position;
	if (!ReadCreature(pCreature, position))
	{
		m_EndFrame = 0;
		return false;
	}
	ApplyPosition(position);
	return true;
}

void MAttachEffect::ApplyPosition(const MAttachCreaturePosition& position)
{
	m_CreatureID = position.id;
	m_PixelX = static_cast<float>(position.x);
	m_PixelY = static_cast<float>(position.y);
	m_PixelZ = static_cast<float>(position.z);
	AffectPosition();
}

//----------------------------------------------------------------------
// Update
//----------------------------------------------------------------------
bool
MAttachEffect::Update()
{	
	if (!IsEnd())
	{	
		NextFrame();

		if (m_BltType == BLT_EFFECT)
		{
			RefreshLight();
		}

		return true;
	}

	return false;
}
