#pragma once

// A creature drawn with an effect (task 4.250, gameview's since then;
// MTopView's members until then): its parts fading out to both sides,
// moving fast, invisible or wiping in, its weapon leaving afterimages,
// and the slayers' pets' and installing slayers' turrets. pPoint is where
// the creature is drawn; each part's screen rect is added to the
// creature's (a pet's turret adds none; an installing turret sets it).
// The ...ForACSlayer forms are the advanced slayer's.

#include "../basic/Platform.h"
#include "CreatureDrawPacks.h"

class MCreature;

void	DrawFadeOut( const CreatureDrawPacks& packs, POINT *pPoint, MCreature* pCreature, int action, int direction, int frame );
void	DrawFadeOutForACSlayer( const CreatureDrawPacks& packs, POINT *pPoint, MCreature* pCreature, int action, int direction, int frame );
void	DrawFastMove( const CreatureDrawPacks& packs, POINT* pPoint, MCreature* pCreature, int action, int direction, int frame );
void	DrawFastMoveForACSlayer( const CreatureDrawPacks& packs, POINT* pPoint, MCreature* pCreature, int action, int direction, int frame );
void	DrawInvisible( const CreatureDrawPacks& packs, POINT* pPoint, MCreature* pCreature, int action, int direction, int frame );
void	DrawInvisibleForACSlayer( const CreatureDrawPacks& packs, POINT* pPoint, MCreature* pCreature, int action, int direction, int frame );
void	DrawWeaponFadeOut( const CreatureDrawPacks& packs, POINT* pPoint, MCreature* pCreature, int action, int direction, int frame );
void	DrawWeaponFadeOutForACSlayer( const CreatureDrawPacks& packs, POINT* pPoint, MCreature* pCreature, int action, int direction, int frame );

// A slayer's pet's turret, of frame set body, facing its turret direction.
void	DrawCentauroTurret( const CreatureDrawPacks& packs, POINT* pPoint, MCreature* pCreature, int action, int direction, int frame , int body);
// An installing slayer's turret; the first frame index sets the
// creature's screen rect.
void	DrawInstallTurret( const CreatureDrawPacks& packs, POINT *pPoint, MCreature* pCreature, int action, int direction, int frame, int frameindex );
