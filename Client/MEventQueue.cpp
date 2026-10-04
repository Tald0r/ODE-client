#include "MEventQueue.h"
#include "DebugLog.h"

void MEventQueue::SetAddGammaRamp(WORD red, WORD green, WORD blue) const
{
	if (m_Host && m_Host->SetAddGammaRamp)
		m_Host->SetAddGammaRamp(red, green, blue);
}

bool MEventQueue::HasEffectStatus(DWORD effect) const
{
	return !m_Host || !m_Host->HasEffectStatus || m_Host->HasEffectStatus(effect);
}

void MEventQueue::StartGDRFade() const
{
	if (m_Host && m_Host->SetFadeStart)
		m_Host->SetFadeStart(31, -1, 1, 0, 0, 0, 4);
}

MEvent::MEvent()
{
	eventID = EVENTID_NULL;
	eventType = EVENTTYPE_NULL;
	eventDelay = -1;
	showTime = -1;
	totalTime = -1;
	eventFlag = 0;
	parameter1 = 0;
	parameter2 = 0;
	parameter3 = 0;
	parameter4 = 0;
}

MEvent::~MEvent()
{
}

DWORD
MEvent::ElapsedMillis() const
{
	return (DWORD)(MonotonicClock::Now() - eventStartTickCount).count();
}

bool
MEvent::IsShowTime() const
{
	if( showTime == -1 )
		return true;

	if( ElapsedMillis() % totalTime < static_cast<DWORD>(showTime) )
		return true;

	return false;
}

MEventQueue::MEventQueue(const MEventHost* host) : m_Host(host)
{
}

MEventQueue::~MEventQueue()
{
	// MEventManager clears before destroying its background images.
	if (!IsEmptyEvent())
		RemoveAllEvent();
}

//--------------------------------------------------
// Register or replace an event.
//--------------------------------------------------
void	MEventQueue::AddEvent(MEvent &event)
{
	event.eventStartTickCount = MonotonicClock::Now();
	m_Events[event.eventID] = event;
	// Preserve the existing refresh after every insertion, including
	// non-fade events: the old condition used bitwise OR.
	{
		const MEvent *tempEvent = GetEventByFlag(EVENTFLAG_FADE_SCREEN);
		if(tempEvent != NULL)
			SetAddGammaRamp((tempEvent->parameter2 >> 16) & 0xff, (tempEvent->parameter2 >> 8) & 0xff, tempEvent->parameter2 & 0xff);
	}
}

//--------------------------------------------------
// Find an event without inserting a missing ID.
//--------------------------------------------------
const MEvent*	MEventQueue::GetEvent(EVENT_ID id)
{
//	return &m_Events[id];
	EVENT_MAP::iterator itr = m_Events.find(id);

	if(itr != m_Events.end())
		return &itr->second;

	return NULL;
}

//--------------------------------------------------
// Check whether an event is registered.
//--------------------------------------------------
bool	MEventQueue::IsEvent(EVENT_ID id)
{
	return (GetEvent(id) != NULL);
}

//--------------------------------------------------
// Remove an event.
//--------------------------------------------------
void	MEventQueue::RemoveEvent(EVENT_ID id)
{
	DEBUG_ADD_FORMAT("[MEventManager] RemoveEvent : %d", id);
	const MEvent *event = GetEvent(id);

	if(event == NULL)
	{
		DEBUG_ADD_WAR("MEventManager] RemoveEvent event == NULL");
		return;
	}

	m_Events.erase(id);

	// Removal has always refreshed (or reset) gamma, even for non-fade events.
	{
		event = GetEventByFlag(EVENTFLAG_FADE_SCREEN);
		if(event == NULL)
			SetAddGammaRamp();
		else
			SetAddGammaRamp((event->parameter2 >> 16) & 0xff, (event->parameter2 >> 8) & 0xff, event->parameter2 & 0xff);
	}
	DEBUG_ADD("[MEventManager] RemoveEvent OK");
}

//--------------------------------------------------
// Remove all events.
//--------------------------------------------------
void	MEventQueue::RemoveAllEvent()
{
	DEBUG_ADD_FORMAT("[MEventManager] RemoveAllEvent Count: %d", m_Events.size());
	EVENT_MAP::iterator itr = m_Events.begin();

	while(itr != m_Events.end())
	{
		EVENT_ID delete_id = itr->second.eventID;
		DEBUG_ADD_FORMAT("[MEventManager] Call RemoveEvent(%d)", delete_id);
		itr++;
		RemoveEvent(delete_id);
	}
	DEBUG_ADD_FORMAT("[MEventManager] RemoveAllEvent OK");

}
//--------------------------------------------------
// Remove events of a given type.
//--------------------------------------------------
void	MEventQueue::RemoveAllEventByType(EVENT_TYPE type)
{
	EVENT_MAP::iterator itr = m_Events.begin();

	while(itr != m_Events.end())
	{
		if(itr->second.eventType == type)
		{
			EVENT_ID delete_id = itr->second.eventID;
			itr++;
			RemoveEvent(delete_id);
		}
		else
			itr++;
	}
}

//--------------------------------------------------
// Count events sharing any requested flag bit.
//--------------------------------------------------
int			MEventQueue::GetEventCountByFlag(DWORD flag)
{
	EVENT_MAP::iterator itr = m_Events.begin();
	int count = 0;

	while(itr != m_Events.end())
	{
		if(itr->second.eventFlag & flag)
		{
			count++;
		}

		itr++;
	}

	return count;
}

//--------------------------------------------------
// Check whether no event shares any requested flag bit.
//--------------------------------------------------
bool			MEventQueue::IsEmptyEventByFlag(DWORD flag)
{
	EVENT_MAP::iterator itr = m_Events.begin();

	while(itr != m_Events.end())
	{
		if(itr->second.eventFlag & flag)
		{
			return false;
		}

		itr++;
	}

	return true;
}

//--------------------------------------------------
// Find the indexed matching event in event-ID order.
//--------------------------------------------------
const MEvent*	MEventQueue::GetEventByFlag(DWORD flag, int count)
{
	EVENT_MAP::iterator itr = m_Events.begin();
	int i = 0;

	while(itr != m_Events.end())
	{
		if(itr->second.eventFlag & flag)
		{
			if(i == count)
				return &itr->second;
			i++;
		}

		itr++;
	}

	return NULL;
}

//--------------------------------------------------
// Process
//--------------------------------------------------
void	MEventQueue::ProcessEvent()
{
	EVENT_MAP::iterator itr = m_Events.begin();

	while(itr != m_Events.end())
	{
		if(itr->second.eventDelay != -1)
		{
			if(itr->second.ElapsedMillis() > static_cast<DWORD>(itr->second.eventDelay))
			{
				EVENT_ID delete_id = itr->second.eventID;
				itr++;
				RemoveEvent(delete_id);
				// After the Gilles de Rais shake expires, begin the dark fade.
				if(delete_id == EVENTID_GDR_PRESENT)
					StartGDRFade();
				continue;
			}
		}
		if(itr->second.eventType == EVENTTYPE_EFFECT)
		{
			if(!HasEffectStatus(itr->second.parameter1))
			{
				EVENT_ID delete_id = itr->second.eventID;
				itr++;
				RemoveEvent(delete_id);
				continue;
			}

		}
		itr++;
	}
}
