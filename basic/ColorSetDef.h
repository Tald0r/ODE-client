#pragma once

// The index sprites' colour sets: 33 base colours, each in 15 variants.
// CIndexSprite draws with them; the colour-set table (CUsingColorSetTable)
// marks which a sprite may use.
#define	MAX_COLORSET_SEED			33	// the base colours
#define	MAX_COLORSET_SEED_MODIFY	15	// each base colour's variants
#define	MAX_COLORSET				495 // MAX_COLORSET_SEED * MAX_COLORSET_SEED_MODIFY
