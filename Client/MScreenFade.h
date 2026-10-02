//----------------------------------------------------------------------
// MScreenFade.h - screen-fade progression without rendering or game globals.
//----------------------------------------------------------------------
#ifndef __MSCREEN_FADE_H__
#define __MSCREEN_FADE_H__

#include <cstdint>

class MScreenFade
{
public:
	// Values use the renderer's 0..31 scale; -1 is the cutscene end marker.
	// Zero step holds a fixed shade. Delay zero advances on logic ticks;
	// otherwise delay counts frames between steps, with no catch-up steps.
	void Start(signed char start, signed char end, signed char step,
		std::uint16_t delay = 0);
	void Stop() { m_Active = false; }
	bool IsActive() const { return m_Active; }
	signed char Value() const { return m_Value; }

	// Draw Value() first, then advance. Call only for an unsuppressed draw.
	// The first active draw starts the frame clock; subsequent starts retain
	// its phase, as the original DrawFade did. Each view owns its own clock.
	void Advance(std::uint32_t frame, bool frameChanged);

private:
	// Explicit signedness keeps the -1 marker identical on every ABI.
	signed char m_Value = 0;
	signed char m_End = 0;
	signed char m_Increment = 0;
	bool m_Active = false;
	std::uint16_t m_Delay = 0;
	std::uint32_t m_PreviousFrame = 0;
	bool m_HasFrame = false;
};

#endif
