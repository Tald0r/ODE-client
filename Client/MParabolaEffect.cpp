//----------------------------------------------------------------------
// MParabolaEffect.cpp
//----------------------------------------------------------------------
#include "Client_PCH.h"
#include "Client.h"
#include "MTopView.h"
#include "MLinearEffect.h"
#include "MParabolaEffect.h"
#include "EffectSpriteTypeDef.h"
#include "MEffectSpriteTypeTable.h"
#include "PacketFunction.h"
#include "SkillDef.h"
//----------------------------------------------------------------------
// 
// constructor/destructor
//
//----------------------------------------------------------------------

MParabolaEffect::MParabolaEffect(BYTE bltType)
: MLinearEffect(bltType)
{
	m_TargetTileX = 0;
	m_TargetTileY = 0;
}

MParabolaEffect::~MParabolaEffect()
{
}

//----------------------------------------------------------------------
//
// member functions
//
//----------------------------------------------------------------------

//----------------------------------------------------------------------
// 목표를 설정한다.
//----------------------------------------------------------------------
void		
MParabolaEffect::SetTarget(int x, int y, int z, WORD speed)
{
	// 목표 설정..
	MLinearEffect::SetTarget(x, y, z, speed);

	//--------------------------------------------------
	// Grenade는 매 순간마다 Z축의 높이가 달라진다.
	//--------------------------------------------------
	m_Motion.Reset(*this, speed);
}
void
MParabolaEffect::MakeCannonadeSmoke()
{
	MEffect*	pEffect;
	BLT_TYPE		bltType = (*g_pEffectSpriteTypeTable)[EFFECTSPRITETYPE_CANNONADE_SMOKE].BltType;
	TYPE_FRAMEID	frameID	= (*g_pEffectSpriteTypeTable)[EFFECTSPRITETYPE_CANNONADE_SMOKE].FrameID;
	int maxFrame = g_pTopView->GetMaxEffectFrame(bltType, frameID);
	//---------------------------------------------
	// Effect 생성
	//---------------------------------------------
	pEffect = new MEffect(bltType);
			
	pEffect->SetFrameID( frameID, maxFrame );	

	pEffect->SetPixelPosition( static_cast<int>(m_PixelX), static_cast<int>(m_PixelY), static_cast<int>(m_PixelZ));
	pEffect->SetZ(static_cast<int>(m_PixelZ));			
//	pEffect->SetStepPixel(egInfo.step);		// 실제로 움직이지는 않지만, 다음 Effect를 위해서 대입해준다.
	pEffect->SetCount( 9 );			// 지속되는 Frame
			
	// 방향 설정
	pEffect->SetDirection( GetDirection());
	pEffect->SetMulti(true);
	g_pZone->AddEffect( pEffect,10);
	return;
}

//----------------------------------------------------------------------
// Move
//----------------------------------------------------------------------
// 매 순간마다 StepX~Z가 달라진다.
//----------------------------------------------------------------------
bool
MParabolaEffect::Update()
{	
	if (g_CurrentFrame < m_EndFrame)
	{
		m_Motion.Advance(*this, m_PixelX, m_PixelY, m_PixelZ, m_StepPixel);

		if(GetActionInfo() == SKILL_CANNONADE)
			MakeCannonadeSmoke();
		//------------------------------------------
		// 다 움직인 경우를 생각해봐야 한다.
		//------------------------------------------
		if (m_Motion.FinishStep(*this, m_PixelX, m_PixelY, m_PixelZ, m_StepPixel))
		{
			//------------------------------------------
			// 더 움직일 필요가 없는 경우이다.			
			//------------------------------------------
			m_EndFrame = 0;

		//	if(GetFrameID() == EFFECTSPRITETYPE_CANNONADE_BALL)
			if(GetActionInfo() == SKILL_CANNONADE)
			{
				ExecuteActionInfoFromMainNode(RESULT_SKILL_GUN_SHOT_GUIDANCE_BOMB,m_TargetTileX, m_TargetTileY, 0,0,	0,	
						m_TargetTileX, m_TargetTileY, 0, 1000, NULL, false);		

			}
			return false;
		}

		//--------------------------------
		// Sector 좌표를 맞춘다.
		//--------------------------------
		AffectPosition();

		//--------------------------------
		// Frame을 바꿔준다.
		//--------------------------------
		NextFrame();

		if (m_BltType == BLT_EFFECT)
		{
			m_Light = g_pTopView->m_EffectAlphaFPK[m_FrameID][m_Direction][m_CurrentFrame].GetLight();
		}

		//--------------------------------
		// Counter를 하나 줄인다.
		//--------------------------------
		//m_Count--;

		return true;
	}

	// 끝~

	return false;

}
