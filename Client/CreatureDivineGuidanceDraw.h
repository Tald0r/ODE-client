#pragma once

// A creature under divine guidance drawn again as a darkened, turning
// silhouette (task 4.248, gameview's since then; MTopView's members until
// then). pPoint is where the creature is drawn; each silhouette is two
// darkened passes of frame 30 of its being drained, a direction apart,
// turning with the frame clock.

#include "../basic/Platform.h"
#include "CreatureDrawPacks.h"

class MCreature;

// The silhouette for the creature's race (a slayer, an ousters or else a
// vampire) and class (advanced or not); body is a vampire's frame set.
void	DrawDivineGuidanceCharacter( const CreatureDrawPacks& packs, POINT *pPoint, MCreature* pCreature, int direction, int body, bool bSlayer, bool bOusters );

void	DrawDivineGuidanceSlayerCharacter( const CreatureDrawPacks& packs, POINT *pPoint, MCreature* pCreature, int direction );
void	DrawDivineGuidanceVampireCharacter( const CreatureDrawPacks& packs, POINT *pPoint, MCreature* pCreature, int direction, int body );
void	DrawDivineGuidanceOustersCharacter( const CreatureDrawPacks& packs, POINT *pPoint, MCreature* pCreature, int direction );
void	DrawDivineGuidanceAdvancementClassSlayerCharacter( const CreatureDrawPacks& packs, POINT *pPoint, MCreature* pCreature, int direction );
void	DrawDivineGuidanceAdvancementClassVampireCharacter( const CreatureDrawPacks& packs, POINT *pPoint, MCreature* pCreature, int direction, int body );
void	DrawDivineGuidanceAdvancementClassOustersCharacter( const CreatureDrawPacks& packs, POINT *pPoint, MCreature* pCreature, int direction );
