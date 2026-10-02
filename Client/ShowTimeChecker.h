//-----------------------------------------------------------------------------
// ShowTimeChecker.h
//-----------------------------------------------------------------------------

#ifndef __SHOWTIMECHECKER_H__
#define __SHOWTIMECHECKER_H__

#ifdef PLATFORM_WINDOWS
#include <Windows.h>
#else
#include "../basic/Platform.h"
#endif

#include <fstream>
#include <functional>
#include <optional>

#include "MonotonicClock.h"

class ShowTimeChecker {
	public :
		ShowTimeChecker();
		~ShowTimeChecker();

		// Missing game time suppresses playback, including loops. Hour ranges
		// are inclusive and may cross midnight; hour bytes are not normalized.
		bool			IsShowTime(MonotonicClock::TimePoint now, std::optional<BYTE> hour) const;
		bool			IsShowHour(std::optional<BYTE> hour) const;

		using Random = std::function<unsigned()>;
		// Equal delay bounds retain the legacy immediate deadline without a
		// random draw. Otherwise the upper bound is exclusive. An absent
		// random source uses rand(); a throwing source leaves the deadline.
		void			SetNextShowTime(MonotonicClock::TimePoint now, const Random& random = {});

		//---------------------------------------------------------------
		// File I/O
		//---------------------------------------------------------------
		void			SaveToFile(std::ofstream& file);
		void			LoadFromFile(std::ifstream& file);

	public :
		// 반복적인가?
		bool			Loop;

		// MinDelay ~ MaxDelay 사이에는 꼭 한 번
		DWORD			MinDelay;
		DWORD			MaxDelay;
		
		// StartHour부터 EndHour 사이에만 (0~24시면 종일?)
		BYTE			StartHour;
		BYTE			EndHour;

		MonotonicClock::TimePoint	NextPlayTime;	// the next show
};

#endif


