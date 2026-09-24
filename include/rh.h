#ifndef GUARD_RH_H
#define GUARD_RH_H
// Romhack hooks.
#include "wild_encounter.h"

#define RH_SAFARI_GEN_OFF 0
#define RH_SAFARI_GEN_ALL 10

// Called right before a wild mon is created. Returns the species to actually use.
enum Species RH_ModifyWildSpecies(enum Species species, enum WildPokemonArea area);
#endif
