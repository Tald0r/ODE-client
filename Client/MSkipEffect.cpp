//----------------------------------------------------------------------
// MSkipEffect.cpp
//----------------------------------------------------------------------
#include "Client_PCH.h"
#include "MEffect.h"
#include "MSkipEffect.h"

//----------------------------------------------------------------------
//
// constructor/destructor
//
//----------------------------------------------------------------------

MSkipEffect::MSkipEffect(BYTE bltType)
: MEffect(bltType)
{
	m_nSkipValue = 3;
}

MSkipEffect::~MSkipEffect()
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
MSkipEffect::Update()
{
	if (IsBeforeFrame(m_EndFrame-4))
	{
		if((rand()%m_nSkipValue))
			SetDrawSkip(true);
		else
			SetDrawSkip(false);
		
		NextFrame();
	
		if (m_BltType == BLT_EFFECT)
		{
			RefreshLight();
		}

		return true;
	}
	
	return false;
}