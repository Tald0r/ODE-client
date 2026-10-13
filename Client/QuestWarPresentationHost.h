#pragma once

#include "MonotonicClock.h"

class GCBloodBibleStatus;
class GCQuestStatus;
class GCSelectQuestID;
class GCWarScheduleList;

namespace QuestWarPresentation {

// Quest and war status replies only present what they receive. A missing
// service skips that presentation; a packet is borrowed for the call.
struct Host
{
	void (*RunQuestStatus)(GCQuestStatus* pPacket) = nullptr;
	void (*RunQuestList)(GCSelectQuestID* pPacket) = nullptr;
	void (*RunWarList)(GCWarScheduleList* pPacket) = nullptr;
	void (*SetCTFStatus)(const MonotonicClock::TimePoint& endTime, int flag_s, int flag_v, int flag_o) = nullptr;
	void (*SetBloodBibleStatus)(GCBloodBibleStatus* pPacket) = nullptr;
};

// Borrowed until replaced. Every call reads the current host again.
const Host* SetHost(const Host* host);
void RunQuestStatus(GCQuestStatus* pPacket);
void RunQuestList(GCSelectQuestID* pPacket);
void RunWarList(GCWarScheduleList* pPacket);
void SetCTFStatus(const MonotonicClock::TimePoint& endTime, int flag_s, int flag_v, int flag_o);
void SetBloodBibleStatus(GCBloodBibleStatus* pPacket);

}
