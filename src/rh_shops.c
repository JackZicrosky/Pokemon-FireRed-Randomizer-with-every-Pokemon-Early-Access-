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
