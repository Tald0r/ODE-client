#pragma once

// A creature's shadow (task 4.249, gameview's since then; MTopView's
// members until then). pPoint is where the creature is drawn; the caller
// locks and unlocks the surface. bUserBlendingShadow is the player's
// option (UserOption::BlendingShadow): worn parts' shadows are blended
// rather than solid.

#include "../basic/Platform.h"
#include "CreatureDrawPacks.h"

class MCreature;

// The shadow of a creature by its type, its state and its race and class,
// or none.
void	DrawCreatureShadow( const CreatureDrawPacks& packs, POINT* pPoint, MCreature* pCreature, bool bUserBlendingShadow );

void	DrawShadowSlayerCharacter( const CreatureDrawPacks& packs, POINT *pPoint, MCreature* pCreature, int action, int direction, int frame, bool bUserBlendingShadow );
void	DrawShadowVampireCharacter( const CreatureDrawPacks& packs, POINT *pPoint, MCreature* pCreature, int action, int direction, int frame, int body, bool bBlendingShadow , bool bSlayerPet_ShowTurret );
void	DrawShadowOustersCharacter( const CreatureDrawPacks& packs, POINT *pPoint, MCreature* pCreature, int action, int direction, int frame, bool bUserBlendingShadow );
void	DrawShadowAdvancementClassSlayerCharacter( const CreatureDrawPacks& packs, POINT *pPoint, MCreature* pCreature, int action, int direction, int frame, bool bUserBlendingShadow );
void	DrawShadowAdvancementClassVampireCharacter( const CreatureDrawPacks& packs, POINT *pPoint, MCreature* pCreature, int action, int direction, int frame, int body, bool bBlendingShadow , bool bSlayerPet_ShowTurret );
void	DrawShadowAdvancementClassOustersCharacter( const CreatureDrawPacks& packs, POINT *pPoint, MCreature* pCreature, int action, int direction, int frame, bool bUserBlendingShadow );
