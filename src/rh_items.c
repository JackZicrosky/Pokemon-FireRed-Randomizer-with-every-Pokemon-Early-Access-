// Items: field items, shops (regular + the romhack's special shops), pickup, prices.
#include "global.h"
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

static u16 RandomTM(u32 salt, u32 key)
{
    u16 item;
    u32 tries;
    for (tries = 0; tries < 8; tries++)
    {
        item = GetTMHMItemId(1 + RH_Hash(salt, key, 7 + tries) % NUM_TECHNICAL_MACHINES);
        if (item != ITEM_NONE)
            return item;
    }
    return ITEM_TM_TOXIC;
}

// ---------------------------------------------------------------------------
// Field items (item balls + hidden items)
// ---------------------------------------------------------------------------
enum Item RH_FieldItem(enum Item item, u32 flag)
{
    u32 i, x, guard;
    if (!S->enabled || S->fieldItems == 0 || ItemIsProtected(item))
    {
        if (S->banLuckyEgg && item == ITEM_LUCKY_EGG && S->enabled && S->fieldItems)
            return ITEM_RARE_CANDY;
        return item;
    }
    for (i = 0; i < RH_FIELD_ITEM_COUNT; i++)
        if (sRhFieldItems[i].flag == flag)
            break;
    switch (S->fieldItems)
    {
    case 1:     // shuffle the game's own items
        if (i == RH_FIELD_ITEM_COUNT)
            return item;
        x = i;
        for (guard = 0; guard < 64; guard++)
        {
            u16 it;
            x = RH_Permute(SALT_FIELD_ITEM, x, RH_FIELD_ITEM_COUNT);
            it = sRhFieldItems[x].item;
            if (!ItemIsProtected(it) && !RH_ItemBanned(it) && !(S->fieldBanBad && RH_ItemIsBad(it)))
                return it;
        }
        return item;
    case 3:     // random, every item about equally often
        if (GetItemTMHMIndex(item) != 0)
            return RandomTM(SALT_FIELD_ITEM, flag);
        x = (i == RH_FIELD_ITEM_COUNT) ? flag % RH_ITEM_COUNT : i;
        for (guard = 0; guard < 128; guard++)
        {
            x = RH_Permute(SALT_FIELD_ITEM, x, RH_ITEM_COUNT);
            if (PoolItemOk(&sRhItems[x], S->fieldBanBad))
                return sRhItems[x].item;
        }
        return item;
    default:    // random
        if (GetItemTMHMIndex(item) != 0)
            return RandomTM(SALT_FIELD_ITEM, flag);
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
enum Item RH_ShopItem(enum Item item, u32 mart, u32 slot)
{
    u32 tries, x;
    if (!S->enabled || S->shopItems == 0 || IsRegularShopItem(item) || ItemIsProtected(item))
        return item;
    if (S->shopGuaranteeEvo && IsEvolutionItem(item))
        return item;
    if (S->shopGuaranteeX && IsXItem(item))
        return item;
    if (S->shopItems == 1)
    {
        // Shuffle: swap stock around between shops (keyed permutation of the item pool, shop items only)
        x = RH_Hash(SALT_SHOP, item, 0) % RH_ITEM_COUNT;
        for (tries = 0; tries < 64; tries++)
        {
            x = RH_Permute(SALT_SHOP, x, RH_ITEM_COUNT);
            if (ShopItemAllowed(sRhItems[x].item) && GetItemPrice(sRhItems[x].item) != 0)
                return sRhItems[x].item;
        }
        return item;
    }
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

// Global slot index -> item: a keyed bijection over the item pool with a fixed predicate is injective,
// so two slots can never get the same item.
static u16 SpecialSlotItem(u32 globalSlot)
{
    u32 x = globalSlot % RH_ITEM_COUNT, guard;
    for (guard = 0; guard < 2048; guard++)
    {
        x = RH_Permute(SALT_SPECIAL_SHOP, x, RH_ITEM_COUNT);
        if (SpecialItemOk(sRhItems[x].item) && !IsKeptSpecialItem(sRhItems[x].item))
            return sRhItems[x].item;
    }
    return ITEM_NONE;
}

// VAR_0x8004 = RH_SPECIAL_* shop id. Opens the shop; the script waits via waitstate.
void RH_OpenSpecialShop(void)
{
    u32 list = gSpecialVar_0x8004, i, n = 0, offset = 0, l;
    const u16 *items;
    if (list >= ARRAY_COUNT(sSpecialLists))
        list = 0;
    items = sSpecialLists[list];
    for (l = 0; l < list; l++)
        for (i = 0; sSpecialLists[l][i] != ITEM_NONE; i++)
            offset++;
    for (i = 0; items[i] != ITEM_NONE && n < SPECIAL_MAX_ITEMS; i++)
    {
        u16 item = items[i];
        if (S->enabled && S->shopSpecial && !SpecialListKept(list))
            item = SpecialSlotItem(offset + i);
        if (item != ITEM_NONE)
            sSpecialBuffer[n++] = item;
    }
    sSpecialBuffer[n] = ITEM_NONE;
    CreatePokemartMenu(sSpecialBuffer);
}

// ---------------------------------------------------------------------------
// Prices ("Balance Shop Item Prices")
// ---------------------------------------------------------------------------
u32 RH_ItemPrice(u16 item, u32 vanilla)
{
    if (!S->enabled || S->shopItems == 0 || !S->shopBalancePrices || vanilla == 0)
        return vanilla;
    if (IsEvolutionItem(item))
        return 3000;
    if (vanilla < 100)
        return 100;
    if (vanilla > 20000)
        return 20000;
    return vanilla;
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
