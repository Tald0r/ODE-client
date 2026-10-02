//----------------------------------------------------------------------
// MMovingEffect.cpp
//----------------------------------------------------------------------
#include "Client_PCH.h"
#include "MEffect.h"
#include "MMovingEffect.h"

//----------------------------------------------------------------------
//
// constructor/destructor
//
//----------------------------------------------------------------------

MMovingEffect::MMovingEffect(BYTE bltType)
: MEffect(bltType)
{
	//m_ObjectType	= TYPE_EFFECT;

	//m_EffectType	= EFFECT_MOVING;

}

MMovingEffect::~MMovingEffect()
{
}

//----------------------------------------------------------------------
//
// member functions
//
//----------------------------------------------------------------------

//----------------------------------------------------------------------
// Update
//----------------------------------------------------------------------
// m_Count가 0일때까지 -1 해주면서 Frame을 바꾼다.
//----------------------------------------------------------------------
bool
MMovingEffect::Update()
{
	if (!IsEnd())
	{
		// Frame을 바꿔준다.
		NextFrame();
		
		if (m_BltType == BLT_EFFECT)
		{
			RefreshLight();
		}

		// Sector 좌표 설정
		AffectPosition();
		
		return true;
	}
	
	return false;
}