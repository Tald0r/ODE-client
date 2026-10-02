//----------------------------------------------------------------------
// MEventQueue.h - event records and lifetime rules, independent of drawing.
//----------------------------------------------------------------------
#ifndef __MEVENT_QUEUE_H__
#define __MEVENT_QUEUE_H__

#include "MonotonicClock.h"
#include <map>
#include <vector>

#define EVENTFLAG_SHOW_STRING				0x00000001 // Show a string.
#define EVENTFLAG_SHOW_DELAY					0x00000002 // Show the remaining delay.
#define EVENTFLAG_SHOW_DELAY_STRING			0x00000003 // Show a string with the delay.
#define EVENTFLAG_SHAKE_SCREEN				0x00000004 // Shake by parameter3.
#define EVENTFLAG_FADE_SCREEN				0x00000008 // Apply the colour in parameter2.
#define EVENTFLAG_NOT_DRAW_BACKGROUND		0x00000010
#define EVENTFLAG_EVENT_BACKGROUND			0x00000020
#define EVENTFLAG_ONLY_EVENT_BACKGROUND		0x00000030
#define EVENTFLAG_QUEST_INFO					0x00000040
#define EVENTFLAG_NOT_DRAW_UI				0x00000080
#define EVENTFLAG_NOT_DRAW_CREATURE			0x00000100
#define EVENTFLAG_NOT_DRAW_INFORMATION		0x00000200
#define EVENTFLAG_NOT_DRAW_CREATURE_SHADOW	0x00000400
#define EVENTFLAG_NOT_DRAW_ITEM				0x00000800
#define EVENTFLAG_NOT_DRAW_EFFECT				0x00001000
#define EVENTFLAG_NOT_DRAW_MOUSE_POINTER		0x00002000
#define EVENTFLAG_DENY_INPUT_MOUSE			0x00004000
#define EVENTFLAG_DENY_INPUT_KEYBOARD			0x00008000
#define EVENTFLAG_DENY_INPUT					0x0000c000
#define EVENTFLAG_NOT_FADE_SCREEN				0x00010000
#define EVENTFLAG_NOT_PLAY_SOUND				0x00020000
#define EVENTFLAG_CLOUD_BACKGROUND			0x00040000

enum EVENT_ID
{
	EVENTID_NULL,
	EVENTID_HALLUCINATION,
	EVENTID_KICK_OUT_FROM_ZONE,
	EVENTID_CONTINUAL_GROUND_ATTACK,
	EVENTID_COMBAT_MASTER,
	EVENTID_METEOR,
	EVENTID_METEOR_SHAKE,
	EVENTID_PREMIUM_HALF,
	EVENTID_TAX_CHANGE, // The shop supplies its tax ratio directly now.
	EVENTID_LOGOUT,
	EVENTID_LOVECHAIN,
	EVENTID_FORCE_LOGOUT_BY_PREMIUM,
	EVENTID_MONSTER_KILL_QUEST,
	EVENTID_OUSTERS_FIN,
	EVENTID_QUEST_FIN,
	EVENTID_POUR_ITEM,
	EVENTID_RESURRECT,
	EVENTID_GDR_PRESENT,
	EVENTID_BG_CLOUD,
	EVENTID_WAR_EFFECT,
	EVENTID_ADVANCEMENT_QUEST_ENDING,
	EVENTID_MAX,
};

enum EVENT_TYPE
{
	EVENTTYPE_NULL,
	EVENTTYPE_ZONE,   // Removed when the zone changes.
	EVENTTYPE_EFFECT, // Removed when the effect in parameter1 ends.
	EVENTTYPE_MAX
};

class MEvent
{
public:
	MEvent();
	~MEvent();

	EVENT_ID				eventID;
	EVENT_TYPE				eventType;
	MonotonicClock::TimePoint eventStartTickCount;
	int						eventDelay; // Milliseconds; -1 means no expiry.
	int						showTime;   // Visible milliseconds per period; -1 means always.
	int						totalTime;  // Positive period when showTime is enabled.
	DWORD					eventFlag;
	DWORD					parameter1; // Effect status for EVENTTYPE_EFFECT.
	DWORD					parameter2; // RGB for EVENTFLAG_FADE_SCREEN.
	DWORD					parameter3; // Shake strength.
	DWORD					parameter4; // Background image ID.
	std::vector<int>		m_StringsID; // Game-string IDs.

	bool IsShowTime() const;
	DWORD ElapsedMillis() const;
};

// Borrowed for the queue's lifetime, including destruction. The executable
// supplies rendering and player actions; absent actions are skipped. Without
// an effect query, effect events keep their explicit lifetime. Time uses the
// existing MonotonicClock seam, so tests need neither a player nor a renderer.
struct MEventHost
{
	void (*SetAddGammaRamp)(WORD red, WORD green, WORD blue) = nullptr;
	bool (*HasEffectStatus)(DWORD effect) = nullptr;
	void (*SetFadeStart)(signed char start, signed char end, signed char step,
		BYTE red, BYTE green, BYTE blue, WORD delay) = nullptr;
};

class MEventQueue
{
public:
	explicit MEventQueue(const MEventHost* host = nullptr);
	~MEventQueue();

	void			ProcessEvent();
	void			AddEvent(MEvent& event);
	bool			IsEmptyEvent() const { return m_Events.empty(); }
	int				GetEventCount() const { return static_cast<int>(m_Events.size()); }
	const MEvent*	GetEvent(EVENT_ID id);
	bool			IsEmptyEventByFlag(DWORD flag);
	const MEvent*	GetEventByFlag(DWORD flag, int count = 0);
	int				GetEventCountByFlag(DWORD flag);
	bool			IsEvent(EVENT_ID id);
	void			RemoveEvent(EVENT_ID id);
	void			RemoveAllEvent();
	void			RemoveAllEventByType(EVENT_TYPE type);

	typedef std::map<EVENT_ID, MEvent> EVENT_MAP;

protected:
	EVENT_MAP m_Events;

private:
	void SetAddGammaRamp(WORD red = 0, WORD green = 0, WORD blue = 0) const;
	bool HasEffectStatus(DWORD effect) const;
	void StartGDRFade() const;
	const MEventHost* m_Host;
};

#endif
