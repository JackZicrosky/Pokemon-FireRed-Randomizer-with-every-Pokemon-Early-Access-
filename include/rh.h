#ifndef GUARD_RH_H
#define GUARD_RH_H
// Romhack hooks: Safari Zone generations + in-game randomizer.
#include "wild_encounter.h"
#include "rh_settings.h"

#define RH_SAFARI_GEN_OFF 0
#define RH_SAFARI_GEN_ALL 10

#define RH_LEVEL_MOD_DEFAULT 5  // index of +0% in the level modifier table

struct TrainerMon;
struct Trainer;
struct LevelUpMove;
struct Evolution;

extern struct RhSettings gRhPendingSettings;     // edited by the New Game randomizer screen

static inline const struct RhSettings *RH_Settings(void)
{
    return &gSaveBlock3Ptr->rhSettings;
}
static inline bool32 RH_Enabled(void)
{
    return gSaveBlock3Ptr->rhSettings.enabled;
}

void CB2_InitRandomizerMenu(void);
void RH_SetDefaultSettings(struct RhSettings *s);
void RH_ApplyPendingSettings(void);              // called from NewGameInitData

// Wild
enum Species RH_ModifyWildSpecies(enum Species species, enum WildPokemonArea area);
u8 RH_ModifyWildLevel(u8 level);
u8 RH_CatchRate(enum Species species, u8 vanilla);

// Trainers / scripted Pokemon
void RH_ModifyTrainerMon(struct TrainerMon *mon, const struct Trainer *trainer, u32 slot);
enum Species RH_StarterSpecies(enum Species vanilla);
enum Species RH_StaticSpecies(enum Species species);
enum Species RH_TradeSpecies(u32 tradeId, enum Species species, bool32 requested);

// Items
enum Item RH_FieldItem(enum Item item, u32 flag);
enum Item RH_ShopItem(enum Item item, u32 mart, u32 slot);

// Species traits
enum Type RH_SpeciesType(enum Species species, u32 slot, enum Type vanilla);
enum Ability RH_SpeciesAbility(enum Species species, u32 slot, enum Ability vanilla);
u32 RH_SpeciesBaseStat(enum Species species, u32 stat, u32 vanilla);
const struct LevelUpMove *RH_LevelUpLearnset(enum Species species, const struct LevelUpMove *vanilla);
const struct Evolution *RH_Evolutions(enum Species species, const struct Evolution *vanilla);
bool32 RH_CanLearnTeachable(enum Species species, enum Move move, bool32 (*vanillaCheck)(enum Species, enum Move));

// Moves
enum Move RH_TMMove(u32 tmhmIndex, enum Move vanilla);
u32 RH_MovePower(enum Move move, u32 vanilla);
u32 RH_MoveAccuracy(enum Move move, u32 vanilla);
u32 RH_MovePP(enum Move move, u32 vanilla);
enum Type RH_MoveType(enum Move move, enum Type vanilla);
u32 RH_MoveCategory(enum Move move, u32 vanilla);

#endif
