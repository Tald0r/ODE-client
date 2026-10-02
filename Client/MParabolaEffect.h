//----------------------------------------------------------------------
// MParabolaEffect.h
//----------------------------------------------------------------------
// 포물선을 그리는 Effect
//----------------------------------------------------------------------

#ifndef	__MPARABOLAEFFECT_H__
#define	__MPARABOLAEFFECT_H__


#include "MLinearEffect.h"
#include "ParabolaEffectMotion.h"

#define	PI	3.14159265


class MParabolaEffect : public MLinearEffect {
	public :
		MParabolaEffect(BYTE bltType);
		~MParabolaEffect(); 

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
		ParabolaEffectMotion m_Motion;
};


#endif

