//----------------------------------------------------------------------
// MLinearEffect.h
//----------------------------------------------------------------------

#ifndef	__MLINEAREFFECT_H__
#define	__MLINEAREFFECT_H__

#include "MMovingEffect.h"
#include "LinearEffectMotion.h"

class MLinearEffect : public MMovingEffect, protected LinearEffectMotion {
	public :
		MLinearEffect(BYTE bltType);
		~MLinearEffect();

		virtual EFFECT_TYPE		GetEffectType()	const	{ return EFFECT_LINEAR; }

		//--------------------------------------------------------
		// 새로운 목표 설정
		//--------------------------------------------------------
		virtual void		SetTarget(int x, int y, int z, WORD stepPixel);
		
		//--------------------------------------------------------
		// 한 번의 Update에 호출될 함수..
		//--------------------------------------------------------
		virtual bool		Update();
};

#endif

