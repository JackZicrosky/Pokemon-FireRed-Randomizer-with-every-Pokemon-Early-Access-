// Misc. tweaks, new-game setup, the HM Kit and small script specials.
#include "global.h"
#include "pokemon_storage_system.h"
#include "constants/characters.h"
#include "event_data.h"
#include "field_move.h"
#include "item.h"
#include "pokemon.h"
#include "string_util.h"
#include "rh_internal.h"
#include "constants/items.h"
#include "constants/species.h"
#include "battle.h"
#include "constants/event_objects.h"
#include "constants/moves.h"

// ---------------------------------------------------------------------------
// Lower-case names off: show species names in capitals like the original games.
// ---------------------------------------------------------------------------
static EWRAM_DATA u8 sNameBuf[2][POKEMON_NAME_LENGTH + 1] = {0};
static EWRAM_DATA u8 sNameNext = 0;

const u8 *RH_SpeciesName(const u8 *name)
{
    u8 *buf;
    u32 i;
    if (gSaveBlock3Ptr->rhSettings.version != RH_SETTINGS_VERSION || gSaveBlock3Ptr->rhSettings.lowerCaseNames)
        return name;
    buf = sNameBuf[sNameNext];
    sNameNext ^= 1;
    for (i = 0; i < POKEMON_NAME_LENGTH && name[i] != EOS; i++)
    {
        u8 c = name[i];
        if (c >= CHAR_a && c <= CHAR_z)
            c = c - CHAR_a + CHAR_A;
        else if (c == CHAR_e_ACUTE)
            c = CHAR_E_ACUTE;
        buf[i] = c;
    }
    buf[i] = EOS;
    return buf;
}

// ---------------------------------------------------------------------------
// The Pokemon Oak shows in his intro (FVX randomizes it unless "No Random Intro Mon")
// ---------------------------------------------------------------------------
u16 RH_IntroSpecies(u16 vanilla)
{
    struct RhFilter f = {0};
    u16 sp;
    if (!S->enabled || !S->randomIntroMon)
        return vanilla;
    sp = RH_PickSpecies(&f, RH_Hash(SALT_STATIC, 0x1A7, 0), SPECIES_NONE);
    return sp ? sp : vanilla;
}

// ---------------------------------------------------------------------------
// Catching tutorial (the old man in Viridian City)
// ---------------------------------------------------------------------------
u16 RH_CatchTutorialSpecies(u16 vanilla)
{
    struct RhFilter f = {0};
    u16 sp;
    if (!S->randomCatchTutorial)                            // a Misc. Tweak: works with the randomizer off too
        return vanilla;
    f.legend = 1;
    f.stage = 1;
    sp = RH_PickSpecies(&f, RH_Hash(SALT_STATIC, 0xCA7C, 0), SPECIES_NONE);
    return sp ? sp : vanilla;
}

// ---------------------------------------------------------------------------
// HM Kit: works for a field move once its Badge is earned and the HM itself has been obtained.
// ---------------------------------------------------------------------------
static u16 HMItemFor(enum FieldMove fieldMove)
{
    switch (fieldMove)
    {
    case FIELD_MOVE_CUT:        return ITEM_HM_CUT;
    case FIELD_MOVE_FLY:        return ITEM_HM_FLY;
    case FIELD_MOVE_SURF:       return ITEM_HM_SURF;
    case FIELD_MOVE_STRENGTH:   return ITEM_HM_STRENGTH;
    case FIELD_MOVE_FLASH:      return ITEM_HM_FLASH;
    case FIELD_MOVE_ROCK_SMASH: return ITEM_HM_ROCK_SMASH;
    case FIELD_MOVE_WATERFALL:  return ITEM_HM_WATERFALL;
    case FIELD_MOVE_DIVE:       return ITEM_HM_DIVE;
    default:                    return ITEM_NONE;
    }
}

bool32 RH_HMKitCovers(enum FieldMove fieldMove)
{
    u16 hm = HMItemFor(fieldMove);
    if (hm == ITEM_NONE || !CheckBagHasItem(ITEM_HM_KIT, 1))
        return FALSE;
    if (GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_SPECIES) == SPECIES_NONE)
        return FALSE;
    return IsFieldMoveUnlocked(fieldMove) && CheckBagHasItem(hm, 1);
}

// ---------------------------------------------------------------------------
// New game setup (called after the settings are copied into the save)
// ---------------------------------------------------------------------------
void RH_OnNewGame(void)
{
    FlagSet(FLAG_RH_HIDE_ROUTE3_BARRICADE);              // only shown after the Pewter Mewtwo is beaten
    AddBagItem(ITEM_HM_KIT, 1);
    AddBagItem(ITEM_RANDOMIZER_SETTINGS, 1);
    if (S->nuzlocke)
    {
        AddBagItem(ITEM_INFINITE_CANDY, 1);
        AddBagItem(ITEM_HEALING_KIT, 1);
    }
    if (S->nationalDexAtStart)
        EnableNationalPokedex();
    if (S->randomPcPotion)
    {
        u32 tries;
        u16 item = ITEM_POTION;
        for (tries = 0; tries < 32; tries++)
        {
            u16 it = 1 + RH_Hash(SALT_FIELD_ITEM, 0x9C, tries) % (ITEMS_COUNT - 1);
            if (GetItemPocket(it) == POCKET_KEY_ITEMS || GetItemPrice(it) == 0 || RH_ItemIsBad(it) || RH_ItemBanned(it)
             || GetItemTMHMIndex(it) != 0)
                continue;
            item = it;
            break;
        }
        CpuFastFill(0, gPokemonStoragePtr->pcItems, sizeof(gPokemonStoragePtr->pcItems));
        AddPCItem(item, 1);
    }
}


// Loading a save: runs started before v0.5 get the Randomizer Settings key item too.
// ---------------------------------------------------------------------------
// The Mewtwo blocking Pewter City's east road (instead of the Gym Guide).
// ---------------------------------------------------------------------------
// Map script (on transition): the Mewtwo object uses graphics variable 0.
void RH_SetPewterMewtwoGfx(void)
{
    VarSet(VAR_OBJ_GFX_ID_0, OBJ_EVENT_GFX_SPECIES(MEWTWO));
}

// After "setwildbattle": replace the (possibly randomized) opponent with the real thing. Level 100, perfect IVs,
// Timid, max Special Attack and Speed, Leftovers, and a moveset that wipes an early-game team.
void RH_PreparePewterMewtwo(void)
{
    static const u16 sMoves[MAX_MON_MOVES] = { MOVE_PSYSTRIKE, MOVE_AURA_SPHERE, MOVE_ICE_BEAM, MOVE_RECOVER };
    struct Pokemon *mon = &gParties[B_TRAINER_OPPONENT_A][0];
    u8 maxEv = MAX_PER_STAT_EVS, restEv = 4;
    u16 item = ITEM_LEFTOVERS;
    u32 i, personality;

    ZeroEnemyPartyMons();
    personality = GetMonPersonality(SPECIES_MEWTWO, MON_GENDERLESS, NATURE_TIMID, RANDOM_UNOWN_LETTER);
    CreateMonWithIVs(mon, SPECIES_MEWTWO, MAX_LEVEL, personality, OTID_STRUCT_PLAYER_ID, MAX_PER_STAT_IVS);
    for (i = 0; i < MAX_MON_MOVES; i++)
        SetMonMoveSlot(mon, sMoves[i], i);
    SetMonData(mon, MON_DATA_SPATK_EV, &maxEv);
    SetMonData(mon, MON_DATA_SPEED_EV, &maxEv);
    SetMonData(mon, MON_DATA_HP_EV, &restEv);
    SetMonData(mon, MON_DATA_HELD_ITEM, &item);
    CalculateMonStats(mon);
}

void RH_OnContinue(void)
{
    if (!CheckBagHasItem(ITEM_RANDOMIZER_SETTINGS, 1))
        AddBagItem(ITEM_RANDOMIZER_SETTINGS, 1);
    if (VarGet(VAR_RH_MOM_GAVE_RINGS) != 0)
        FlagSet(FLAG_SYS_B_DASH);                         // Mom gives the Running Shoes with the rings
}

#ifndef RELEASE
#include "config/rh_test.h"
#ifdef RH_TEST_EXTRA
// Test builds only: turn on a broad set of randomizer options and give the player a Pokemon.
void RH_TestSetup(void)
{
    struct Pokemon mon;
    S->enabled = TRUE;
    S->seed = 0x1234567;
    S->mechanicsGen = 1;
    S->baseStats = 1; S->updateBaseStatsGen = 5; S->expCurve = 1; S->expCurveWho = 1;
    S->types = 2; S->forceDualTypes = TRUE;
    S->abilities = 1; S->combineDuplicateAbilities = TRUE; S->ensureTwoAbilities = TRUE;
    S->evolutions = 2; S->evoNoConvergence = TRUE; S->evoForceGrowth = TRUE; S->evoMakeEasier = 40; S->evoRemoveTimeBased = TRUE;
    S->typeChart = 2;
    S->movesets = 3;
    S->moveAccuracy = TRUE; S->movePP = TRUE; S->moveType = TRUE;
    S->tmMoves = 1; S->tmCompat = 2; S->tutorMoves = 1;
    S->wild = TRUE; S->wildZone = 1; S->wildSplitEncounterTypes = TRUE; S->wildTypeRestriction = 1; S->wildCatchEmAll = TRUE;
    S->wildSimilarStrength = TRUE; S->wildBalanceLowLevel = TRUE; S->wildEvoRestriction = 2;
    S->trainers = 2; S->rivalCarriesTeam = TRUE; S->leagueUnique = 2; S->trainerLocalPokemon = TRUE; S->trainersEvolveOn = TRUE;
    S->diverseTypes[2] = TRUE; S->heldItemsFor[2] = TRUE; S->heldSensible = TRUE; S->battleStyle = 1;
    S->statics = 3; S->trades = 2; S->tradeNicknames = TRUE;
    S->instantText = TRUE; S->nuzlocke = TRUE; S->lowerCaseNames = FALSE;
    S->shopSpecial = TRUE; S->fieldItems = 3; S->shopItems = 2;
    S->playerGraphics = 1;
    CreateMon(&mon, SPECIES_BULBASAUR, 20, 0, OTID_STRUCT_PLAYER_ID);
    GiveMonInitialMoveset(&mon);
    CalculateMonStats(&mon);
    GiveScriptedMonToPlayer(&mon, PARTY_SIZE);
    CreateMon(&mon, SPECIES_PIDGEY, 18, 0, OTID_STRUCT_PLAYER_ID);
    GiveMonInitialMoveset(&mon);
    CalculateMonStats(&mon);
    GiveScriptedMonToPlayer(&mon, PARTY_SIZE);
    AddBagItem(ITEM_INFINITE_CANDY, 1);
    AddBagItem(ITEM_HEALING_KIT, 1);
    AddBagItem(ITEM_HM_CUT, 1);
    AddBagItem(ITEM_MASTER_BALL, 5);
}
#endif
#ifdef RH_TEST_EXTRA
// Test builds only: a plain party and the romhack's key items (for field tests).
void RH_TestSetupLight(void)
{
    struct Pokemon mon;
    S->instantText = TRUE;
    CreateMon(&mon, SPECIES_BULBASAUR, 20, 0, OTID_STRUCT_PLAYER_ID);
    GiveMonInitialMoveset(&mon);
    CalculateMonStats(&mon);
    GiveScriptedMonToPlayer(&mon, PARTY_SIZE);
    CreateMon(&mon, SPECIES_PIDGEY, 18, 0, OTID_STRUCT_PLAYER_ID);
    GiveMonInitialMoveset(&mon);
    CalculateMonStats(&mon);
    GiveScriptedMonToPlayer(&mon, PARTY_SIZE);
    AddBagItem(ITEM_INFINITE_CANDY, 1);
    AddBagItem(ITEM_HEALING_KIT, 1);
    AddBagItem(ITEM_HM_CUT, 1);
}
#endif
#ifdef RH_TEST_MENU_PRESET
// Test builds only: a broad "normal play" preset for the settings screen (instant text and graphics are left to the
// test to set through the menu).
void RH_TestMenuPreset(struct RhSettings *s)
{
    s->seed = 0xBEEF;
    s->bstMode = 1;
    s->baseStats = 2; s->types = 1; s->abilities = 1; s->evolutions = 1; s->evoAdjustLevels = TRUE;
    s->evoChangeImpossible = TRUE;
    s->typeChart = 2;
    s->starters = 2; s->starterTypes = 2; s->starterNoLegends = TRUE; s->starterHeldItems = TRUE;
    s->statics = 3; s->trades = 2;
    s->movePower = TRUE; s->moveNames = TRUE; s->movesets = 1;
    s->tmMoves = 1; s->tmCompat = 1; s->tutorMoves = 1;
    s->trainers = 1; s->rivalCarriesTeam = TRUE; s->trainersEvolveOn = TRUE; s->heldItemsFor[0] = TRUE; s->battleStyle = 0;
    s->wild = TRUE; s->wildZone = 2; s->wildSimilarStrength = TRUE;
    s->fieldItems = 2; s->shopItems = 2; s->shopSpecial = TRUE;
    s->paletteMode = 1; s->paletteFollowTypes = TRUE;
    s->nuzlocke = TRUE; s->runWithoutShoes = TRUE; s->reusableTMs = TRUE;
    s->noPrematureEvos = TRUE;
}
#endif
#ifdef RH_TEST_FILL_BAG
void RH_TestFillBag(void)
{
    u32 it;
    for (it = 1; it < ITEMS_COUNT; it++)
        if (GetItemPocket(it) < POCKETS_COUNT && gItemsInfo[it].name[0] != 0)
            AddBagItem(it, 1);
}
#endif
#ifdef RH_TEST_VARIANT
// Test builds only: Misc. Tweaks in the field. Variant bit 0 = unused (running indoors is always on), bit 1 = Run Without Running
// Shoes (the shoes flag is cleared), bit 2 = Instantaneous Text.
void RH_TestTweaks(u32 variant)
{
    struct Pokemon mon;
    S->enabled = TRUE;
    S->runWithoutShoes = (variant & 2) != 0;
    S->instantText = (variant & 4) != 0;
    S->statics = (variant & 8) ? 2 : 0;
    S->seed = 0x2468ACE;
    RH_InvalidateSettingsHash();
    if (variant & 2)
        FlagClear(FLAG_SYS_B_DASH);
    else
        FlagSet(FLAG_SYS_B_DASH);
    CreateMon(&mon, SPECIES_MEWTWO, 70, 0, OTID_STRUCT_PLAYER_ID);
    GiveMonInitialMoveset(&mon);
    CalculateMonStats(&mon);
    GiveScriptedMonToPlayer(&mon, PARTY_SIZE);
}
#endif
#endif

// FRLG scripts treat "has the National Dex" as "post-game" (Indigo Plateau door guard, Celio, Dunsparce Tunnel).
// With "National Dex at Start" that only becomes true once the game is cleared.
bool8 RH_IsPostgameNationalDex(void)
{
    return IsNationalPokedexEnabled() && (!gSaveBlock3Ptr->rhSettings.nationalDexAtStart || FlagGet(FLAG_SYS_GAME_CLEAR));
}
