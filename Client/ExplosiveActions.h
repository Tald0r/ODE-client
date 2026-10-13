#pragma once

// The action a mine or a bomb item of this type sets off: one of the five
// mines or five bombs, by item type; -1 for any other type.
int GetMineActionInfo(int itemType);
int GetBombActionInfo(int itemType);
