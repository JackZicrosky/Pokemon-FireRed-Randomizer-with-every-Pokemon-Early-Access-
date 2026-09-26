// Misc. tweaks, new-game setup, the HM Kit and small script specials.
#include "global.h"
#include "constants/characters.h"
#include "event_data.h"
#include "field_move.h"
#include "item.h"
#include "pokemon.h"
#include "string_util.h"
#include "rh_internal.h"
#include "constants/items.h"
#include "constants/species.h"

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
// Catching tutorial (the old man in Viridian City)
// ---------------------------------------------------------------------------
u16 RH_CatchTutorialSpecies(u16 vanilla)
{
    struct RhFilter f = {0};
    u16 sp;
    if (!S->enabled || !S->randomCatchTutorial)
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
    AddBagItem(ITEM_HM_KIT, 1);
    if (S->nuzlocke)
    {
        AddBagItem(ITEM_INFINITE_CANDY, 1);
        AddBagItem(ITEM_HEALING_KIT, 1);
    }
    if (S->nationalDexAtStart)
        EnableNationalPokedex();
    if (S->enabled && S->randomPcPotion)
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
        CpuFastFill(0, gSaveBlock1Ptr->pcItems, sizeof(gSaveBlock1Ptr->pcItems));
        AddPCItem(item, 1);
    }
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
#endif

// FRLG scripts treat "has the National Dex" as "post-game" (Indigo Plateau door guard, Celio, Dunsparce Tunnel).
// With "National Dex at Start" that only becomes true once the game is cleared.
bool8 RH_IsPostgameNationalDex(void)
{
    return IsNationalPokedexEnabled() && (!gSaveBlock3Ptr->rhSettings.nationalDexAtStart || FlagGet(FLAG_SYS_GAME_CLEAR));
}
