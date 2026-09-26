#ifndef GUARD_RH_INTERNAL_H
#define GUARD_RH_INTERNAL_H
// Shared helpers for the romhack randomizer modules (src/rh_*.c).
#include "rh.h"

#define S (&gSaveBlock3Ptr->rhSettings)

// Independent random streams.
enum RhSalt
{
    SALT_WILD = 1, SALT_WILD_AREA, SALT_WILD_THEME, SALT_TRAINER, SALT_TRAINER_THEME, SALT_STARTER,
    SALT_STATIC, SALT_TRADE, SALT_TYPE1, SALT_TYPE2, SALT_ABILITY, SALT_STATS, SALT_STAT_PERM,
    SALT_LEARNSET, SALT_EVO, SALT_TM, SALT_TM_COMPAT, SALT_TUTOR_COMPAT, SALT_MOVE_POWER, SALT_MOVE_ACC,
    SALT_MOVE_PP, SALT_MOVE_TYPE, SALT_MOVE_CAT, SALT_FIELD_ITEM, SALT_SHOP, SALT_HELD_ITEM, SALT_GLOBAL_PERM,
    SALT_TYPE_CHART, SALT_MOVE_NAME, SALT_TRAINER_NAME, SALT_TRAINER_CLASS, SALT_NICKNAME, SALT_OT, SALT_IV,
    SALT_TUTOR, SALT_PICKUP, SALT_SPECIAL_SHOP, SALT_EXTRA_MON, SALT_RIVAL, SALT_LEAGUE, SALT_ADDED_STATS,
    SALT_EVO_LEVEL, SALT_WILD_ITEM, SALT_CATCH_ALL, SALT_BATTLE_STYLE, SALT_STARTER_ITEM, SALT_STARTER_TYPE,
    SALT_EVO_RANK, SALT_STATIC_SLOT, SALT_TRADE_SLOT, SALT_TRAINER_SHINY, SALT_EGG_MOVES, SALT_MOVE_CAT2,
};

u32 RH_Hash(u32 salt, u32 a, u32 b);
u32 RH_Permute(u32 salt, u32 x, u32 n);

// Species pool --------------------------------------------------------------
struct RhPoolMon;
extern const struct RhPoolMon *RH_PoolAt(u32 index);
u32 RH_PoolCount(void);
u16 RH_PoolSpecies(u32 index);
s32 RH_PoolIndexOf(u16 species);
bool32 RH_PoolAllowed(u32 index);            // respects the Pokemon Pool setting (never megas)
u16 RH_FamilyRoot(u16 species);
u16 RH_TraitRoot(u16 species);             // family root, but split evolutions start their own branch
u16 RH_PreEvo(u16 species);                  // direct pre-evolution (vanilla), SPECIES_NONE if basic
struct RhMoveData { u8 power, accuracy, pp, type, category; };
const u8 *RH_GenBaseStats(u16 species, u32 gen);             // HP, Atk, Def, Speed, SpAtk, SpDef or NULL
bool32 RH_GenMoveData(u16 move, u32 gen, struct RhMoveData *out);
u32 RH_StatsGen(void);                       // generation whose base stats are used (1-9)
u32 RH_MovesGen(void);                       // generation whose move data is used (1-9)
bool32 RH_IsLegendary(u16 species);
u32 RH_VanillaBST(u16 species);
u32 RH_SettingsHash(void);                   // hash of all settings (cache key)
void RH_InvalidateSettingsHash(void);        // after changing settings within a frame
bool32 RH_EvoItemsForSale(void);             // evolution sellers keep every evolution item
u32 RH_SpeciesStage(u16 species);            // 1 = basic
u32 RH_SpeciesChain(u16 species);            // stages in its vanilla line
bool32 RH_SpeciesHasType(u16 species, u8 type);
u8 RH_RandomMonType(u32 hash);
u32 RH_TypeCount(u32 typeIndex);             // allowed Pokemon with that type (cached)
u8 RH_RandomPopulatedType(u32 hash, u32 minCount);
u32 RH_TypeIndexOf(u8 type);
extern const u8 gRhMonTypes[18];

#define RH_MAX_EXCLUDE 12
struct RhFilter
{
    u16 minBst, maxBst;                      // 0 = no limit
    u8 type;                                 // TYPE_NONE = any
    u8 primaryType;                          // TYPE_NONE = any; replacement's first type must match
    u8 legend;                               // 0 any, 1 no legendaries, 2 only legendaries
    u8 stage;                                // 0 any, else exact evolution stage
    bool8 threeStageBasic;
    bool8 allowMegas;
    bool8 monoType;
    bool8 megasOnly;                         // with allowMegas: only Mega Evolutions
    bool8 allowVariants;                     // regional/alternate forms even if the pool setting excludes them
    bool8 noLeagueReserved;                  // never the Elite Four's unique Pokemon (never relaxed)
    u8 excludeCount;
    u16 exclude[RH_MAX_EXCLUDE];
    bool32 (*extra)(u16 species);            // optional extra predicate
};
u16 RH_PickWithFilter(const struct RhFilter *f, u32 hash);
u16 RH_PickRanked(const struct RhFilter *f, u32 rank, u32 salt);    // rank-th accepted in a keyed order
bool32 RH_FilterAccepts(const struct RhFilter *f, u32 poolIndex);
u16 RH_PickSpecies(struct RhFilter *f, u32 hash, u16 similarTo);   // relaxes constraints when nothing fits
u16 RH_PickSpeciesNearBst(struct RhFilter *f, u32 hash, u32 bst);  // same, around a base stat total (0 = any)
void RH_FilterExclude(struct RhFilter *f, u16 species);

u16 RH_EvolveOnce(u16 species, u32 salt);
u16 RH_EvolveTimes(u16 species, u32 times);
u16 RH_FullyEvolve(u16 species);
s32 RH_ApplyPercent(s32 value, s16 percent);   // value * (100 + percent) / 100, min 1

bool32 RH_IsLeagueReserved(u16 species);
enum Species RH_WildSpeciesAt(u8 mapGroup, u8 mapNum, const struct WildPokemonInfo *info, u32 slot, enum WildPokemonArea area, u32 level);
u16 RH_StarterForSlot(u32 slot);
s32 RH_StarterFamily(u16 species, u32 *stage);
u32 RH_BuildSpecialShop(u32 list, u16 *out, u32 max);   // stock of a special shop (RH_SPECIAL_*), returns count
#ifndef RELEASE
u32 RH_DebugStaticCount(void);
u16 RH_DebugStaticOriginal(u32 i);
u16 RH_DebugStaticResult(u32 i);
#endif
u32 RH_InGameTradeCount(void);
void RH_InGameTradeSpecies(u32 id, u16 *given, u16 *requested);     // "Pokemon League Has Unique Pokemon"

// Items --------------------------------------------------------------------
bool32 RH_ItemIsBad(u16 item);
bool32 RH_ItemIsOverpowered(u16 item);
bool32 RH_ItemBanned(u16 item);              // global bans (Lucky Egg when "Ban Lucky Egg")
u16 RH_RandomHeldItem(u32 hash, bool32 banBad, bool32 consumableOnly);

#endif
