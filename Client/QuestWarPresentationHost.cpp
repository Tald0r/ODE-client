#include "Client_PCH.h"
#include "QuestWarPresentationHost.h"

namespace QuestWarPresentation {
namespace {
const Host* s_Host = nullptr;
}

const Host* SetHost(const Host* host)
{
	const Host* previous = s_Host;
	s_Host = host;
	return previous;
}

void RunQuestStatus(GCQuestStatus* pPacket)
{
	if (s_Host && s_Host->RunQuestStatus) s_Host->RunQuestStatus(pPacket);
}

void RunQuestList(GCSelectQuestID* pPacket)
{
	if (s_Host && s_Host->RunQuestList) s_Host->RunQuestList(pPacket);
}

void RunWarList(GCWarScheduleList* pPacket)
{
	if (s_Host && s_Host->RunWarList) s_Host->RunWarList(pPacket);
}

void SetCTFStatus(const MonotonicClock::TimePoint& endTime, int flag_s, int flag_v, int flag_o)
{
	if (s_Host && s_Host->SetCTFStatus) s_Host->SetCTFStatus(endTime, flag_s, flag_v, flag_o);
}

void SetBloodBibleStatus(GCBloodBibleStatus* pPacket)
{
	if (s_Host && s_Host->SetBloodBibleStatus) s_Host->SetBloodBibleStatus(pPacket);
}

}
