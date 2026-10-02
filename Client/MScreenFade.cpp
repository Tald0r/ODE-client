#include "MScreenFade.h"

#include <cstdlib>

void MScreenFade::Start(signed char start, signed char end, signed char step,
	std::uint16_t delay)
{
	step = static_cast<signed char>(std::abs(step));
	m_Value = start;
	m_End = end;
	m_Increment = static_cast<signed char>((start < end) ? step : -step);
	m_Active = true;
	m_Delay = delay;
}

void MScreenFade::Advance(std::uint32_t frame, bool frameChanged)
{
	if (!m_Active)
		return;

	if (!m_HasFrame)
	{
		m_PreviousFrame = frame;
		m_HasFrame = true;
	}

	if (m_Delay)
	{
		const std::uint32_t elapsed = frame - m_PreviousFrame;
		if (elapsed >= m_Delay)
		{
			if (m_End == -1 && m_Value == 1)
			{
				// Gilles de Rais holds at the darkest visible shade for
				// 80 frames; the original comparison is strictly greater.
				if (elapsed > 16 * 5)
					m_Active = false;
			}
			else
			{
				m_PreviousFrame = frame;
				m_Value = static_cast<signed char>(m_Value + m_Increment);
			}
		}
	}
	else if (frameChanged)
	{
		m_Value = static_cast<signed char>(m_Value + m_Increment);
	}

	if (m_Increment > 0)
	{
		if (m_Value > m_End || m_Value > 31)
			m_Active = false;
	}
	else
	{
		if (m_Value < m_End || m_Value < 1)
			m_Active = false;
	}
}
