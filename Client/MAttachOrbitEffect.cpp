#include "Client_PCH.h"
#include "MAttachOrbitEffect.h"

void MAttachOrbitEffect::InitOrbitPosition()
{
	EffectOrbit::InitializePositions();
}

MAttachOrbitEffect::MAttachOrbitEffect(TYPE_EFFECTSPRITETYPE type, DWORD last, int orbit_type, int orbit_step, DWORD linkCount)
	: MAttachEffect(type, last, linkCount), m_Orbit(orbit_type, orbit_step)
{
}

MAttachOrbitEffect::~MAttachOrbitEffect() = default;

bool MAttachOrbitEffect::Update()
{
	const bool active = MAttachEffect::Update();
	m_Orbit.Update(active);
	return active;
}

int MAttachOrbitEffect::GetPixelX() const
{
	return PixelCoordinate(m_PixelX, m_Orbit.GetPosition().x);
}

int MAttachOrbitEffect::GetPixelY() const
{
	return PixelCoordinate(m_PixelY, m_Orbit.GetPosition().y);
}
