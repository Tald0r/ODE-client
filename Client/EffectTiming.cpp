#include "Client_PCH.h"
#include "EffectTiming.h"

EffectTiming::EffectTiming()
{
	m_EndFrame = 0;
	m_EndLinkFrame = 0;
	m_DelayFrame = 0;
	m_dwWaitFrame = 0;
}

void EffectTiming::SetCount(DWORD now, DWORD duration, DWORD linkCount)
{
	m_EndFrame = now + duration - 1;
	m_EndLinkFrame = linkCount == MAX_LINKCOUNT ? m_EndFrame : now + linkCount - 1;
}

void EffectTiming::SetAttachedCount(DWORD now, DWORD duration, DWORD linkCount)
{
	SetCount(now, duration, linkCount);
	if (duration == 0xFFFF)
	{
		m_EndFrame = 0xFFFFFFFF;
		if (linkCount == MAX_LINKCOUNT) m_EndLinkFrame = m_EndFrame;
	}
}

bool EffectTiming::IsEnd(DWORD now) const
{
	return now >= m_EndFrame;
}

void EffectTiming::SetDelayFrame(DWORD now, DWORD frames)
{
	m_DelayFrame = now + frames;
}

bool EffectTiming::IsDelayFrame(DWORD now) const
{
	return now < m_DelayFrame;
}

void EffectTiming::SetWaitFrame(DWORD now, DWORD frames)
{
	m_dwWaitFrame = now + frames;
}

bool EffectTiming::IsWaitFrame(DWORD now) const
{
	return now < m_dwWaitFrame;
}
