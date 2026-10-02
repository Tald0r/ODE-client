//-----------------------------------------------------------------------------
// MScreenEffectManager.cpp
//-----------------------------------------------------------------------------
#include "Client_PCH.h"
#include "MScreenEffectManager.h"
#include "MEffect.h"

const MScreenEffectManagerHost* MScreenEffectManager::s_pHost = nullptr;

const MScreenEffectManagerHost* MScreenEffectManager::SetHost(const MScreenEffectManagerHost* host)
{
	const auto* previous = s_pHost;
	s_pHost = host;
	return previous;
}

bool MScreenEffectManager::ReadCurrentFrame(DWORD& frame)
{
	frame = 0;
	if (!s_pHost || !s_pHost->CurrentFrame) return false;
	frame = s_pHost->CurrentFrame();
	return true;
}

void MScreenEffectManager::GenerateNext(MEffect* effect)
{
	if (s_pHost && s_pHost->GenerateNext) s_pHost->GenerateNext(effect);
}

// Update only the effects present at entry. Generated effects are added
// at the front and remain untouched until the next manager update.
void MScreenEffectManager::Update()
{
	EFFECT_LIST::iterator iEffect = m_listEffect.begin();
	const int count = static_cast<int>(m_listEffect.size());

	for (int i = 0; i < count; ++i)
	{
		MEffect* pEffect = *iEffect;
		if (pEffect->Update())
		{
			++iEffect;
			DWORD now;
			if (ReadCurrentFrame(now) && now >= pEffect->GetEndLinkFrame()
				&& pEffect->GetLinkSize() != 0)
			{
				// The generator may transfer the target; this effect stays alive.
				GenerateNext(pEffect);
			}
		}
		else
		{
			const auto iTemp = iEffect++;
			if (pEffect->GetLinkSize() != 0) GenerateNext(pEffect);
			delete pEffect;
			m_listEffect.erase(iTemp);
		}
	}
}
