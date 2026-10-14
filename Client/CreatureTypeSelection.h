#ifndef CLIENT_CREATURE_TYPE_SELECTION_H
#define CLIENT_CREATURE_TYPE_SELECTION_H

// A vampire's creature type for its shape: in its own shape, the frame its
// coat (a vampire coat's item type) gives a man or a woman; as a bat, a
// wolf or a werewolf, that form's; SHAPE_MAX for any other shape.
int GetVampireCreatureType(int shape, bool bMale, int coatType);

// An ouster's creature type, whatever its coat.
int GetOustersCreatureType(int coatType);

#endif
