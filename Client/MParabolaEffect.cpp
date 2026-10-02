//----------------------------------------------------------------------
// MParabolaEffect.cpp
//----------------------------------------------------------------------
#include "Client_PCH.h"
#include "MParabolaEffect.h"
#include "SkillDef.h"
#include <utility>

const MParabolaEffectHost* MParabolaEffect::s_pHost = nullptr;

const MParabolaEffectHost* MParabolaEffect::SetHost(const MParabolaEffectHost* host)
{
	const auto* previous = s_pHost;
	s_pHost = host;
	return previous;
}

bool MParabolaEffect::ReadSmokeSprite(MParabolaSmokeSprite& sprite)
{
	sprite = {};
	return s_pHost && s_pHost->SmokeSprite && s_pHost->SmokeSprite(sprite);
}

void MParabolaEffect::QueueSmoke(std::unique_ptr<MEffect> smoke, DWORD waitCount)
{
	if (s_pHost && s_pHost->QueueSmoke)
		s_pHost->QueueSmoke(std::move(smoke), waitCount);
}

void MParabolaEffect::CannonadeImpact(TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y)
{
	if (s_pHost && s_pHost->CannonadeImpact) s_pHost->CannonadeImpact(x, y);
}

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
	MParabolaSmokeSprite sprite;
	if (!ReadSmokeSprite(sprite)) return;
	auto effect = std::make_unique<MEffect>(sprite.bltType);
	effect->SetFrameID(sprite.frameID, static_cast<BYTE>(sprite.maxFrames));
	effect->SetPixelPosition(static_cast<int>(m_PixelX), static_cast<int>(m_PixelY), static_cast<int>(m_PixelZ));
	effect->SetZ(static_cast<int>(m_PixelZ));
	effect->SetCount(9);
	effect->SetDirection(GetDirection());
	effect->SetMulti(true);
	QueueSmoke(std::move(effect), 10);
}

//----------------------------------------------------------------------
// Move
//----------------------------------------------------------------------
// 매 순간마다 StepX~Z가 달라진다.
//----------------------------------------------------------------------
bool
MParabolaEffect::Update()
{	
	if (!IsEnd())
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
				CannonadeImpact(m_TargetTileX, m_TargetTileY);

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
			RefreshLight();
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
