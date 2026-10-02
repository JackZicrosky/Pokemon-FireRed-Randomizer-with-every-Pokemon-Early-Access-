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

// Poke Balls missing from the FVX item table (no vanilla shop price). The Celadon Poke Ball seller sells them and
// they join the shop randomization pools (regular "Shop Items" and the special shops).
static const u16 sRhExtraShopItems[] = {
    ITEM_MASTER_BALL, ITEM_CHERISH_BALL, ITEM_BEAST_BALL, ITEM_DREAM_BALL, ITEM_PARK_BALL, ITEM_SPORT_BALL,
    ITEM_STRANGE_BALL, ITEM_SAFARI_BALL,
};
#define SHOP_POOL_COUNT (RH_ITEM_COUNT + ARRAY_COUNT(sRhExtraShopItems))

static u16 ShopPoolItem(u32 i)
{
    return i < RH_ITEM_COUNT ? sRhItems[i].item : sRhExtraShopItems[i - RH_ITEM_COUNT];
}

// Items whose only use is selling (Nuggets, Pearls, Stardust, Relics, Shards...): never in randomized shops.
// Bottle Caps and the mushrooms (Two Island move reminder) have a use and stay.
static bool32 IsSellOnlyItem(u16 item)
{
    enum ItemSortType t = gItemsInfo[item].sortType;
    switch (item)
    {
    case ITEM_BOTTLE_CAP: case ITEM_GOLD_BOTTLE_CAP: case ITEM_TINY_MUSHROOM: case ITEM_BIG_MUSHROOM:
        return FALSE;
    }
    return t == ITEM_TYPE_SELLABLE || t == ITEM_TYPE_RELIC || t == ITEM_TYPE_SHARD;
}

// Nuzlocke Mode: no Rare Candy in any shop.
static bool32 IsNuzlockeBannedShopItem(u16 item)
{
    return S->enabled && S->nuzlocke && item == ITEM_RARE_CANDY;
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
    if (IsSellOnlyItem(item) || IsNuzlockeBannedShopItem(item))
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
        u16 it = ShopPoolItem(RH_Hash(SALT_SHOP, mart * 256 + slot, tries) % SHOP_POOL_COUNT);
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
                 RH_Items_ZCrystal[], RH_Items_Training[], RH_Items_Berries[], RH_Items_Snacks[], RH_Items_Balls[];

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
    [RH_SPECIAL_BALLS]       = RH_Items_Balls,
};

#define SPECIAL_MAX_ITEMS 100
static EWRAM_DATA u16 sSpecialBuffer[SPECIAL_MAX_ITEMS + 1] = {0};
static EWRAM_DATA bool8 sBallShopOpen = FALSE;     // the Poke Ball seller's shop is the one open

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
    if (IsSellOnlyItem(item) || IsNuzlockeBannedShopItem(item))
        return FALSE;
    return TRUE;
}

// The evolution sellers are always randomized with the other special shops; "Guarantee Evolution Items" then
// places every evolution item somewhere in the special shops instead.
static bool32 SpecialListKept(u32 list)
{
    return list == RH_SPECIAL_TRAINING && S->shopGuaranteeX;
}

// Can every evolution item still be bought?
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

u16 RH_DebugPoolItem(u32 i) { return i < RH_ITEM_COUNT ? sRhItems[i].item : ITEM_NONE; }
bool32 RH_DebugIsEvolutionItem(u16 item) { return IsEvolutionItem(item); }
bool32 RH_DebugSpecialItemOk(u16 item) { return SpecialItemOk(item) && !IsKeptSpecialItem(item); }

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
        // special shops (FVX-style no-duplicates). With "Guarantee Evolution Items", the evolution items are
        // numbered first and the permutation runs over all special-shop slots, so each of them lands in one slot.
        u32 len, accepted = 0, p, j, evoCount = 0, total = 0;
        u16 *acc = AllocUnchecked(SHOP_POOL_COUNT * sizeof(u16));
        for (len = 0; items[len] != ITEM_NONE && len < max - 1; len++)
            ;
        for (l = 0; l < ARRAY_COUNT(sSpecialLists); l++)
            if (!SpecialListKept(l))
                for (i = 0; sSpecialLists[l][i] != ITEM_NONE; i++)
                    total++;
        for (p = 0; p < SHOP_POOL_COUNT; p++)
        {
            u16 it = ShopPoolItem(p);
            if (SpecialItemOk(it) && !IsKeptSpecialItem(it) && S->shopGuaranteeEvo && IsEvolutionItem(it))
            {
                if (acc != NULL)
                    acc[accepted] = it;
                accepted++;
                evoCount++;
            }
        }
        for (p = 0; p < SHOP_POOL_COUNT; p++)
        {
            u16 it = ShopPoolItem(p);
            if (SpecialItemOk(it) && !IsKeptSpecialItem(it) && !(S->shopGuaranteeEvo && IsEvolutionItem(it)))
            {
                if (acc != NULL)
                    acc[accepted] = it;
                accepted++;
            }
        }
        for (j = 0; j < len && accepted != 0 && acc != NULL; j++)
        {
            u32 k;
            if (evoCount != 0 && total >= evoCount && accepted > evoCount)
            {
                u32 r = RH_Permute(SALT_SPECIAL_SHOP, (offset + j) % total, total);
                k = (r < evoCount) ? r : evoCount + (r - evoCount) % (accepted - evoCount);
            }
            else
            {
                k = RH_Permute(SALT_SPECIAL_SHOP, (offset + j) % accepted, accepted);
            }
            out[n++] = acc[k];
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
    sBallShopOpen = (gSpecialVar_0x8004 == RH_SPECIAL_BALLS);
    CreatePokemartMenu(sSpecialBuffer);
}

// ---------------------------------------------------------------------------
// Poke Ball seller: in a game without shop randomization ("Shop Items" off and "Randomize Special Shops" off) his
// Master Ball is a one-time purchase. With either option on it is an ordinary item (and part of the random pool).
// ---------------------------------------------------------------------------
static bool32 ShopsRandomized(void)
{
    return S->enabled && (S->shopItems != 0 || S->shopSpecial);
}

bool32 RH_ShopItemIsOneTime(u16 item)
{
    return sBallShopOpen && item == ITEM_MASTER_BALL && !ShopsRandomized();
}

void RH_DebugSetBallShopOpen(bool32 open) { sBallShopOpen = open; }

bool32 RH_ShopItemSoldOut(u16 item)
{
    return RH_ShopItemIsOneTime(item) && FlagGet(FLAG_RH_BOUGHT_MASTER_BALL);
}

void RH_ShopOnPurchase(u16 item)
{
    if (RH_ShopItemIsOneTime(item))
        FlagSet(FLAG_RH_BOUGHT_MASTER_BALL);
}

// Every shop list goes through here (shop.c, CreatePokemartMenu): drops Rare Candy in Nuzlocke Mode and the
// one-time Master Ball once it has been bought.
static EWRAM_DATA u16 sFilteredShop[SPECIAL_MAX_ITEMS + 1] = {0};

const u16 *RH_FilterShopList(const u16 *items)
{
    u32 i, n = 0;
    bool32 changed = FALSE;
    for (i = 0; items[i] != ITEM_NONE; i++)
        if (IsNuzlockeBannedShopItem(items[i]) || RH_ShopItemSoldOut(items[i]))
            changed = TRUE;
    if (!changed)
        return items;
    for (i = 0; items[i] != ITEM_NONE && n < SPECIAL_MAX_ITEMS; i++)
        if (!IsNuzlockeBannedShopItem(items[i]) && !RH_ShopItemSoldOut(items[i]))
            sFilteredShop[n++] = items[i];
    sFilteredShop[n] = ITEM_NONE;
    return sFilteredShop;
}

// ---------------------------------------------------------------------------
// Prices ("Balance Shop Item Prices")
// ---------------------------------------------------------------------------
// Prices for the Poke Balls the games never sell (the Celadon Poke Ball seller sells them all).
static u32 ExtraBallPrice(u16 item)
{
    switch (item)
    {
    case ITEM_MASTER_BALL:  return 100000;
    case ITEM_CHERISH_BALL: return 3000;
    case ITEM_BEAST_BALL:   return 2000;
    case ITEM_DREAM_BALL:   return 1000;
    case ITEM_PARK_BALL:    return 1000;
    case ITEM_STRANGE_BALL: return 1000;
    case ITEM_SAFARI_BALL:  return 500;
    case ITEM_SPORT_BALL:   return 300;
    }
    return 0;
}

// "Balance Shop Prices" curve (owner's spec): prices up to 1000 stay; above that, the more expensive an item, the
// harder the cut (10000 -> ~6800, 20000 -> ~10500, 100000 -> ~22200, 250000 -> ~29500). sCurve[k] is the price
// for 1000 << k (1000 * e^(x - 0.07 x^2), x = ln(price / 1000)); prices in between are interpolated.
static const u16 sBalanceCurve[] = { 1000, 1933, 3497, 5911, 9346, 13806, 19069, 24632, 29730, 33580, 35450 };

static u32 BalancePrice(u32 price)
{
    u32 k, lo, out;
    if (price <= 1000)
        return price;
    for (k = 0; k < ARRAY_COUNT(sBalanceCurve) - 1; k++)
    {
        lo = 1000u << k;
        if (price < (lo << 1))
        {
            out = sBalanceCurve[k] + (sBalanceCurve[k + 1] - sBalanceCurve[k]) * (price - lo) / lo;
            return (out + 5) / 10 * 10;
        }
    }
    return sBalanceCurve[ARRAY_COUNT(sBalanceCurve) - 1];
}

u32 RH_DebugBalancePrice(u32 price) { return BalancePrice(price); }

u32 RH_ItemPrice(u16 item, u32 vanilla)
{
    u32 price = vanilla != 0 ? vanilla : ExtraBallPrice(item);
    if (!S->enabled || price == 0)
        return price;
    if (S->shopAddCheapRareCandy && item == ITEM_RARE_CANDY)
        return 10;                                           // FVX "Add Cheap Rare Candies"
    if (!S->shopBalancePrices)
        return price;
    // FVX's balanced price table first (not for the balls only the romhack sells: FVX lists the Master Ball at 3000)
    if (vanilla != 0 && item < ARRAY_COUNT(sRhBalancedPrices) && sRhBalancedPrices[item] != 0)
        price = sRhBalancedPrices[item];
    return BalancePrice(price);
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
    sBallShopOpen = FALSE;
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
