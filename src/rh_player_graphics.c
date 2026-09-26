// Custom Player Graphics: UPR FVX graphics packs (converted by tools/rh/gen_player_graphics.py).
#include "global.h"
#include "data.h"
#include "event_object_movement.h"
#include "sprite.h"
#include "palette.h"
#include "decompress.h"
#include "rh_player_palettes.h"
#include "constants/event_objects.h"
#include "constants/trainers.h"

struct RhPlayerPack
{
    const struct SpriteFrameImage *normal, *surf, *bike, *fish, *item, *itemBike;
    const u32 *front;
    const u16 *frontPal;
    const u32 *oak;
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

static bool32 IsGirlGraphics(u16 graphicsId)
{
    switch (graphicsId)
    {
    case OBJ_EVENT_GFX_GREEN_NORMAL: case OBJ_EVENT_GFX_GREEN_BIKE: case OBJ_EVENT_GFX_GREEN_SURF:
    case OBJ_EVENT_GFX_GREEN_FIELD_MOVE: case OBJ_EVENT_GFX_GREEN_FISH: case OBJ_EVENT_GFX_GREEN_VS_SEEKER:
    case OBJ_EVENT_GFX_GREEN_VS_SEEKER_BIKE:
        return TRUE;
    }
    return FALSE;
}

static const struct RhPlayerPack *CurrentPack(void)
{
    u32 i = gSaveBlock3Ptr->rhSettings.playerGraphics;
    if (i == 0 || i > ARRAY_COUNT(sRhPlayerPacks))
        return NULL;
    return &sRhPlayerPacks[i - 1];
}

// "Character to Replace": 0 = Auto (whichever character the player picked), 1 = Boy, 2 = Girl.
static u32 ReplacedGender(void)
{
    u32 r = gSaveBlock3Ptr->rhSettings.playerGraphicsReplace;
    if (r == 0 || r > 2)
        return gSaveBlock2Ptr->playerGender != MALE ? FEMALE : MALE;
    return r == 2 ? FEMALE : MALE;
}

bool32 RH_PlayerGraphicsIsGirl(void)
{
    return ReplacedGender() == FEMALE;
}

// Is the given player character (MALE / FEMALE) replaced by a graphics pack?
bool32 RH_PlayerGraphicsReplaces(u32 gender)
{
    return CurrentPack() != NULL && (gender != MALE) == RH_PlayerGraphicsIsGirl();
}

// Oak's intro pics (8bpp BG, 64x96): TRUE if the pack's pic was loaded.
bool32 RH_LoadOakSpeechPlayerPic(u32 gender, void *vram, u32 paletteOffset)
{
    const struct RhPlayerPack *pack = CurrentPack();
    u32 r = gSaveBlock3Ptr->rhSettings.playerGraphicsReplace;
    // In Oak's intro the choice isn't made yet: with Auto, both characters show the pack.
    if (pack == NULL || (r != 0 && (gender != MALE) != (r == 2)))
        return FALSE;
    LoadPalette(pack->frontPal, paletteOffset, PLTT_SIZE_4BPP);
    DecompressDataWithHeaderVram(pack->oak, vram);
    return TRUE;
}

const struct ObjectEventGraphicsInfo *RH_PlayerObjectGraphics(u16 graphicsId, const struct ObjectEventGraphicsInfo *vanilla)
{
    const struct RhPlayerPack *pack = CurrentPack();
    s32 kind = KindOf(graphicsId);
    bool32 isGirl = IsGirlGraphics(graphicsId);
    struct ObjectEventGraphicsInfo *info;
    if (pack == NULL || kind < 0 || vanilla == NULL || isGirl != RH_PlayerGraphicsIsGirl())
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
