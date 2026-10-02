//----------------------------------------------------------------------
// MGuidanceEffect.h
//----------------------------------------------------------------------

#ifndef	__MGUIDANCEEFFECT_H__
#define	__MGUIDANCEEFFECT_H__

#include "MLinearEffect.h"
#include "MTypeDef.h"

// Borrowed live-creature lookup. A missing host/entry or creature stops tracing.
struct MGuidanceEffectHost
{
	bool (*CreaturePosition)(TYPE_OBJECTID id, int& x, int& y, int& z) = nullptr;
};

class MGuidanceEffect : public MLinearEffect {
	public :
		MGuidanceEffect(BYTE bltType);
		~MGuidanceEffect();
		static const MGuidanceEffectHost* SetHost(const MGuidanceEffectHost* host);
		
		virtual EFFECT_TYPE		GetEffectType()	const	{ return EFFECT_GUIDANCE; }

		//--------------------------------------------------------
		// 목표 설정
		//--------------------------------------------------------
		void				SetTraceCreatureID(TYPE_OBJECTID id);
		TYPE_OBJECTID		GetTraceCreatureID()					{ return m_CreatureID; }
		
		//--------------------------------------------------------
		// 한 번의 Update에 호출될 함수..
		//--------------------------------------------------------
		virtual bool		Update();

	protected :
		virtual bool		TraceCreature();		// 추적 좌표 설정
		// Resolve this trace id; a missing target clears its id and lifetime.
		bool TraceCreaturePosition(int& x, int& y, int& z);

	protected :
		TYPE_OBJECTID	m_CreatureID;

	private :
		static bool ReadCreaturePosition(TYPE_OBJECTID id, int& x, int& y, int& z);
		static const MGuidanceEffectHost* s_pHost;
};

#endif