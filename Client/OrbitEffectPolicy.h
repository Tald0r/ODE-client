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
// of the creature's first attached effect: true for the four summoned
// elemental types, and only when hasAttachedEffect says the creature has
// an attached effect to read. The caller still checks that the effect
// is an orbit effect.
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
