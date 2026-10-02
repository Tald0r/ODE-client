//-----------------------------------------------------------------------------
// MScreenEffectManager.h
//-----------------------------------------------------------------------------

#ifndef __MSCREENEFFECTMANAGER_H__
#define __MSCREENEFFECTMANAGER_H__

#include "MEffectManager.h"
#include "Platform.h"

// Borrowed services. A missing clock skips links while an effect is active;
// missing generation services leave target cleanup to the owning effect.
struct MScreenEffectManagerHost
{
	DWORD (*CurrentFrame)() = nullptr;
	// Receives a borrowed effect and may transfer its owned target.
	void (*GenerateNext)(MEffect* effect) = nullptr;
};

class MScreenEffectManager : public MEffectManager {
	public :
		MScreenEffectManager() {}
		~MScreenEffectManager() {}
		static const MScreenEffectManagerHost* SetHost(const MScreenEffectManagerHost* host);

		void		Update();

	private:
		static bool ReadCurrentFrame(DWORD& frame);
		static void GenerateNext(MEffect* effect);
		static const MScreenEffectManagerHost* s_pHost;
};

#endif


