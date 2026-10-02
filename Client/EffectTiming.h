#ifndef EFFECT_TIMING_H
#define EFFECT_TIMING_H

#include "Platform.h"
#include "MTypeDef.h"

class EffectTiming
{
public:
	EffectTiming();
	void SetCount(DWORD now, DWORD duration, DWORD linkCount = MAX_LINKCOUNT);
	// Only attached effects interpret duration 0xFFFF as an infinite deadline.
	void SetAttachedCount(DWORD now, DWORD duration, DWORD linkCount = MAX_LINKCOUNT);
	DWORD GetEndFrame() const { return m_EndFrame; }
	DWORD GetEndLinkFrame() const { return m_EndLinkFrame; }
	bool IsEnd(DWORD now) const;
	void SetDelayFrame(DWORD now, DWORD frames);
	bool IsDelayFrame(DWORD now) const;
	void SetWaitFrame(DWORD now, DWORD frames);
	bool IsWaitFrame(DWORD now) const;

protected:
	DWORD m_EndFrame;
	DWORD m_EndLinkFrame;
	DWORD m_DelayFrame;
	DWORD m_dwWaitFrame;
};

#endif
