//----------------------------------------------------------------------
// MScreenEffect.cpp
//----------------------------------------------------------------------
#include "Client_PCH.h"
#include "MEffect.h"
#include "MScreenEffect.h"

#include <cmath>
#include <limits>

namespace {
int ScreenCoordinate(float offset, int basis)
{
	if (std::isnan(offset)) return basis;
	// Truncate the offset first, as the original projection did. Keep the
	// sum wide until after adding the basis so opposite signs can cancel.
	const double coordinate = std::trunc(static_cast<double>(offset)) + basis;
	if (coordinate >= (std::numeric_limits<int>::max)())
		return (std::numeric_limits<int>::max)();
	if (coordinate <= (std::numeric_limits<int>::min)())
		return (std::numeric_limits<int>::min)();
	return static_cast<int>(coordinate);
}
} // namespace

//----------------------------------------------------------------------
// static
//----------------------------------------------------------------------
int		MScreenEffect::m_ScreenBasisX	= 0;
int		MScreenEffect::m_ScreenBasisY	= 0;


//----------------------------------------------------------------------
//
// constructor/destructor
//
//----------------------------------------------------------------------

MScreenEffect::MScreenEffect(BYTE bltType)
: MEffect(bltType)
{
	//m_ObjectType	= TYPE_EFFECT;

	//m_EffectType	= EFFECT_SCREEN;

}

MScreenEffect::~MScreenEffect()
{
}

//----------------------------------------------------------------------
//
// member functions
//
//----------------------------------------------------------------------

//----------------------------------------------------------------------
// Set Screen Basis
//----------------------------------------------------------------------
// 화면 기준점
//----------------------------------------------------------------------
void		
MScreenEffect::SetScreenBasis(int bx, int by)
{
	m_ScreenBasisX = bx;
	m_ScreenBasisY = by;
}

//----------------------------------------------------------------------
// Set Screen Position
//----------------------------------------------------------------------
// 화면에서의 좌표
//----------------------------------------------------------------------
void		
MScreenEffect::SetScreenPosition(int x, int y)
{
	// 좌표 보정값을 저장한다.
	m_PixelX = static_cast<float>(static_cast<double>(x) - m_ScreenBasisX);
	m_PixelY = static_cast<float>(static_cast<double>(y) - m_ScreenBasisY);
}

int MScreenEffect::GetScreenX()
{
	return ScreenCoordinate(m_PixelX, m_ScreenBasisX);
}

int MScreenEffect::GetScreenY()
{
	return ScreenCoordinate(m_PixelY, m_ScreenBasisY);
}

//----------------------------------------------------------------------
// Update
//----------------------------------------------------------------------
// m_Count가 0일때까지 -1 해주면서 Frame을 바꾼다.
//----------------------------------------------------------------------
bool
MScreenEffect::Update()
{
	if (!IsEnd())
	{
		// Frame을 바꿔준다.
		NextFrame();
		
		if (m_BltType == BLT_EFFECT)
		{
			RefreshLight();
		}
		
		return true;
	}
	
	return false;
}