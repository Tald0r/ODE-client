//----------------------------------------------------------------------
// AffectModifyInfo.h
//----------------------------------------------------------------------
// Applies a ModifyInfo packet's status changes to a status array. Every
// packet that carries a ModifyInfo (GCModifyInformation, GCOtherModifyInfo
// and the skill, attack and item replies deriving from it) is applied
// through this one function, kept free of game state so that it can be
// tested on its own.
//----------------------------------------------------------------------

#ifndef __AFFECTMODIFYINFO_H__
#define __AFFECTMODIFYINFO_H__

class MStatus;
class ModifyInfo;

//----------------------------------------------------------------------
// AffectModifyInfo
//----------------------------------------------------------------------
// Pops every short entry, then every long entry, off pInfo and hands
// each (type, value) to pStatus->SetStatus in wire order, so a type sent
// in both lists ends with its long value. The type is the wire byte,
// passed on unchecked: MCreature's and MPlayer's SetStatus refuse one at
// or past MAX_MODIFY, the base MStatus::SetStatus does not. pInfo is left
// with both counts at 0.
//----------------------------------------------------------------------
extern void		AffectModifyInfo(MStatus* pStatus, ModifyInfo* pInfo);

#endif
