// Custom Player Graphics: UPR FVX graphics packs (converted by tools/rh/gen_player_graphics.py).
#include "global.h"
#include "data.h"
#include "event_object_movement.h"
#include "sprite.h"
#include "rh_player_palettes.h"
#include "constants/event_objects.h"
#include "constants/trainers.h"

struct RhPlayerPack
{
    const struct SpriteFrameImage *normal, *surf, *bike, *fish, *item, *itemBike;
    const u32 *front;
    const u16 *frontPal;
    const u8 *back;
    const u16 *backPal;
};

#include "data/rh_player_graphics.h"

enum { PG_NORMAL, PG_BIKE, PG_SURF, PG_FIELD_MOVE, PG_FISH, PG_VS_SEEKER, PG_VS_SEEKER_BIKE, PG_COUNT };

static EWRAM_DATA struct ObjectEventGraphicsInfo sPlayerGfx[PG_COUNT] = {0};

static s32 KindOf(u16 graphicsId)
{
    switch (graphicsId)
    {
    case OBJ_EVENT_GFX_RED_NORMAL:          case OBJ_EVENT_GFX_GREEN_NORMAL:          return PG_NORMAL;
    case OBJ_EVENT_GFX_RED_BIKE:            case OBJ_EVENT_GFX_GREEN_BIKE:            return PG_BIKE;
    case OBJ_EVENT_GFX_RED_SURF:            case OBJ_EVENT_GFX_GREEN_SURF:            return PG_SURF;
    case OBJ_EVENT_GFX_RED_FIELD_MOVE:      case OBJ_EVENT_GFX_GREEN_FIELD_MOVE:      return PG_FIELD_MOVE;
    case OBJ_EVENT_GFX_RED_FISH:            case OBJ_EVENT_GFX_GREEN_FISH:            return PG_FISH;
    case OBJ_EVENT_GFX_RED_VS_SEEKER:       case OBJ_EVENT_GFX_GREEN_VS_SEEKER:       return PG_VS_SEEKER;
    case OBJ_EVENT_GFX_RED_VS_SEEKER_BIKE:  case OBJ_EVENT_GFX_GREEN_VS_SEEKER_BIKE:  return PG_VS_SEEKER_BIKE;
    default: return -1;
    }
}

static const struct RhPlayerPack *CurrentPack(void)
{
    u32 i = gSaveBlock3Ptr->rhSettings.playerGraphics;
    if (i == 0 || i > ARRAY_COUNT(sRhPlayerPacks))
        return NULL;
    return &sRhPlayerPacks[i - 1];
}

const struct ObjectEventGraphicsInfo *RH_PlayerObjectGraphics(u16 graphicsId, const struct ObjectEventGraphicsInfo *vanilla)
{
    const struct RhPlayerPack *pack = CurrentPack();
    s32 kind = KindOf(graphicsId);
    struct ObjectEventGraphicsInfo *info;
    if (pack == NULL || kind < 0 || vanilla == NULL)
        return NULL;
    info = &sPlayerGfx[kind];
    *info = *vanilla;
    info->paletteTag = OBJ_EVENT_PAL_TAG_RH_PLAYER + gSaveBlock3Ptr->rhSettings.playerGraphics - 1;
    switch (kind)
    {
    case PG_NORMAL:         info->images = pack->normal; break;
    case PG_BIKE:           info->images = pack->bike; break;
    case PG_SURF:           info->images = pack->surf; break;
    case PG_FIELD_MOVE:
    case PG_VS_SEEKER:      info->images = pack->item; break;
    case PG_FISH:           info->images = pack->fish; break;
    case PG_VS_SEEKER_BIKE: info->images = pack->itemBike; break;
    }
    return info;
}

// Trainer pics (front: trainer card / intro; back: battle). Built in RAM from the vanilla entry.
static EWRAM_DATA u32 sFrontBuf[(sizeof(struct TrainerFrontPicInfo) + 3) / 4] = {0};
static EWRAM_DATA u32 sBackBuf[(sizeof(struct TrainerBackPicInfo) + 3) / 4] = {0};
static EWRAM_DATA struct TrainerPicInfo sPicInfo = {0};

const struct TrainerPicInfo *RH_PlayerTrainerPicInfo(enum TrainerPicID trainerPic)
{
    const struct RhPlayerPack *pack = CurrentPack();
    const struct TrainerPicInfo *vanilla = &gTrainerPicInfo[trainerPic];
    u8 *front = (u8 *)sFrontBuf, *back = (u8 *)sBackBuf;
    const void *p;
    if (pack == NULL)
        return vanilla;
    memcpy(front, vanilla->frontPic, sizeof(struct TrainerFrontPicInfo));
    memcpy(back, vanilla->backPic, sizeof(struct TrainerBackPicInfo));
    p = pack->front;
    memcpy(front + offsetof(struct TrainerFrontPicInfo, imageData), &p, sizeof(p));
    p = pack->frontPal;
    memcpy(front + offsetof(struct TrainerFrontPicInfo, paletteData), &p, sizeof(p));
    p = pack->back;
    memcpy(back + offsetof(struct TrainerBackPicInfo, image) + offsetof(struct SpriteFrameImage, data), &p, sizeof(p));
    p = pack->backPal;
    memcpy(back + offsetof(struct TrainerBackPicInfo, paletteData), &p, sizeof(p));
    memcpy((u8 *)&sPicInfo + offsetof(struct TrainerPicInfo, frontPic), &front, sizeof(front));
    memcpy((u8 *)&sPicInfo + offsetof(struct TrainerPicInfo, backPic), &back, sizeof(back));
    return &sPicInfo;
}
