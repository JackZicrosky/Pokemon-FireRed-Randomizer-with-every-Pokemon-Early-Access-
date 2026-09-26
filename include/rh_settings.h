#ifndef GUARD_RH_SETTINGS_H
#define GUARD_RH_SETTINGS_H
// Randomizer settings chosen at New Game (modelled on Universal Pokemon Randomizer FVX). Stored in SaveBlock3.
// Every field is a small enum/bool/number so the options menu can edit it generically.

enum { RH_POOL_GEN1, RH_POOL_GEN1_3, RH_POOL_ALL, RH_POOL_ALL_FORMS };

#define RH_SEED_TEXT_LENGTH 10

struct RhSettings
{
    u32 seed;                     // hash of seedText
    u8 seedText[RH_SEED_TEXT_LENGTH + 1];
    u8 enabled;
    u8 version;
    // ---- General ----
    u8 mechanicsGen;              // 1-9 (battle mechanics generation); split + Fairy always on
    u8 speciesPool;               // RH_POOL_*
    u8 nuzlocke;                  // gives the infinite Rare Candy + party-heal key items

    // ---- Pokemon Traits ----
    u8 baseStats;                 // 0 unchanged, 1 shuffle, 2 random
    u8 baseStatsFollowEvos;
    u8 baseStatsRandomAdded;      // "Randomize Added Stats on Evolution"
    u8 updateBaseStatsGen;        // 0 off, else generation 1-9
    u8 expCurve;                  // 0 off, else 1 Medium Fast, 2 Medium Slow, 3 Fast, 4 Slow, 5 Erratic, 6 Fluctuating
    u8 expCurveWho;               // 0 Legendaries: Slow, 1 Strong Legendaries: Slow, 2 All Pokemon
    u8 types;                     // 0 unchanged, 1 random (follow evos), 2 random (completely)
    u8 forceDualTypes;
    u8 abilities;                 // 0 unchanged, 1 random
    u8 allowWonderGuard;
    u8 combineDuplicateAbilities;
    u8 ensureTwoAbilities;
    u8 abilitiesFollowEvos;
    u8 banTrapAbilities;
    u8 banNegativeAbilities;
    u8 banBadAbilities;
    u8 evolutions;                // 0 unchanged, 1 random, 2 random every level
    u8 evoSimilarStrength;
    u8 evoSameTyping;
    u8 evoLimitThreeStages;
    u8 evoNoConvergence;
    u8 evoForceChange;
    u8 evoForceGrowth;
    u8 evoChangeImpossible;
    u8 evoMakeEasier;             // 0 off, else max evolution level (30-55)
    u8 evoEstimatedLevels;
    u8 evoRemoveTimeBased;

    // ---- Type Effectiveness ----
    u8 typeChart;                 // 0 unchanged, 1 random, 2 random (balanced), 3 keep type identities, 4 inverse
    u8 updateTypeChart;           // gen 9 matchups (default on)
    u8 inverseRandomImmunities;

    // ---- Starters, Statics & Trades ----
    u8 starters;                  // 0 unchanged, 1 custom, 2 random (completely), 3 random basic 2-evo, 4 random incl. regional
    u16 customStarters[3];
    u8 starterNoLegends;
    u8 starterHeldItems;
    u8 starterBanBadItems;
    u8 starterBstMinOn, starterBstMaxOn;
    u16 starterBstMin, starterBstMax;
    u8 starterTypes;              // 0 none, 1 fire/water/grass, 2 any type triangle, 3 unique, 4 single type
    u8 starterSingleType;         // TYPE_* (0 = random)
    u8 starterNoDualTypes;
    u8 statics;                   // 0 unchanged, 1 swap legends & standards, 2 random completely, 3 random similar strength
    u8 staticRandomize600;
    u8 staticLimitMainGameLegends;
    u8 staticFixMusic;
    u8 staticLevelModOn;
    s16 staticLevelMod;           // -100..150 %
    u8 trades;                    // 0 unchanged, 1 given only, 2 both
    u8 tradeNicknames, tradeOTs, tradeIVs, tradeItems;

    // ---- Moves & Movesets ----
    u8 movePower, moveAccuracy, movePP, moveType, moveNames;
    u8 updateMovesGen;            // 0 off, else generation 1-9
    u8 movesets;                  // 0 unchanged, 1 random prefer type, 2 random completely, 3 metronome only
    u8 guaranteedLevel1On;
    u8 guaranteedLevel1Moves;     // 2-4
    u8 reorderDamagingMoves;
    u8 movesetNoGameBreaking;
    u8 movesetGoodDamagingOn;
    u8 movesetGoodDamaging;       // 0-100 %

    // ---- Foe Pokemon ----
    u8 trainers;                  // 0 unchanged, 1 random, 2 random even, 3 type themed, 4 type themed (E4/gyms), 5 keep themed, 6 keep themes or primary
    u8 betterMovesets[3];         // boss, important, regular
    u8 additionalMons[3];         // 0 = off, else 1-5
    u8 heldItemsFor[3];
    u8 heldConsumableOnly, heldSensible, heldHighestOnly;
    u8 diverseTypes[3];
    u8 battleStyle;               // 0 unchanged, 1 random, 2 single style
    u8 battleStyleDoubles;        // for single style: 0 singles, 1 doubles
    u8 rivalCarriesTeam;
    u8 trainerSimilarStrength;
    u8 trainerAvoidDuplicates;
    u8 trainerWeightTypes;
    u8 trainerLocalPokemon;
    u8 trainerNoLegends;
    u8 noEarlyWonderGuard;
    u8 leagueUnique;              // 0 off, 1-2
    u8 randomTrainerNames;
    u8 randomTrainerClassNames;
    u8 trainersEvolveOn;
    u8 trainersEvolveLevel;       // latest fully evolved level (30-65)
    u8 trainerLevelModOn;
    s16 trainerLevelMod;          // -100..150 %

    // ---- Wild Pokemon ----
    u8 wild;                      // randomize wild pokemon on/off
    u8 wildZone;                  // 0 1 in whole game, 1 per named location, 2 per encounter set, 3 maximum possible, 4 completely random always
    u8 wildSplitEncounterTypes;
    u8 wildTypeRestriction;       // 0 none, 1 randomize zone themes, 2 keep primary type
    u8 wildKeepThemes;
    u8 wildEvoRestriction;        // 0 none, 1 only basic, 2 same evolution stage
    u8 wildKeepRelations;
    u8 wildNoLegends;
    u8 wildCatchRateOn;
    u8 wildCatchRate;             // 1-5
    u8 wildHeldItems;
    u8 wildBanBadItems;
    u8 wildCatchEmAll;
    u8 wildSimilarStrength;
    u8 wildBalanceLowLevel;
    u8 wildMegas;                 // permanent Mega Evolutions can appear
    u8 wildLevelModOn;
    s16 wildLevelMod;             // -100..150 %

    // ---- TMs & Tutors ----
    u8 tmMoves;                   // 0 unchanged, 1 random
    u8 tmNoGameBreaking, tmKeepFieldMoves, tmGoodDamagingOn, tmGoodDamaging;
    u8 tmCompat;                  // 0 unchanged, 1 random prefer type, 2 random completely, 3 full
    u8 tmLevelupSanity, tmCompatFollowEvos, fullHMCompat;
    u8 tutorMoves;                // 0 unchanged, 1 random
    u8 tutorNoGameBreaking, tutorKeepFieldMoves, tutorGoodDamagingOn, tutorGoodDamaging;
    u8 tutorCompat;               // same values as tmCompat

    // ---- Items ----
    u8 fieldItems;                // 0 unchanged, 1 shuffle, 2 random, 3 random even distribution
    u8 fieldBanBad;
    u8 shopItems;                 // 0 unchanged, 1 shuffle, 2 random
    u8 shopBanBad, shopBanRegular, shopBanOverpowered, shopGuaranteeEvo, shopGuaranteeX, shopBalancePrices;
    u8 shopSpecial;               // randomize the romhack's special shops (evolution/herb/Celadon counters)
    u8 pickupItems;               // 0 unchanged, 1 random
    u8 pickupBanBad;

    // ---- Custom Player Graphics ----
    u8 playerGraphics;            // 0 = default, else index into the graphics pack table

    // ---- Misc. Tweaks ----
    u8 instantText, runIndoors, randomPcPotion, nationalDexAtStart, fastEggs, lowerCaseNames,
       randomCatchTutorial, banLuckyEgg, balanceStaticLevels, runWithoutShoes, reusableTMs, forgettableTMs, noEVs;

    u8 padding[8];

    // ---- Added in v0.3 (appended so v0.2 saves keep working; they read as 0 = off) ----
    u8 playerGraphicsReplace;     // 0 = auto (the character picked in the intro), 1 = boy, 2 = girl
    u8 moveCategory;              // randomize physical / special of damaging moves
    u8 tutorLevelupSanity, tutorCompatFollowEvos;
    u8 evolutionMovesForAll;      // every evolved Pokemon learns a move when it evolves
    u8 starterAllowAltFormes, staticAllowAltFormes, wildAllowAltFormes, trainerAllowAltFormes;
    u8 trainerRandomShiny;        // 1/256 shiny trainer Pokemon
    u8 shopAddCheapRareCandy;     // FVX "Add Cheap Rare Candies"
    u8 reserved[21];
};

#define RH_SETTINGS_VERSION 2
#endif
