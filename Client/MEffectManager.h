//-----------------------------------------------------------------------------
// MEffectManager.h
//-----------------------------------------------------------------------------

#ifndef __MEFFECTMANAGER_H__
#define __MEFFECTMANAGER_H__

#ifdef _MSC_VER
#pragma warning(disable:4786)
#endif

#include <list>
class MEffect;

class MEffectManager {
	public :
		// Effect list
		typedef std::list<MEffect*>						EFFECT_LIST;

	public :
		MEffectManager();
		virtual ~MEffectManager();
		MEffectManager(const MEffectManager&) = delete;
		MEffectManager& operator=(const MEffectManager&) = delete;

		//------------------------------------------------------
		// Release
		//------------------------------------------------------
		virtual void		Release();

		//------------------------------------------------------
		// Add
		//------------------------------------------------------
		// Own each pointer once; null and already owned pointers are no-ops.
		virtual void		AddEffect(MEffect* pEffect);

		//------------------------------------------------------
		// Update
		//------------------------------------------------------
		virtual void		Update() = 0;

		//------------------------------------------------------
		// list
		//------------------------------------------------------
		int					GetSize() const			{ return static_cast<int>(m_listEffect.size()); }
		EFFECT_LIST::const_iterator GetEffects()	{ return m_listEffect.begin(); }

	protected :
		EFFECT_LIST			m_listEffect;
};

#endif

