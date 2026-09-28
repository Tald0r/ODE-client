//----------------------------------------------------------------------
// OrbitEffectPolicy.h
//----------------------------------------------------------------------
// Which orbit effects MAttachCreatureOrbitEffectGenerator starts at the
// orbit step of the creature's current orbit effect, and which replace
// the creature's attached effects, kept free of game state so that the
// choice can be tested on its own.
//----------------------------------------------------------------------

#ifndef __ORBITEFFECTPOLICY_H__
#define __ORBITEFFECTPOLICY_H__

//----------------------------------------------------------------------
// ConsultsPreviousOrbitStep
//----------------------------------------------------------------------
// Whether a new orbit effect of spriteType continues from the orbit step
// of the creature's first attached effect. hasAttachedEffect says
// whether the creature has an attached effect at all.
//----------------------------------------------------------------------
bool	ConsultsPreviousOrbitStep(unsigned short spriteType, bool hasAttachedEffect);

//----------------------------------------------------------------------
// ClearsBeforeAttach
//----------------------------------------------------------------------
// Whether attaching an orbit effect of spriteType first removes the
// creature's attached effects.
//----------------------------------------------------------------------
bool	ClearsBeforeAttach(unsigned short spriteType);

#endif
