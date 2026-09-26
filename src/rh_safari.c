// Safari Zone generation selector: replaces Safari encounters with Pokemon from a chosen generation,
// placed into areas by type (water types on water, etc.).
#include "global.h"
#include "event_data.h"
#include "random.h"
#include "safari_zone.h"
#include "string_util.h"
#include "text.h"
#include "rh.h"
#include "constants/species.h"
#include "constants/maps.h"
#include "data/rh_safari.h"

#define POOL(name) { sRhSafari_##name, ARRAY_COUNT(sRhSafari_##name) }
struct RhPool { const struct RhSafariMon *mons; u16 count; };
enum { P_CENTER, P_EAST, P_NORTH, P_WEST, P_SURF, P_FISH };
static const struct RhPool sPools[] = { POOL(CENTER), POOL(EAST), POOL(NORTH), POOL(WEST), POOL(SURF), POOL(FISH) };

static u32 PoolWeight(u32 pool, u32 mode)
{
    u32 i, total = 0;
    for (i = 0; i < sPools[pool].count; i++)
    {
        const struct RhSafariMon *m = &sPools[pool].mons[i];
        if (mode == RH_SAFARI_GEN_ALL)
            total += m->weightAll;
        else if (m->gen == mode)
            total += m->weight;
    }
    return total;
}

static u16 PickFromPool(u32 pool, u32 mode, u32 total)
{
    u32 i, r = Random32() % total;
    for (i = 0; i < sPools[pool].count; i++)
    {
        const struct RhSafariMon *m = &sPools[pool].mons[i];
        u32 w = (mode == RH_SAFARI_GEN_ALL) ? m->weightAll : (m->gen == mode ? m->weight : 0);
        if (r < w)
            return m->species;
        r -= w;
    }
    return SPECIES_NONE;
}

static u16 ApplyFormGroup(u16 species)
{
    u32 i;
    for (i = 0; i < ARRAY_COUNT(sRhSafariFormGroups); i++)
        if (sRhSafariFormGroups[i].base == species)
            return sRhSafariFormGroups[i].forms[Random() % sRhSafariFormGroups[i].count];
    return species;
}

static s32 LandPoolForMap(void)
{
    u16 map = (gSaveBlock1Ptr->location.mapGroup << 8) | gSaveBlock1Ptr->location.mapNum;
    switch (map)
    {
    case MAP_SAFARI_ZONE_CENTER:    return P_CENTER;
    case MAP_SAFARI_ZONE_EAST:      return P_EAST;
    case MAP_SAFARI_ZONE_NORTH_FRLG: return P_NORTH;
    case MAP_SAFARI_ZONE_WEST:      return P_WEST;
    }
    return -1;
}

enum Species RH_SafariSpecies(enum Species species, enum WildPokemonArea area, bool32 *handled)
{
    *handled = FALSE;
    u32 mode = VarGet(VAR_RH_SAFARI_GEN), total;
    s32 pool, alt = -1;

    if (mode == RH_SAFARI_GEN_OFF || mode > RH_SAFARI_GEN_ALL || !GetSafariZoneFlag())
        return species;
    pool = LandPoolForMap();
    if (pool < 0)
        return species;
    if (area == WILD_AREA_WATER)
        pool = P_SURF, alt = P_FISH;
    else if (area == WILD_AREA_FISHING)
        pool = P_FISH, alt = P_SURF;
    else if (area != WILD_AREA_LAND)
        return species;

    total = PoolWeight(pool, mode);
    if (total == 0 && alt >= 0)
    {
        pool = alt;
        total = PoolWeight(pool, mode);
    }
    if (total == 0)
        return species;
    *handled = TRUE;
    return ApplyFormGroup(PickFromPool(pool, mode, total));
}


static const u8 *const sGenNames[] = {
    [0] = COMPOUND_STRING("CLASSIC KANTO"),
    [2] = COMPOUND_STRING("GEN 2 (JOHTO)"),
    [3] = COMPOUND_STRING("GEN 3 (HOENN)"),
    [4] = COMPOUND_STRING("GEN 4 (SINNOH)"),
    [5] = COMPOUND_STRING("GEN 5 (UNOVA)"),
    [6] = COMPOUND_STRING("GEN 6 (KALOS)"),
    [7] = COMPOUND_STRING("GEN 7 (ALOLA)"),
    [8] = COMPOUND_STRING("GEN 8 (GALAR)"),
    [9] = COMPOUND_STRING("GEN 9 (PALDEA)"),
    [10] = COMPOUND_STRING("ALL GENERATIONS"),
};

// Buffers the current Safari setting name into gStringVar1.
void RH_BufferSafariGenName(void)
{
    u32 mode = VarGet(VAR_RH_SAFARI_GEN);
    if (mode > RH_SAFARI_GEN_ALL || sGenNames[mode] == NULL)
        mode = 0;
    StringCopy(gStringVar1, sGenNames[mode]);
}
