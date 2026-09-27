// Items: field items, shops (regular + the romhack's special shops), pickup, prices.
#include "global.h"
#include "malloc.h"
#include "event_data.h"
#include "event_object_movement.h"
#include "item.h"
#include "overworld.h"
#include "script.h"
#include "shop.h"
#include "rh_internal.h"
#include "constants/items.h"
#include "constants/rh_shops.h"
#include "constants/rh_special_shops.h"
#include "data/rh_randomizer_tables.h"
#include "data/rh_prices.h"

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static bool32 ItemIsProtected(u16 item)
{
    if (item == ITEM_NONE || GetItemPocket(item) == POCKET_KEY_ITEMS)
        return TRUE;
    if (GetItemTMHMIndex(item) > NUM_TECHNICAL_MACHINES)      // HMs (1-based index)
        return TRUE;
    return FALSE;
}

static bool32 IsRegularShopItem(u16 item)
{
    switch (item)
    {
    case ITEM_POKE_BALL: case ITEM_GREAT_BALL: case ITEM_ULTRA_BALL:
    case ITEM_POTION: case ITEM_SUPER_POTION: case ITEM_HYPER_POTION: case ITEM_MAX_POTION: case ITEM_FULL_RESTORE:
    case ITEM_REVIVE: case ITEM_ANTIDOTE: case ITEM_PARALYZE_HEAL: case ITEM_AWAKENING: case ITEM_BURN_HEAL:
    case ITEM_ICE_HEAL: case ITEM_FULL_HEAL: case ITEM_REPEL: case ITEM_SUPER_REPEL: case ITEM_MAX_REPEL:
    case ITEM_ESCAPE_ROPE:
        return TRUE;
    }
    return FALSE;
}

static bool32 IsEvolutionItem(u16 item)
{
    enum ItemSortType t = gItemsInfo[item].sortType;
    return GetItemPocket(item) != POCKET_KEY_ITEMS && (t == ITEM_TYPE_EVOLUTION_STONE || t == ITEM_TYPE_EVOLUTION_ITEM);
}

static bool32 IsXItem(u16 item)
{
    switch (item)
    {
    case ITEM_X_ATTACK: case ITEM_X_DEFENSE: case ITEM_X_SP_ATK: case ITEM_X_SP_DEF: case ITEM_X_SPEED:
    case ITEM_X_ACCURACY: case ITEM_DIRE_HIT: case ITEM_GUARD_SPEC:
        return TRUE;
    }
    return FALSE;
}

static bool32 PoolItemOk(const struct RhItem *it, bool32 banBad)
{
    if (RH_ItemBanned(it->item))
        return FALSE;
    if (banBad && (it->junk || RH_ItemIsBad(it->item)))
        return FALSE;
    return TRUE;
}

static u16 RandomPoolItem(u32 salt, u32 a, u32 b, bool32 banBad)
{
    u32 tries;
    for (tries = 0; tries < 48; tries++)
    {
        const struct RhItem *it = &sRhItems[RH_Hash(salt, a, b * 64 + tries) % RH_ITEM_COUNT];
        if (PoolItemOk(it, banBad))
            return it->item;
    }
    return ITEM_POTION;
}

// ---------------------------------------------------------------------------
// Field items (item balls + hidden items)
// ---------------------------------------------------------------------------
// Shuffle keeps the game's own items (FVX: "Ban Bad Items" only affects the random modes).
static bool32 FieldPoolItemOk(u16 it)
{
    return !ItemIsProtected(it) && GetItemTMHMIndex(it) == 0 && !RH_ItemBanned(it);
}

// k-th field item (in a keyed order) that passes "ok": different k never share an entry.
static s32 RankedFieldEntry(u32 k, bool32 tms)
{
    u32 p, n = RH_FIELD_ITEM_COUNT, count = 0, accepted = 0;
    for (p = 0; p < n; p++)
    {
        u16 it = sRhFieldItems[p].item;
        accepted += tms ? (GetItemTMHMIndex(it) != 0 && !ItemIsProtected(it)) : FieldPoolItemOk(it);
    }
    if (accepted == 0)
        return -1;
    k = RH_Permute(SALT_FIELD_ITEM, k % accepted, accepted);
    for (p = 0; p < n; p++)
    {
        u16 it = sRhFieldItems[p].item;
        if (tms ? (GetItemTMHMIndex(it) != 0 && !ItemIsProtected(it)) : FieldPoolItemOk(it))
        {
            if (count == k)
                return p;
            count++;
        }
    }
    return -1;
}

// Rank of field entry i among the spots of its own kind (TM or not; every spot counts, so no two spots share a
// rank even when their original item is banned).
static u32 FieldRank(u32 i, bool32 tm)
{
    u32 p, r = 0;
    for (p = 0; p < i; p++)
    {
        u16 it = sRhFieldItems[p].item;
        if (!ItemIsProtected(it) && (GetItemTMHMIndex(it) != 0) == tm)
            r++;
    }
    return r;
}

// TM spots in the random modes (FVX getRequiredFieldTMs): the TMs found in the field are placed first, so none of
// them becomes impossible to get; TM spots only ever hold TMs.
static enum Item FieldTM(u32 i, enum Item item)
{
    s32 e;
    if (i == RH_FIELD_ITEM_COUNT)
        return item;
    e = RankedFieldEntry(FieldRank(i, TRUE), TRUE);
    return e >= 0 ? sRhFieldItems[e].item : item;
}

enum Item RH_FieldItem(enum Item item, u32 flag)
{
    u32 i, x, guard;
    bool32 isTM = GetItemTMHMIndex(item) != 0;
    if (!S->enabled || S->fieldItems == 0 || ItemIsProtected(item))
        return item;
    for (i = 0; i < RH_FIELD_ITEM_COUNT; i++)
        if (sRhFieldItems[i].flag == flag)
            break;
    switch (S->fieldItems)
    {
    case 1:     // shuffle the game's own items (FVX: TMs stay in TM spots, only their numbers move)
    {
        s32 e;
        if (i == RH_FIELD_ITEM_COUNT)
            return item;
        e = RankedFieldEntry(FieldRank(i, isTM), isTM);
        return e >= 0 ? sRhFieldItems[e].item : item;
    }
    case 3:     // random, every item about equally often
        if (isTM)
            return FieldTM(i, item);
        x = (i == RH_FIELD_ITEM_COUNT) ? flag % RH_ITEM_COUNT : i;
        for (guard = 0; guard < 128; guard++)
        {
            x = RH_Permute(SALT_FIELD_ITEM, x, RH_ITEM_COUNT);
            if (PoolItemOk(&sRhItems[x], S->fieldBanBad))
                return sRhItems[x].item;
        }
        return item;
    default:    // random
        if (isTM)
            return FieldTM(i, item);
        return RandomPoolItem(SALT_FIELD_ITEM, flag, 0, S->fieldBanBad);
    }
}

// Std_FindItem: VAR_0x8000 = item, VAR_LAST_TALKED = item ball local id.
void RH_RandomizeItemBall(void)
{
    const struct ObjectEventTemplate *t = GetObjectEventTemplateByLocalIdAndMap(gSpecialVar_LastTalked, gSaveBlock1Ptr->location.mapNum, gSaveBlock1Ptr->location.mapGroup);
    if (t != NULL && t->flagId != 0)
        gSpecialVar_0x8000 = RH_FieldItem(gSpecialVar_0x8000, t->flagId);
}

// ---------------------------------------------------------------------------
// Regular shops
// ---------------------------------------------------------------------------
static bool32 ShopItemAllowed(u16 item)
{
    if (item == ITEM_NONE || RH_ItemBanned(item) || GetItemPocket(item) == POCKET_KEY_ITEMS)
        return FALSE;
    if (S->shopBanBad && RH_ItemIsBad(item))
        return FALSE;
    if (S->shopBanRegular && IsRegularShopItem(item))
        return FALSE;
    if (S->shopBanOverpowered && RH_ItemIsOverpowered(item))
        return FALSE;
    if (GetItemTMHMIndex(item) > NUM_TECHNICAL_MACHINES)
        return FALSE;
    return TRUE;
}

// Regular clerks: balls, medicine and repels stay; everything else follows the Shop Items setting.
u16 RH_ShuffledShopItem(u16 item, u32 mart, u32 slot);

// A shop slot the Shop Items option may change (not balls / medicine / repels, key items or guaranteed items).
bool32 RH_ShopSlotRandomizable(u16 item)
{
    if (item == ITEM_NONE || IsRegularShopItem(item) || ItemIsProtected(item))
        return FALSE;
    if (S->shopGuaranteeEvo && IsEvolutionItem(item))
        return FALSE;
    if (S->shopGuaranteeX && IsXItem(item))
        return FALSE;
    return TRUE;
}

enum Item RH_ShopItem(enum Item item, u32 mart, u32 slot)
{
    u32 tries;
    if (!S->enabled || S->shopItems == 0 || IsRegularShopItem(item) || ItemIsProtected(item))
        return item;
    if (S->shopGuaranteeEvo && IsEvolutionItem(item))
        return item;
    if (S->shopGuaranteeX && IsXItem(item))
        return item;
    if (S->shopItems == 1)
        return RH_ShuffledShopItem(item, mart, slot);        // the shops' own items, moved around
    for (tries = 0; tries < 32; tries++)
    {
        u16 it = sRhItems[RH_Hash(SALT_SHOP, mart * 256 + slot, tries) % RH_ITEM_COUNT].item;
        if (ShopItemAllowed(it) && GetItemPrice(it) != 0)
            return it;
    }
    return item;
}

// ---------------------------------------------------------------------------
// The romhack's special shops (evolution specialist, herb shop, Celadon counters).
// Every slot gets a different random item and no item appears twice across all of them.
// ---------------------------------------------------------------------------
extern const u16 RH_Items_Evo[], RH_Items_Herb[], RH_Items_Competitive[], RH_Items_Forms[], RH_Items_Mega[],
                 RH_Items_ZCrystal[], RH_Items_Training[], RH_Items_Berries[], RH_Items_Snacks[];

static const u16 *const sSpecialLists[] = {
    [RH_SPECIAL_EVO]         = RH_Items_Evo,
    [RH_SPECIAL_HERB]        = RH_Items_Herb,
    [RH_SPECIAL_COMPETITIVE] = RH_Items_Competitive,
    [RH_SPECIAL_FORMS]       = RH_Items_Forms,
    [RH_SPECIAL_MEGA]        = RH_Items_Mega,
    [RH_SPECIAL_ZCRYSTAL]    = RH_Items_ZCrystal,
    [RH_SPECIAL_TRAINING]    = RH_Items_Training,
    [RH_SPECIAL_BERRIES]     = RH_Items_Berries,
    [RH_SPECIAL_SNACKS]      = RH_Items_Snacks,
};

#define SPECIAL_MAX_ITEMS 100
static EWRAM_DATA u16 sSpecialBuffer[SPECIAL_MAX_ITEMS + 1] = {0};

static bool32 SpecialItemOk(u16 item)
{
    if (RH_ItemBanned(item) || GetItemPrice(item) == 0)
        return FALSE;
    if (S->shopBanBad && RH_ItemIsBad(item))
        return FALSE;
    if (S->shopBanRegular && IsRegularShopItem(item))
        return FALSE;
    if (S->shopBanOverpowered && RH_ItemIsOverpowered(item))
        return FALSE;
    return TRUE;
}

static bool32 SpecialListKept(u32 list)
{
    return (list == RH_SPECIAL_EVO && S->shopGuaranteeEvo) || (list == RH_SPECIAL_TRAINING && S->shopGuaranteeX);
}

// Can every evolution item still be bought? (The evolution sellers keep their stock.)
bool32 RH_EvoItemsForSale(void)
{
    return !S->enabled || !S->shopSpecial || S->shopGuaranteeEvo;
}

static bool32 IsKeptSpecialItem(u16 item)
{
    u32 l, i;
    for (l = 0; l < ARRAY_COUNT(sSpecialLists); l++)
    {
        if (!SpecialListKept(l))
            continue;
        for (i = 0; sSpecialLists[l][i] != ITEM_NONE; i++)
            if (sSpecialLists[l][i] == item)
                return TRUE;
    }
    return FALSE;
}

// Stock of special shop "list" (RH_SPECIAL_*) into out (ITEM_NONE-terminated); returns the item count.
u32 RH_BuildSpecialShop(u32 list, u16 *out, u32 max)
{
    u32 i, n = 0, offset = 0, l;
    const u16 *items;
    if (list >= ARRAY_COUNT(sSpecialLists))
        list = 0;
    items = sSpecialLists[list];
    for (l = 0; l < list; l++)
        for (i = 0; sSpecialLists[l][i] != ITEM_NONE; i++)
            offset++;
    if (S->enabled && S->shopSpecial && !SpecialListKept(list))
    {
        // Global slot j gets accepted item number perm(j) (a keyed bijection), so no item is ever sold by two
        // special shops (FVX-style no-duplicates).
        u32 len, accepted = 0, p, j;
        u16 *acc = AllocUnchecked(RH_ITEM_COUNT * sizeof(u16));
        for (len = 0; items[len] != ITEM_NONE && len < max - 1; len++)
            ;
        for (p = 0; p < RH_ITEM_COUNT; p++)
        {
            u16 it = sRhItems[p].item;
            if (SpecialItemOk(it) && !IsKeptSpecialItem(it))
            {
                if (acc != NULL)
                    acc[accepted] = it;
                accepted++;
            }
        }
        for (j = 0; j < len && accepted != 0; j++)
        {
            u32 k = RH_Permute(SALT_SPECIAL_SHOP, (offset + j) % accepted, accepted);
            if (acc != NULL)
            {
                out[n++] = acc[k];
            }
            else
            {
                for (p = 0; p < RH_ITEM_COUNT; p++)          // no memory: find it the slow way
                {
                    u16 it = sRhItems[p].item;
                    if (SpecialItemOk(it) && !IsKeptSpecialItem(it) && k-- == 0)
                    {
                        out[n++] = it;
                        break;
                    }
                }
            }
        }
        if (acc != NULL)
            Free(acc);
    }
    else
    {
        for (i = 0; items[i] != ITEM_NONE && n < max - 1; i++)
            out[n++] = items[i];
    }
    out[n] = ITEM_NONE;
    return n;
}

// VAR_0x8004 = RH_SPECIAL_* shop id. Opens the shop; the script waits via waitstate.
void RH_OpenSpecialShop(void)
{
    RH_BuildSpecialShop(gSpecialVar_0x8004, sSpecialBuffer, SPECIAL_MAX_ITEMS + 1);
    CreatePokemartMenu(sSpecialBuffer);
}

// ---------------------------------------------------------------------------
// Prices ("Balance Shop Item Prices")
// ---------------------------------------------------------------------------
u32 RH_ItemPrice(u16 item, u32 vanilla)
{
    if (!S->enabled || vanilla == 0)
        return vanilla;
    if (S->shopAddCheapRareCandy && item == ITEM_RARE_CANDY)
        return 10;                                           // FVX "Add Cheap Rare Candies"
    if (!S->shopBalancePrices)
        return vanilla;
    if (item < ARRAY_COUNT(sRhBalancedPrices) && sRhBalancedPrices[item] != 0)
        return sRhBalancedPrices[item];                      // FVX's balanced price table
    return vanilla;
}

// Celadon Dept. Store counters that FVX randomizes (4F stones, 5F X items and vitamins). VAR_0x8004 = counter.
extern const u16 CeladonCity_DepartmentStore_4F_Items[], CeladonCity_DepartmentStore_5F_XItems[], CeladonCity_DepartmentStore_5F_Vitamins[];
static EWRAM_DATA u16 sCounterBuffer[32] = {0};

void RH_OpenRandomizedPokemart(void)
{
    static const u16 *const sCounters[] = { CeladonCity_DepartmentStore_4F_Items, CeladonCity_DepartmentStore_5F_XItems, CeladonCity_DepartmentStore_5F_Vitamins };
    u32 which = gSpecialVar_0x8004 % ARRAY_COUNT(sCounters), i, n = 0, k;
    const u16 *items = sCounters[which];
    for (i = 0; items[i] != ITEM_NONE && n < ARRAY_COUNT(sCounterBuffer) - 1; i++)
    {
        u16 it = RH_ShopItem(items[i], RH_MART_COUNT + which, i);
        for (k = 0; k < n && sCounterBuffer[k] != it; k++)
            ;
        if (k == n)
            sCounterBuffer[n++] = it;
    }
    if (S->enabled && S->shopAddCheapRareCandy && n < ARRAY_COUNT(sCounterBuffer) - 1)
        sCounterBuffer[n++] = ITEM_RARE_CANDY;
    sCounterBuffer[n] = ITEM_NONE;
    CreatePokemartMenu(sCounterBuffer);
}

// ---------------------------------------------------------------------------
// Pickup
// ---------------------------------------------------------------------------
enum Item RH_PickupItem(enum Item vanilla, u32 tableIndex)
{
    if (!S->enabled || S->pickupItems == 0)
        return vanilla;
    return RandomPoolItem(SALT_PICKUP, tableIndex, 0, S->pickupBanBad);
}
