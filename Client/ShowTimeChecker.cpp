#include "Client_PCH.h"
#include "ShowTimeChecker.h"
#include <cstdlib>

bool ShowTimeChecker::IsShowTime(MonotonicClock::TimePoint now, std::optional<BYTE> hour) const
{
	return IsShowHour(hour) && (Loop || now >= NextPlayTime);
}

bool ShowTimeChecker::IsShowHour(std::optional<BYTE> hour) const
{
	if (!hour) return false;
	if (StartHour <= EndHour) return *hour >= StartHour && *hour <= EndHour;
	return *hour >= StartHour || *hour <= EndHour;
}

void ShowTimeChecker::SetNextShowTime(MonotonicClock::TimePoint now, const Random& random)
{
	const DWORD delayGap = MaxDelay - MinDelay;
	if (delayGap == 0)
	{
		NextPlayTime = now;
	}
	else
	{
		const unsigned draw = random ? random() : static_cast<unsigned>(std::rand());
		NextPlayTime = now + MonotonicClock::Millis(MinDelay + draw % delayGap);
	}
}
