//----------------------------------------------------------------------
// MAttachEffect.h
//----------------------------------------------------------------------
//
// Effects attached to creatures.
//
// Creature owners refresh position explicitly; Update advances animation.
//
//----------------------------------------------------------------------

#ifndef	__MATTACHEFFECT_H__
#define	__MATTACHEFFECT_H__

#ifdef _MSC_VER
#pragma warning(disable:4786)
#endif

#include "MMovingEffect.h"
#include "MTypeDef.h"
#include <list>
class MCreature;

struct MAttachEffectSprite
{
	BYTE bltType = 0;
	TYPE_FRAMEID frameID = 0;
	int maxFrames = 0;
};

struct MAttachCreaturePosition
{
	TYPE_OBJECTID id = OBJECTID_NULL;
	int x = 0, y = 0, z = 0;
};

// Borrowed world services. ID lookup tries live, fake, then corpse creatures.
// Missing sprite metadata uses the existing unknown-type effect defaults.
struct MAttachEffectHost
{
	bool (*Sprite)(TYPE_EFFECTSPRITETYPE type, MAttachEffectSprite& sprite) = nullptr;
	bool (*LiveCreature)(TYPE_OBJECTID id, MAttachCreaturePosition& position) = nullptr;
	bool (*FakeCreature)(TYPE_OBJECTID id, MAttachCreaturePosition& position) = nullptr;
	bool (*CorpseCreature)(TYPE_OBJECTID id, MAttachCreaturePosition& position) = nullptr;
	bool (*CreaturePosition)(const MCreature* creature, MAttachCreaturePosition& position) = nullptr;
};

class MAttachEffect : public MMovingEffect {
	public :
		MAttachEffect(TYPE_EFFECTSPRITETYPE type, DWORD last, DWORD linkCount=MAX_LINKCOUNT);
		~MAttachEffect();
		static const MAttachEffectHost* SetHost(const MAttachEffectHost* host);

		virtual EFFECT_TYPE		GetEffectType()	const	{ return EFFECT_ATTACH; }

		//--------------------------------------------------------
		// Attachment target
		//--------------------------------------------------------
		void					SetAttachCreatureID(TYPE_OBJECTID id);	// Resolve through the host.
		bool					SetAttachCreature(MCreature* pCreature);		// Copy the creature position.
		TYPE_OBJECTID			GetAttachCreatureID()					{ return m_CreatureID; }

		//--------------------------------------------------------
		// Sprite or color effect
		//--------------------------------------------------------
		bool					IsEffectSprite() const	{ return m_bEffectSprite; }
		bool					IsEffectColor() const	{ return !m_bEffectSprite; }
		void					SetEffectSprite(TYPE_EFFECTSPRITETYPE es)	{ m_EffectSpriteType = es; m_bEffectSprite = true; }
		void					SetEffectColor(WORD colorSet)				{ m_EffectColor = colorSet; m_bEffectSprite = false; }
		
		void					SetEffectColorPart(ADDON part)				{ m_bEffectColorPart = part; }
		ADDON					GetEffectColorPart() const					{ return m_bEffectColorPart; }

		TYPE_EFFECTSPRITETYPE	GetEffectSpriteType() const	{ return m_EffectSpriteType; }
		WORD					GetEffectColor() const		{ return m_EffectColor; }		
		
		//--------------------------------------------------------
		// Advance one update.
		//--------------------------------------------------------
		virtual bool			Update();

		
	protected :
		TYPE_OBJECTID				m_CreatureID;

		bool						m_bEffectSprite;		// Whether the union holds a sprite type.

		union {
			TYPE_EFFECTSPRITETYPE	m_EffectSpriteType;		// Sprite type
			WORD					m_EffectColor;			// Effect color
		};

		ADDON						m_bEffectColorPart;		// Part affected by the color change

	private:
		struct Sprite
		{
			MAttachEffectSprite info;
			bool available = false;
		};
		MAttachEffect(TYPE_EFFECTSPRITETYPE type, DWORD last, DWORD linkCount, const Sprite& sprite);
		static Sprite ReadSprite(TYPE_EFFECTSPRITETYPE type);
		static bool FindCreature(TYPE_OBJECTID id, MAttachCreaturePosition& position);
		static bool ReadCreature(const MCreature* creature, MAttachCreaturePosition& position);
		void ApplyPosition(const MAttachCreaturePosition& position);
		static const MAttachEffectHost* s_pHost;
};

// Attachment list
typedef	std::list<MAttachEffect*>	ATTACHEFFECT_LIST;

#endif

