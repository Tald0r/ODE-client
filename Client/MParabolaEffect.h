//----------------------------------------------------------------------
// MParabolaEffect.h
//----------------------------------------------------------------------
// 포물선을 그리는 Effect
//----------------------------------------------------------------------

#ifndef	__MPARABOLAEFFECT_H__
#define	__MPARABOLAEFFECT_H__


#include "MLinearEffect.h"
#include "ParabolaEffectMotion.h"
#include <memory>

#define	PI	3.14159265


struct MParabolaSmokeSprite
{
	BYTE bltType = 0;
	TYPE_FRAMEID frameID = 0;
	int maxFrames = 0;
};

// Borrowed services. Missing metadata skips smoke creation; missing queue or
// impact entries discard that output. QueueSmoke receives exclusive ownership.
struct MParabolaEffectHost
{
	bool (*SmokeSprite)(MParabolaSmokeSprite& sprite) = nullptr;
	void (*QueueSmoke)(std::unique_ptr<MEffect> smoke, DWORD waitCount) = nullptr;
	void (*CannonadeImpact)(TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y) = nullptr;
};

class MParabolaEffect : public MLinearEffect {
	public :
		MParabolaEffect(BYTE bltType);
		~MParabolaEffect(); 
		static const MParabolaEffectHost* SetHost(const MParabolaEffectHost* host);

		virtual EFFECT_TYPE		GetEffectType()	const	{ return EFFECT_PARABOLA; }

		//--------------------------------------------------------
		// 새로운 목표 설정
		//--------------------------------------------------------
		virtual void		SetTarget(int x, int y, int z, WORD stepPixel);
		
		//--------------------------------------------------------
		// 한 번의 Update에 호출될 함수..
		//--------------------------------------------------------
		virtual bool		Update();

		void				MakeCannonadeSmoke();
		void				SetTargetTile(int x, int y) {	m_TargetTileX = x; m_TargetTileY = y;	}
	protected :	
		TYPE_SECTORPOSITION m_TargetTileX;
		TYPE_SECTORPOSITION m_TargetTileY;
	private:
		static bool ReadSmokeSprite(MParabolaSmokeSprite& sprite);
		static void QueueSmoke(std::unique_ptr<MEffect> smoke, DWORD waitCount);
		static void CannonadeImpact(TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y);
		static const MParabolaEffectHost* s_pHost;
		ParabolaEffectMotion m_Motion;
};


#endif

