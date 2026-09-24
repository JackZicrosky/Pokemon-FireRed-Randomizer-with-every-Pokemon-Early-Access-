#ifndef GUARD_RH_SETTINGS_H
#define GUARD_RH_SETTINGS_H
// Randomizer settings chosen at New Game. Stored in SaveBlock3.
// Every field is a small enum/bool so the options menu can edit them generically.

enum { RH_POOL_GEN1, RH_POOL_GEN1_3, RH_POOL_ALL, RH_POOL_ALL_FORMS };

struct RhSettings
{
    u32 seed;
    u8 enabled;
    u8 version;               // settings layout version
    // Pokemon traits
    u8 speciesPool;           // RH_POOL_*
    u8 baseStats;             // 0 unchanged, 1 shuffle, 2 random (BST kept)
    u8 baseStatsFollowEvos;
    u8 types;                 // 0 unchanged, 1 random (follow evolutions), 2 completely random
    u8 abilities;             // 0 unchanged, 1 random
    u8 abilitiesFollowEvos;
    u8 banWonderGuard;
    u8 banTrapAbilities;
    u8 banBadAbilities;
    u8 evolutions;            // 0 unchanged, 1 random, 2 random similar strength
    u8 evoSameType;
    // Starters, statics, trades
    u8 starters;              // 0 unchanged, 1 random, 2 random basic with two evolutions
    u8 statics;               // 0 unchanged, 1 swap (legendary for legendary), 2 completely random
    u8 trades;                // 0 unchanged, 1 randomize given, 2 randomize given + requested
    // Moves
    u8 movePower, moveAccuracy, movePP, moveType, moveCategory;
    u8 movesets;              // 0 unchanged, 1 random (prefer same type), 2 completely random, 3 metronome only
    u8 goodDamaging;          // 0 off, 1 25%, 2 50%, 3 75% damaging moves
    u8 banBrokenMoves;
    // Foe Pokemon
    u8 trainers;              // 0 unchanged, 1 random, 2 similar strength, 3 type themed, 4 keep gym/elite type themes
    u8 rivalStarter;
    u8 trainerNoLegends;
    u8 trainerForceEvolved;   // 0 off, 1 lv 30+, 2 lv 40+, 3 lv 50+
    u8 trainerLevel;          // index into level modifier table (5 = +0%)
    u8 trainerItems;
    // Wild Pokemon
    u8 wild;                  // 0 unchanged, 1 random every encounter, 2 area 1-to-1, 3 global 1-to-1
    u8 wildSimilar;
    u8 wildTypeThemed;
    u8 wildNoLegends;
    u8 wildLevel;             // index into level modifier table (5 = +0%)
    u8 wildCatchRate;         // 0 unchanged, 1 min 25%, 2 min 50%, 3 guaranteed-ish
    // TMs / tutors
    u8 tmMoves;               // 0 unchanged, 1 random (HMs untouched)
    u8 tmCompat;              // 0 unchanged, 1 random (prefer same type), 2 completely random, 3 full
    u8 tutorCompat;           // same as tmCompat
    // Items
    u8 fieldItems;            // 0 unchanged, 1 shuffle, 2 random
    u8 banBadItems;
    u8 shopItems;             // 0 unchanged, 1 random (keep essentials)
    u8 padding[6];
};

#define RH_SETTINGS_VERSION 1
#endif
