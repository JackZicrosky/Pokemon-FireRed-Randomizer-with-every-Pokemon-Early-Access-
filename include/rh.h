#ifndef GUARD_RH_H
#define GUARD_RH_H
// Romhack hooks: Safari Zone generations + in-game randomizer (src/rh_*.c).
#include "wild_encounter.h"
#include "rh_settings.h"
#include "constants/field_move.h"

#define RH_SAFARI_GEN_OFF 0
#define RH_SAFARI_GEN_ALL 10

struct TrainerMon;
struct Trainer;
struct TrainerGenerator;
struct LevelUpMove;
struct Evolution;
struct Pokemon;

extern struct RhSettings gRhPendingSettings;     // edited by the New Game randomizer screen

static inline const struct RhSettings *RH_Settings(void)
{
    return &gSaveBlock3Ptr->rhSettings;
}
static inline bool32 RH_Enabled(void)
{
    return gSaveBlock3Ptr->rhSettings.enabled;
}

// Settings / new game
void CB2_InitRandomizerMenu(void);
void RH_SetDefaultSettings(struct RhSettings *s);
void RH_ApplyPendingSettings(void);
void RH_FixTMText(u8 *str);                       // NPC texts naming a randomized TM's old move
void RH_OnNewGame(void);
void RH_OnContinue(void);
void RH_OpenRandomizerSettings(void);
u16 RH_IntroSpecies(u16 vanilla);
const u16 *RH_MonPalette(u16 species, bool32 isShiny, const u16 *normal, const u16 *vanilla);

// Wild
enum Species RH_ModifyWildSpecies(const struct WildPokemonInfo *info, u32 slot, enum WildPokemonArea area, u32 level);
u8 RH_ModifyWildLevel(u8 level);
u8 RH_CatchRate(enum Species species, u8 vanilla);
#define RH_CatchRateOf(species) RH_CatchRate((species), gSpeciesInfo[(species)].catchRate)
bool32 RH_GuaranteedCatch(void);
enum Item RH_WildHeldItem(enum Species species, bool32 rare, enum Item vanilla);

// Trainers / scripted Pokemon
void RH_ModifyTrainerMon(struct TrainerMon *mon, const struct Trainer *trainer, u32 slot);
void RH_FinishTrainerParty(struct Pokemon *party, const struct Trainer *trainer, u32 *count, struct TrainerGenerator *gen);
enum Species RH_StarterSpecies(enum Species vanilla);
enum Item RH_StarterHeldItem(enum Item vanilla);
enum Species RH_StaticSpecies(enum Species species);
bool32 RH_IsStaticGift(u16 species);
u8 RH_StaticLevel(u8 level);
u8 RH_BalanceStaticLevel(u16 species, u8 level);
bool32 RH_StaticUsesLegendMusic(u16 newSpecies, bool32 vanillaLegendMusic);
u16 RH_LegendMusicSpecies(u16 newSpecies);
enum Species RH_RoamerSpecies(enum Species species);
struct MenuAction;
const struct MenuAction *RH_GameCornerPrizeList(const struct MenuAction *vanilla, u32 count);
enum Species RH_TradeSpecies(u32 tradeId, enum Species species, bool32 requested);
void RH_ModifyTradeMon(struct Pokemon *mon, u32 tradeId);
u16 RH_CatchTutorialSpecies(u16 vanilla);

// Items
enum Item RH_FieldItem(enum Item item, u32 flag);
enum Item RH_ShopItem(enum Item item, u32 mart, u32 slot);
u32 RH_ItemPrice(u16 item, u32 vanilla);
const u16 *RH_FilterShopList(const u16 *items);
bool32 RH_ShopItemIsOneTime(u16 item);
bool32 RH_ShopItemSoldOut(u16 item);
void RH_ShopOnPurchase(u16 item);
enum Item RH_PickupItem(enum Item vanilla, u32 tableIndex);
bool32 RH_HMKitCovers(enum FieldMove fieldMove);

// Species traits
enum Type RH_SpeciesType(enum Species species, u32 slot, enum Type vanilla);
enum Ability RH_SpeciesAbility(enum Species species, u32 slot, enum Ability vanilla);
u32 RH_SpeciesBaseStat(enum Species species, u32 stat, u32 vanilla);
enum GrowthRate RH_SpeciesGrowthRate(enum Species species, enum GrowthRate vanilla);
const struct LevelUpMove *RH_LevelUpLearnset(enum Species species, const struct LevelUpMove *vanilla);
const u16 *RH_EggMoves(enum Species species, const u16 *vanilla);
const struct Evolution *RH_Evolutions(enum Species species, const struct Evolution *vanilla);
bool32 RH_CanLearnTeachable(enum Species species, enum Move move, bool32 (*vanillaCheck)(enum Species, enum Move));
const u8 *RH_SpeciesName(const u8 *name);

// Moves / type chart
enum Move RH_TMMove(u32 tmhmIndex, enum Move vanilla);
enum Move RH_TutorMove(enum Move vanilla);
u32 RH_MovePower(enum Move move, u32 vanilla);
u32 RH_MoveAccuracy(enum Move move, u32 vanilla);
u32 RH_MovePP(enum Move move, u32 vanilla);
enum Type RH_MoveType(enum Move move, enum Type vanilla);
u32 RH_MoveCategory(enum Move move, u32 vanilla);
const u8 *RH_MoveName(enum Move move, const u8 *vanilla);
uq4_12_t RH_TypeModifier(enum Type atkType, enum Type defType, uq4_12_t vanilla);

// UI Theme (rh_theme.c)
enum { RH_THEME_DEFAULT, RH_THEME_RANDOMIZER, RH_THEME_AMOLED, RH_THEME_COUNT };
u16 RH_ThemeColor(u16 color, u32 theme);
void RH_ThemePalette(u16 *pal, u32 count, u32 theme);
void RH_ThemeLoadedPalette(const void *src, u32 offset, u32 size);
enum { RH_THEME_MODE_TEXT, RH_THEME_MODE_SCREEN };
void RH_ThemeLoadedRange(u32 offset, u32 count, u32 mode, u16 keep);

#endif
