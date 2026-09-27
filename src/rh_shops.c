// Progressive Poke Mart stock (badge-gated) for the romhack.
#include "global.h"
#include "rh.h"
#include "event_data.h"
#include "shop.h"
#include "script.h"
#include "constants/items.h"
#include "constants/flags.h"
#include "constants/rh_shops.h"
#include "data/rh_shops.h"

#define RH_MART_MAX_ITEMS 160

static EWRAM_DATA u16 sRhMartBuffer[RH_MART_MAX_ITEMS] = {0};

static u32 CountBadges(void)
{
    u32 i, n = 0;
    for (i = 0; i < 8; i++)
        if (FlagGet(FLAG_BADGE01_GET + i))
            n++;
    return n;
}

static bool32 InBuffer(u32 count, u16 item)
{
    u32 i;
    for (i = 0; i < count; i++)
        if (sRhMartBuffer[i] == item)
            return TRUE;
    return FALSE;
}

static u32 Append(u32 count, u16 item)
{
    if (count < RH_MART_MAX_ITEMS - 1 && item != ITEM_NONE && !InBuffer(count, item))
        sRhMartBuffer[count++] = item;
    return count;
}

// ---------------------------------------------------------------------------
// "Shop Items: Shuffle": every randomizable stock slot of every shop (marts, themed stock, the Celadon counters)
// in one fixed order; slot number g gets the item of slot perm(g), so the game's shop items just move around.
// ---------------------------------------------------------------------------
extern const u16 CeladonCity_DepartmentStore_4F_Items[], CeladonCity_DepartmentStore_5F_XItems[], CeladonCity_DepartmentStore_5F_Vitamins[];
bool32 RH_ShopSlotRandomizable(u16 item);
u32 RH_Permute(u32 salt, u32 x, u32 n);
#define SALT_SHOP_SHUFFLE 0x5105

// Calls back for every randomizable slot; stops (returning its item) at index "stopAt". Returns the count.
static u32 WalkShopSlots(u32 stopAt, u16 *itemAt, u32 findMart, u32 findSlot, s32 *foundIndex)
{
    static const u16 *const sCounters[] = { CeladonCity_DepartmentStore_4F_Items, CeladonCity_DepartmentStore_5F_XItems, CeladonCity_DepartmentStore_5F_Vitamins };
    u32 m, i, n = 0;
    for (m = 0; m < RH_MART_COUNT + ARRAY_COUNT(sCounters); m++)
    {
        const u16 *base = (m < RH_MART_COUNT) ? sRhMarts[m].base : sCounters[m - RH_MART_COUNT];
        for (i = 0; base[i] != ITEM_NONE; i++)
        {
            if (!RH_ShopSlotRandomizable(base[i]))
                continue;
            if (m == findMart && i == findSlot)
                *foundIndex = n;
            if (n == stopAt)
                *itemAt = base[i];
            n++;
        }
        if (m >= RH_MART_COUNT)
            continue;
        for (i = 0; sRhMarts[m].themed[i].item != ITEM_NONE; i++)
        {
            u16 it = sRhMarts[m].themed[i].item;
            if (!RH_ShopSlotRandomizable(it))
                continue;
            if (m == findMart && 64 + i == findSlot)
                *foundIndex = n;
            if (n == stopAt)
                *itemAt = it;
            n++;
        }
    }
    return n;
}

u16 RH_ShuffledShopItem(u16 item, u32 mart, u32 slot)
{
    s32 g = -1;
    u16 result = item;
    u32 n = WalkShopSlots(0xFFFF, &result, mart, slot, &g);
    if (g < 0 || n == 0)
        return item;
    WalkShopSlots(RH_Permute(SALT_SHOP_SHUFFLE, g, n), &result, 0xFFFF, 0xFFFF, &g);
    return result;
}

// VAR_0x8004 = RH_MART_* id. Opens the mart; script waits via waitstate.
void RH_OpenMart(void)
{
    u32 mart = gSpecialVar_0x8004, badges = CountBadges(), count = 0, i;
    const u16 *base;
    const struct RhMartEntry *themed;

    if (mart >= RH_MART_COUNT)
        mart = 0;
    base = sRhMarts[mart].base;
    themed = sRhMarts[mart].themed;

    for (i = 0; base[i] != ITEM_NONE; i++)
        count = Append(count, RH_ShopItem(base[i], mart, i));
    for (i = 0; i < ARRAY_COUNT(sRhProgressive); i++)
        if (badges >= sRhProgressive[i].minBadges)
            count = Append(count, sRhProgressive[i].item);
    for (i = 0; themed[i].item != ITEM_NONE; i++)
        if (badges >= themed[i].minBadges)
            count = Append(count, RH_ShopItem(themed[i].item, mart, 64 + i));
    if (gSaveBlock3Ptr->rhSettings.enabled && gSaveBlock3Ptr->rhSettings.shopAddCheapRareCandy)
        count = Append(count, ITEM_RARE_CANDY);             // FVX "Add Cheap Rare Candies"
    sRhMartBuffer[count] = ITEM_NONE;

    CreatePokemartMenu(sRhMartBuffer);
}
