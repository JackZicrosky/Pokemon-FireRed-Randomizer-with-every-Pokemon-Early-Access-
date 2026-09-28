// New Game randomizer settings screen, modelled on Universal Pokemon Randomizer FVX.
// Dark UI. SELECT opens the section menu (hamburger), L/R jump between sections, UP/DOWN pick an option,
// LEFT/RIGHT change it, A opens text fields / toggles, START jumps to "Begin Run".
#include "global.h"
#include "config/rh_test.h"
#include "bg.h"
#include "gpu_regs.h"
#include "international_string_util.h"
#include "main.h"
#include "malloc.h"
#include "save.h"
#include "menu.h"
#include "naming_screen.h"
#include "oak_speech.h"
#include "palette.h"
#include "pokemon.h"
#include "random.h"
#include "scanline_effect.h"
#include "sound.h"
#include "sprite.h"
#include "string_util.h"
#include "task.h"
#include "text.h"
#include "battle_main.h"
#include "data.h"
#include "window.h"
#include "rh.h"
#include "rh_internal.h"
#include "constants/rgb.h"
#include "constants/songs.h"
#include "constants/species.h"
#include "constants/pokemon.h"

enum { WIN_HEADER, WIN_LIST, WIN_DESC, WIN_MENU };

#define LIST_ROWS     7
#define ROW_H         16
#define MENU_ROWS     9

enum RowKind { RK_HEADER, RK_CHOICE, RK_TOGGLE, RK_SLIDER, RK_TEXT, RK_ACTION };
enum TextField { TF_SEED, TF_STARTER1, TF_STARTER2, TF_STARTER3, TF_TYPE, TF_BST_MIN, TF_BST_MAX };
enum Action { ACT_BEGIN, ACT_SHOW_CODE, ACT_ENTER_CODE, ACT_SAVE_1, ACT_SAVE_2, ACT_SAVE_3, ACT_LOAD_1, ACT_LOAD_2, ACT_LOAD_3,
              ACT_DEFAULTS };
enum SliderFmt { SF_PLAIN, SF_PERCENT, SF_SIGNED_PERCENT, SF_LEVEL, SF_GEN, SF_OFF_GEN };

struct RhRow
{
    u8 kind;
    u8 section;
    u8 indent;
    u8 fmt;          // slider format / text field id
    u16 offset;      // field offset in struct RhSettings
    u8 size;         // 1 = u8, 2 = s16/u16
    s16 min, max, step;
    const u8 *label;
    const u8 *const *choices;
    u8 choiceCount;
    const u8 *desc;
    bool32 (*visible)(const struct RhSettings *s);
};

enum
{
    SEC_GENERAL, SEC_TRAITS, SEC_TYPES, SEC_STARTERS, SEC_MOVES, SEC_FOES, SEC_WILD, SEC_TMS, SEC_ITEMS,
    SEC_GRAPHICS, SEC_MISC, SEC_CODES, SEC_BEGIN, SEC_COUNT
};

static const u8 *const sSectionNames[SEC_COUNT] =
{
    [SEC_GENERAL]  = COMPOUND_STRING("General"),
    [SEC_TRAITS]   = COMPOUND_STRING("Pokémon Traits"),
    [SEC_TYPES]    = COMPOUND_STRING("Type Effectiveness"),
    [SEC_STARTERS] = COMPOUND_STRING("Starters, Statics & Trades"),
    [SEC_MOVES]    = COMPOUND_STRING("Moves & Movesets"),
    [SEC_FOES]     = COMPOUND_STRING("Foe Pokémon"),
    [SEC_WILD]     = COMPOUND_STRING("Wild Pokémon"),
    [SEC_TMS]      = COMPOUND_STRING("TM/HMs & Tutors"),
    [SEC_ITEMS]    = COMPOUND_STRING("Items"),
    [SEC_GRAPHICS] = COMPOUND_STRING("Custom Player Graphics"),
    [SEC_MISC]     = COMPOUND_STRING("Misc. Tweaks"),
    [SEC_CODES]    = COMPOUND_STRING("Settings Codes & Presets"),
    [SEC_BEGIN]    = COMPOUND_STRING("Begin Run"),
};

// ---------------------------------------------------------------------------
// Choice lists
// ---------------------------------------------------------------------------
#define LIST(name, ...) static const u8 *const name[] = { __VA_ARGS__ }
#define CS(x) COMPOUND_STRING(x)
LIST(sOffOn, CS("Off"), CS("On"));
LIST(sPool, CS("Kanto 151"), CS("Gen 1-3 (386)"), CS("All 1025"), CS("All + Forms"));
LIST(sMechGen, CS("Gen 1"), CS("Gen 2"), CS("Gen 3"), CS("Gen 4"), CS("Gen 5"), CS("Gen 6"), CS("Gen 7"), CS("Gen 8"), CS("Gen 9"));
LIST(sUSR, CS("Unchanged"), CS("Shuffle"), CS("Random"));
LIST(sExpCurves, CS("Medium Fast"), CS("Medium Slow"), CS("Fast"), CS("Slow"), CS("Erratic"), CS("Fluctuating"));
LIST(sExpWho, CS("Legendaries: Slow"), CS("Strong Legends: Slow"), CS("All Pokémon"));
LIST(sTypesC, CS("Unchanged"), CS("Random (follow evos)"), CS("Random (completely)"));
LIST(sUR, CS("Unchanged"), CS("Random"));
LIST(sEvosC, CS("Unchanged"), CS("Random"), CS("Random Every Level"));
LIST(sTypeChart, CS("Unchanged"), CS("Random"), CS("Random (balanced)"), CS("Keep Identities"), CS("Inverse"));
LIST(sStarters, CS("Unchanged"), CS("Custom"), CS("Random (completely)"), CS("Random (2 evolutions)"), CS("Random (basic)"));
LIST(sBstModes, CS("Unchanged"), CS("Random Buff/Nerf"), CS("Shuffle"), CS("Random"));
LIST(sStarterTypes, CS("None"), CS("Fire/Water/Grass"), CS("Any Type Triangle"), CS("Unique"), CS("Single Type"));
LIST(sStatics, CS("Unchanged"), CS("Swap Legend/Standard"), CS("Random (completely)"), CS("Random (similar str.)"));
LIST(sTrades, CS("Unchanged"), CS("Given Pokémon Only"), CS("Requested & Given"));
LIST(sMovesets, CS("Unchanged"), CS("Random (same type)"), CS("Random (completely)"), CS("Metronome Only"));
LIST(sTrainers, CS("Unchanged"), CS("Random"), CS("Random (even)"), CS("Type Themed"), CS("Type Themed (E4/Gyms)"),
     CS("Keep Themed Trainers"), CS("Keep Themes Or Primary"));
LIST(sBattleStyle, CS("Unchanged"), CS("Random"), CS("Single Style"));
LIST(sSinglesDoubles, CS("Single Battles"), CS("Double Battles"));
LIST(sWildZone, CS("1 In Whole Game"), CS("1 Per Location"), CS("1 Per Encounter Set"), CS("Maximum Possible"),
     CS("Random Always"), CS("1 Per Map"));
LIST(sWildType, CS("None"), CS("Randomize Zone Themes"), CS("Keep Primary Type"));
LIST(sWildEvo, CS("None"), CS("Only Basic Pokémon"), CS("Same Evolution Stage"));
LIST(sCompat, CS("Unchanged"), CS("Random (same type)"), CS("Random (completely)"), CS("Full Compatibility"));
LIST(sFieldItems, CS("Unchanged"), CS("Shuffle"), CS("Random"), CS("Random (even)"));
LIST(sShopItems, CS("Unchanged"), CS("Shuffle"), CS("Random"));

// Player graphics pack names (index 0 = default). Kept in sync with src/data/rh_player_graphics.h.
static const u8 *const sBoyGirl[] = { CS("Boy & Girl"), CS("Boy Only"), CS("Girl Only") };
extern const u8 *const gRhPlayerGraphicsNames[];
extern const u8 gRhPlayerGraphicsCount;

// ---------------------------------------------------------------------------
// Visibility rules
// ---------------------------------------------------------------------------
#define VIS(name, expr) static bool32 name(const struct RhSettings *s) { return (expr); }
VIS(visPlayerGraphics, s->playerGraphics != 0)
VIS(visPalettes, s->paletteMode != 0)
VIS(visStatsRandomized, s->baseStats != 0)
VIS(visBst, s->bstMode != 0)
VIS(visBstPct, s->bstMode == 1)
VIS(visBstShuffle, s->bstMode == 2)
VIS(visExpCurve, s->expCurve != 0)
VIS(visTypesRandom, s->types != 0)
VIS(visAbilities, s->abilities != 0)
VIS(visEvos, s->evolutions != 0)
VIS(visEvosRandom, s->evolutions == 1)
VIS(visStatsAdded, s->baseStats == 2 && s->baseStatsFollowEvos)
VIS(visEstLevels, s->evolutions != 2 && (s->evoChangeImpossible || s->evoMakeEasier))
VIS(visEvoFixes, s->evolutions != 2)
VIS(visInverse, s->typeChart == 4)
VIS(visCustomStarters, s->starters == 1)
VIS(visRandomStarters, s->starters >= 2 || (s->starters == 1 && (!s->customStarters[0] || !s->customStarters[1] || !s->customStarters[2])))
VIS(visStarterItems, s->starterHeldItems)
VIS(visBstMin, visRandomStarters(s) && s->starterBstMinOn)
VIS(visBstMax, visRandomStarters(s) && s->starterBstMaxOn)
VIS(visSingleType, visRandomStarters(s) && s->starterTypes == 4)
VIS(visStatics, s->statics != 0)
VIS(visStaticLevel, s->staticLevelModOn)
VIS(visStaticsSimilar, s->statics == 3)
VIS(visTrades, s->trades != 0)
VIS(visMovesets, s->movesets != 0 && s->movesets != 3)
VIS(visLevel1, s->movesets != 0 && s->movesets != 3 && s->guaranteedLevel1On)
VIS(visMovesetDamaging, s->movesets != 0 && s->movesets != 3 && s->movesetGoodDamagingOn)
VIS(visSingleStyle, s->battleStyle == 2)
VIS(visTrainerEvolve, s->trainersEvolveOn)
VIS(visTrainerLevel, s->trainerLevelModOn)
VIS(visWild, s->wild)
VIS(visWildSplit, s->wild && (s->wildZone == 0 || s->wildZone == 1 || s->wildZone == 5))
VIS(visWildKeepThemes, s->wild)
VIS(visWildKeepRelations, s->wild && s->wildEvoRestriction != 2 && s->wildZone != 3 && s->wildZone != 4)
VIS(visWildCatch, s->wildCatchRateOn)
VIS(visWildHeldBan, s->wildHeldItems)
VIS(visWildBalance, s->wild && s->wildSimilarStrength)
VIS(visWildLevel, s->wildLevelModOn)
VIS(visTmMoves, s->tmMoves != 0 && s->movesets != 3)
VIS(visTmDamaging, s->tmMoves != 0 && s->movesets != 3 && s->tmGoodDamagingOn)
VIS(visNotFullTm, s->tmCompat != 3)
VIS(visNotFullTutor, s->tutorCompat != 3)
VIS(visTmFollow, s->tmCompat == 1 || s->tmCompat == 2 || (s->tmCompat == 0 && s->tmLevelupSanity))
VIS(visTutorFollow, s->tutorCompat == 1 || s->tutorCompat == 2 || (s->tutorCompat == 0 && s->tutorLevelupSanity))
VIS(visNotMetronome, s->movesets != 3)
VIS(visTutorMoves, s->tutorMoves != 0 && s->movesets != 3)
VIS(visTutorDamaging, s->tutorMoves != 0 && s->movesets != 3 && s->tutorGoodDamagingOn)
VIS(visFieldRandom, s->fieldItems >= 2)
VIS(visShopFilters, s->shopItems != 0 || s->shopSpecial)
VIS(visTrainersRandom, s->trainers != 0)
VIS(visTrainerWeight, s->trainers == 3)
VIS(visPickup, s->pickupItems != 0)

// ---------------------------------------------------------------------------
// Rows
// ---------------------------------------------------------------------------
#define OFS(f) offsetof(struct RhSettings, f)
#define SZ(f) sizeof(((struct RhSettings *)0)->f)
#define HDR(sec, text) { .kind = RK_HEADER, .section = sec, .label = CS(text) }
#define CHOICE(sec, ind, f, list, text, d, vis) { .kind = RK_CHOICE, .section = sec, .indent = ind, .offset = OFS(f), .size = SZ(f), \
    .choices = list, .choiceCount = ARRAY_COUNT(list), .label = CS(text), .desc = CS(d), .visible = vis }
#define TOGGLE(sec, ind, f, text, d, vis) CHOICE(sec, ind, f, sOffOn, text, d, vis)
#define SLIDER(sec, ind, f, lo, hi, st, fm, text, d, vis) { .kind = RK_SLIDER, .section = sec, .indent = ind, .offset = OFS(f), .size = SZ(f), \
    .min = lo, .max = hi, .step = st, .fmt = fm, .label = CS(text), .desc = CS(d), .visible = vis }
#define ACTION(sec, act, text, d) { .kind = RK_ACTION, .section = sec, .fmt = act, .label = CS(text), .desc = CS(d) }
#define TEXT(sec, ind, tf, text, d, vis) { .kind = RK_TEXT, .section = sec, .indent = ind, .fmt = tf, .label = CS(text), .desc = CS(d), .visible = vis }

static const struct RhRow sRows[] =
{
    // ---------------- General ----------------
    CHOICE(SEC_GENERAL, 0, mechanicsGen, sMechGen, "Battle Mechanics", "Which generation's battle rules apply.\nPhys./Special split + Fairy always on.", NULL),
    TOGGLE(SEC_GENERAL, 0, enabled, "Randomizer", "Master switch. Off = nothing is\nrandomized (Misc. Tweaks still apply).", NULL),
    CHOICE(SEC_GENERAL, 0, speciesPool, sPool, "Pokémon Pool", "Which Pokémon the randomizer may use.\n+ Forms adds regional/alt. forms.", NULL),
    TEXT(SEC_GENERAL, 0, TF_SEED, "Seed", "Type any text. Same seed + same options\n= the exact same randomized game.", NULL),
    TOGGLE(SEC_GENERAL, 0, nuzlocke, "Nuzlocke Mode", "Adds two key items. One is an infinite\nRare Candy, one heals your whole party.", NULL),
    TOGGLE(SEC_GENERAL, 0, noPrematureEvos, "No Premature Evolutions", "Random wild/trainer Pokémon are never\nmore evolved than their level allows.", NULL),
    TOGGLE(SEC_GENERAL, 0, randomIntroMon, "Random Intro Pokémon", "Oak shows a random Pokémon in his\nintro instead of Nidoran.", NULL),

    // ---------------- Pokemon Traits ----------------
    HDR(SEC_TRAITS, "Pokémon Base Statistics"),
    CHOICE(SEC_TRAITS, 1, baseStats, sUSR, "Base Stats", "Shuffle: stats swap around.\nRandom: the total is redistributed.", NULL),
    TOGGLE(SEC_TRAITS, 2, baseStatsFollowEvos, "Follow Evolutions", "Evolutions keep the same shuffle / stat\nproportions (split evos roll their own).", visStatsRandomized),
    TOGGLE(SEC_TRAITS, 2, baseStatsRandomAdded, "Rand. Added Stats on Evo", "The stats a Pokémon gains when it\nevolves are distributed randomly.", visStatsAdded),
    TOGGLE(SEC_TRAITS, 2, statsFollowMegas, "Follow Mega Evolutions", "Megas keep their base form's new stat\norder / proportions.", visStatsRandomized),
    CHOICE(SEC_TRAITS, 1, bstMode, sBstModes, "Base Stat Totals", "Change each Pokémon's total. Its stats\nkeep their proportions.", NULL),
    SLIDER(SEC_TRAITS, 2, bstChangePct, 5, 100, 5, SF_PERCENT, "Maximum Change", "Buff/Nerf: each Pokémon gets 100% +-\nup to this much of its total.", visBstPct),
    TOGGLE(SEC_TRAITS, 2, bstFollowEvos, "Follow Evolutions", "Families share a buff/nerf, or are\nshuffled with same-length families.", visBst),
    TOGGLE(SEC_TRAITS, 2, bstSeparateLegends, "Separate Legendaries", "Legendaries only swap totals with\nother legendaries.", visBstShuffle),
    CHOICE(SEC_TRAITS, 1, updateBaseStatsGen, sMechGen, "Update Base Stats to Gen", "Use base stats from this generation.\nOff = same as Battle Mechanics.", NULL),
    CHOICE(SEC_TRAITS, 1, expCurve, sExpCurves, "Standardize EXP Curves", "All Pokémon get this EXP curve. Also\nwidens random evolution choices.", NULL),
    CHOICE(SEC_TRAITS, 2, expCurveWho, sExpWho, "Applies To", "Legendaries (or only >600 BST ones)\nget Slow instead; All = no exception.", visExpCurve),
    HDR(SEC_TRAITS, "Pokémon Types"),
    CHOICE(SEC_TRAITS, 1, types, sTypesC, "Types", "Follow evos: evolutions keep the base\ntypes (may add a 2nd). Completely: new.", NULL),
    TOGGLE(SEC_TRAITS, 2, forceDualTypes, "Force Dual Types", "Every Pokémon gets two types.", visTypesRandom),
    TOGGLE(SEC_TRAITS, 2, typesFollowMegas, "Follow Mega Evolutions", "Megas keep their base form's new\ntype (a changed type stays different).", visTypesRandom),
    HDR(SEC_TRAITS, "Pokémon Abilities"),
    CHOICE(SEC_TRAITS, 1, abilities, sUR, "Abilities", "New random abilities (Wonder Guard\nholders like Shedinja keep theirs).", NULL),
    TOGGLE(SEC_TRAITS, 2, allowWonderGuard, "Allow Wonder Guard", "Any Pokémon may get Wonder Guard.\nCan be very broken - use with care.", visAbilities),
    TOGGLE(SEC_TRAITS, 2, combineDuplicateAbilities, "Combine Duplicate Abil.", "Abilities with the same effect (Clear\nBody/White Smoke...) count as one.", visAbilities),
    TOGGLE(SEC_TRAITS, 2, ensureTwoAbilities, "Ensure Two Abilities", "Every Pokémon gets two abilities.", visAbilities),
    TOGGLE(SEC_TRAITS, 2, abilitiesFollowEvos, "Follow Evolutions", "Non-split evolutions keep their\npre-evolution's random abilities.", visAbilities),
    TOGGLE(SEC_TRAITS, 2, abilitiesFollowMegas, "Follow Mega Evolutions", "Megas get their base form's new\nabilities.", visAbilities),
    TOGGLE(SEC_TRAITS, 2, banTrapAbilities, "Ban Trapping Abilities", "No Arena Trap, Magnet Pull or\nShadow Tag.", visAbilities),
    TOGGLE(SEC_TRAITS, 2, banNegativeAbilities, "Ban Negative Abilities", "No Defeatist, Slow Start, Truant,\nKlutz or Stall.", visAbilities),
    TOGGLE(SEC_TRAITS, 2, banBadAbilities, "Ban Bad Abilities", "No Plus/Minus, Frisk, Forewarn... and\ndoubles-only ones unless all doubles.", visAbilities),
    HDR(SEC_TRAITS, "Pokémon Evolutions"),
    CHOICE(SEC_TRAITS, 1, evolutions, sEvosC, "Evolutions", "Random: new targets with the same EXP\ncurve. Every Level: evolve each level.", NULL),
    TOGGLE(SEC_TRAITS, 2, evoSimilarStrength, "Similar Strength", "New evolutions have a similar base\nstat total to the original.", visEvosRandom),
    TOGGLE(SEC_TRAITS, 2, evoSameTyping, "Same Typing", "New evolutions share a type with\nthe Pokémon evolving.", visEvos),
    TOGGLE(SEC_TRAITS, 2, evoLimitThreeStages, "Limit to Three Stages", "No evolution chain longer than\nthree stages.", visEvosRandom),
    TOGGLE(SEC_TRAITS, 2, evoNoConvergence, "No Convergence", "No two Pokémon evolve into the\nsame Pokémon.", visEvos),
    TOGGLE(SEC_TRAITS, 2, evoForceChange, "Force Change", "Every evolution is different from\nthe original.", visEvos),
    TOGGLE(SEC_TRAITS, 2, evoForceGrowth, "Force Growth", "Evolutions always have a higher base\nstat total than before.", visEvosRandom),
    TOGGLE(SEC_TRAITS, 1, evoChangeImpossible, "Change Impossible Evos", "Trade and other impossible evolutions\nbecome level-up or item evolutions.", visEvoFixes),
    SLIDER(SEC_TRAITS, 1, evoMakeEasier, 0, 55, 5, SF_LEVEL, "Make Evolutions Easier", "Final stage by this Lv, middle by 75%.\nMax (55): levels kept, other fixes on.", visEvoFixes),
    TOGGLE(SEC_TRAITS, 1, evoEstimatedLevels, "Use Estimated Evo Levels", "Changed evolutions use levels estimated\nfrom the game instead of fixed ones.", visEstLevels),
    TOGGLE(SEC_TRAITS, 1, evoRemoveTimeBased, "Remove Time-Based Evos", "Day/night evos work any time; day/night\npairs and dusk use Sun/Moon/Dusk Stone.", visEvoFixes),
    TOGGLE(SEC_TRAITS, 1, evoAdjustLevels, "Adjust Evolution Levels", "Level-up evolutions happen at a level\nthat fits (no Dragonite at Lv 7).", visEvoFixes),

    // ---------------- Type effectiveness ----------------
    CHOICE(SEC_TYPES, 0, typeChart, sTypeChart, "Type Effectiveness", "Balanced: no type over base-game max.\nIdentities: types keep their counts.", NULL),
    TOGGLE(SEC_TYPES, 1, inverseRandomImmunities, "Add Random Immunities", "A few weaknesses become immunities\ninstead (as many as the base game).", visInverse),
    TOGGLE(SEC_TYPES, 0, updateTypeChart, "Update Type Effectiveness", "Gen 6+ matchups before any randomizing.\nOff = chart of Battle Mechanics gen.", NULL),

    // ---------------- Starters, statics & trades ----------------
    HDR(SEC_STARTERS, "Starter Pokémon"),
    CHOICE(SEC_STARTERS, 1, starters, sStarters, "Starters", "Custom: type 3 names (blank = random).\nBasic: not evolved from anything.", NULL),
    TEXT(SEC_STARTERS, 2, TF_STARTER1, "Starter 1", "Replaces Bulbasaur. Type the name in\ncaps; blank = random.", visCustomStarters),
    TEXT(SEC_STARTERS, 2, TF_STARTER2, "Starter 2", "Replaces Charmander. Type the name\nin caps; blank = random.", visCustomStarters),
    TEXT(SEC_STARTERS, 2, TF_STARTER3, "Starter 3", "Replaces Squirtle. Type the name in\ncaps; blank = random.", visCustomStarters),
    TOGGLE(SEC_STARTERS, 2, starterAllowAltFormes, "Allow Alternate Formes", "Regional and other alternate forms can\nbe starters.", visRandomStarters),
    TOGGLE(SEC_STARTERS, 2, starterNoLegends, "Don't Use Legendaries", "Random starters are never legendary.", visRandomStarters),
    TOGGLE(SEC_STARTERS, 1, starterHeldItems, "Random Starter Held Items", "All three starters hold the same\nrandom item (Gen 3 rule).", NULL),
    TOGGLE(SEC_STARTERS, 2, starterBanBadItems, "Ban Bad Items", "No berries, mail or other weak items.", visStarterItems),
    TOGGLE(SEC_STARTERS, 2, starterBstMinOn, "Limit BST: Minimum", "Random starters have at least this\nbase stat total.", visRandomStarters),
    TEXT(SEC_STARTERS, 3, TF_BST_MIN, "Minimum BST", "Type a number (e.g. 300).", visBstMin),
    TOGGLE(SEC_STARTERS, 2, starterBstMaxOn, "Limit BST: Maximum", "Random starters have at most this\nbase stat total.", visRandomStarters),
    TEXT(SEC_STARTERS, 3, TF_BST_MAX, "Maximum BST", "Type a number (e.g. 350).", visBstMax),
    CHOICE(SEC_STARTERS, 2, starterTypes, sStarterTypes, "Type Restrictions", "Triangle: each beats the next. Unique:\nno shared types. Single: all 1 type.", visRandomStarters),
    TEXT(SEC_STARTERS, 3, TF_TYPE, "Single Type", "Type name in caps (e.g. DRAGON).\nBlank = a random type.", visSingleType),
    TOGGLE(SEC_STARTERS, 2, starterNoDualTypes, "No Dual Types", "Random starters have only one type\n(they may still evolve into two).", visRandomStarters),
    HDR(SEC_STARTERS, "Static Pokémon"),
    CHOICE(SEC_STARTERS, 1, statics, sStatics, "Static Pokémon", "Swap: legend<->legend, other<->other.\nSimilar: close BST. No repeats.", NULL),
    TOGGLE(SEC_STARTERS, 2, staticRandomize600, "Randomize 600+ BST", "Statics with 600+ BST (Mewtwo...) get\na purely random replacement.", visStaticsSimilar),
    TOGGLE(SEC_STARTERS, 2, staticAllowAltFormes, "Allow Alternate Formes", "Regional and other alternate forms can\nreplace static Pokémon.", visStatics),
    TOGGLE(SEC_STARTERS, 2, staticFixMusic, "Fix Music", "Legendary encounters keep their special\nbattle music after randomizing.", visStatics),
    TOGGLE(SEC_STARTERS, 1, staticLevelModOn, "Static Level Modifier", "Change the level of static Pokémon\n(works with statics unchanged too).", NULL),
    SLIDER(SEC_STARTERS, 3, staticLevelMod, -100, 150, 5, SF_SIGNED_PERCENT, "Level Change", "Percentage added to static levels.", visStaticLevel),
    HDR(SEC_STARTERS, "In-Game Trades"),
    CHOICE(SEC_STARTERS, 1, trades, sTrades, "In-Game Trades", "Given only, or both the Pokémon you\nget and the one asked for (all unique).", NULL),
    TOGGLE(SEC_STARTERS, 2, tradeNicknames, "Randomize Nicknames", "Traded Pokémon get a random nickname.", visTrades),
    TOGGLE(SEC_STARTERS, 2, tradeOTs, "Randomize OTs", "Traded Pokémon get a random Original\nTrainer name and ID.", visTrades),
    TOGGLE(SEC_STARTERS, 2, tradeIVs, "Randomize IVs", "Traded Pokémon get random IVs.", visTrades),
    TOGGLE(SEC_STARTERS, 2, tradeItems, "Randomize Items", "Traded Pokémon hold a random item.", visTrades),

    // ---------------- Moves ----------------
    HDR(SEC_MOVES, "Move Data"),
    TOGGLE(SEC_MOVES, 1, movePower, "Randomize Move Power", "Random power, mostly 50-100, sometimes\n20-150. Multi-hit: divided per hit.", NULL),
    TOGGLE(SEC_MOVES, 1, moveAccuracy, "Randomize Move Accuracy", "Most moves get random accuracy. OHKO\nstays low; sure-hit moves never change.", NULL),
    TOGGLE(SEC_MOVES, 1, movePP, "Randomize Move PP", "Every move gets random PP (5-40,\nmostly 15-25).", NULL),
    TOGGLE(SEC_MOVES, 1, moveType, "Randomize Move Types", "Every move except Struggle and ???\nmoves gets a random type.", NULL),
    TOGGLE(SEC_MOVES, 1, moveCategory, "Randomize Move Category", "Damaging moves randomly become\nPhysical or Special.", NULL),
    TOGGLE(SEC_MOVES, 1, moveNames, "Randomize Move Names", "Moves get random names based on\ntheir (randomized) type.", NULL),
    CHOICE(SEC_MOVES, 1, updateMovesGen, sMechGen, "Update Moves to Gen", "Move power/accuracy/PP/type from this\ngen. Off = same as Battle Mechanics.", NULL),
    HDR(SEC_MOVES, "Pokémon Movesets"),
    CHOICE(SEC_MOVES, 1, movesets, sMovesets, "Movesets", "Random movesets; each Pokémon starts\nwith a good attack. Metronome: only it.", NULL),
    TOGGLE(SEC_MOVES, 2, guaranteedLevel1On, "Guaranteed Level 1 Moves", "Every Pokémon knows a set number of\nmoves at level 1.", visMovesets),
    SLIDER(SEC_MOVES, 3, guaranteedLevel1Moves, 2, 4, 1, SF_PLAIN, "Moves at Level 1", "How many moves at level 1 (2-4).", visLevel1),
    TOGGLE(SEC_MOVES, 2, reorderDamagingMoves, "Reorder Damaging Moves", "Weaker attacks are learned first;\nother moves keep their places.", visMovesets),
    TOGGLE(SEC_MOVES, 2, evolutionMovesForAll, "Evolution Moves for All", "Every evolved Pokémon learns a\nmove when it evolves.", visMovesets),
    TOGGLE(SEC_MOVES, 2, movesetNoGameBreaking, "No Game-Breaking Moves", "No Sonic Boom or Dragon Rage. Gen 1\nrules: also OHKO moves and Spore.", visMovesets),
    TOGGLE(SEC_MOVES, 2, movesetGoodDamagingOn, "Force % Good Damaging", "A set share of each Pokémon's moves\nwill be good attacks.", visMovesets),
    SLIDER(SEC_MOVES, 3, movesetGoodDamaging, 0, 100, 5, SF_PERCENT, "Good Damaging Moves", "This share of random moves is forced\nto be a good attack.", visMovesetDamaging),

    // ---------------- Foes ----------------
    CHOICE(SEC_FOES, 0, trainers, sTrainers, "Trainer Pokémon", "Random, even spread, or type themes\n(per gym/E4, or keep original themes).", NULL),
    HDR(SEC_FOES, "Better Movesets for..."),
    TOGGLE(SEC_FOES, 1, betterMovesets[0], "Boss Trainers", "Gym Leaders, Giovanni, Elite Four,\nChampion: TM/tutor moves that fit.", NULL),
    TOGGLE(SEC_FOES, 1, betterMovesets[1], "Important Trainers", "Your rival's battles.", NULL),
    TOGGLE(SEC_FOES, 1, betterMovesets[2], "Regular Trainers", "Every other trainer.", NULL),
    HDR(SEC_FOES, "Additional Pokémon for..."),
    SLIDER(SEC_FOES, 1, additionalMons[0], 0, 5, 1, SF_PLAIN, "Boss Trainers", "Extra Pokémon (up to 6 total).\n0 = off.", NULL),
    SLIDER(SEC_FOES, 1, additionalMons[1], 0, 5, 1, SF_PLAIN, "Important Trainers", "Extra Pokémon (up to 6 total).\n0 = off.", NULL),
    SLIDER(SEC_FOES, 1, additionalMons[2], 0, 5, 1, SF_PLAIN, "Regular Trainers", "Extra Pokémon (up to 6 total).\n0 = off.", NULL),
    HDR(SEC_FOES, "Add Held Items to..."),
    TOGGLE(SEC_FOES, 1, heldItemsFor[0], "Boss Trainers", "Gym Leaders, Giovanni, Elite Four and\nChampion's Pokémon hold items.", NULL),
    TOGGLE(SEC_FOES, 1, heldItemsFor[1], "Important Trainers", "Your rival's Pokémon hold items.", NULL),
    TOGGLE(SEC_FOES, 1, heldItemsFor[2], "Regular Trainers", "Give regular trainers' Pokémon items.", NULL),
    TOGGLE(SEC_FOES, 1, heldConsumableOnly, "Consumable Only", "Only single-use items: mostly berries,\nplus Focus Sash, White Herb...", NULL),
    TOGGLE(SEC_FOES, 1, heldSensible, "Sensible Items", "Items that suit the Pokémon and its\nmoves (resist berries, boosters...).", NULL),
    TOGGLE(SEC_FOES, 1, heldHighestOnly, "Highest Level Only", "Only one of the trainer's highest-\nlevel Pokémon gets an item.", NULL),
    HDR(SEC_FOES, "Force Diverse Types for..."),
    TOGGLE(SEC_FOES, 1, diverseTypes[0], "Boss Trainers", "No two Pokémon share a type.\nIgnored for type-themed trainers.", NULL),
    TOGGLE(SEC_FOES, 1, diverseTypes[1], "Important Trainers", "No two Pokémon share a type.\nIgnored for type-themed trainers.", NULL),
    TOGGLE(SEC_FOES, 1, diverseTypes[2], "Regular Trainers", "No two Pokémon share a type.\nIgnored for type-themed trainers.", NULL),
    HDR(SEC_FOES, "Battle Style"),
    CHOICE(SEC_FOES, 1, battleStyle, sBattleStyle, "Battle Style", "Random singles/doubles. The first rival\nbattle never changes.", NULL),
    CHOICE(SEC_FOES, 2, battleStyleDoubles, sSinglesDoubles, "Style", "Every trainer battle uses this style.", visSingleStyle),
    HDR(SEC_FOES, "Other"),
    TOGGLE(SEC_FOES, 1, rivalCarriesTeam, "Rival Carries Starter", "The rival always uses the starter you\ndidn't pick (evolved); the rest random.", visTrainersRandom),
    TOGGLE(SEC_FOES, 1, rivalSameTeam, "Rival Keeps Same Team", "The rival uses one team all game; his\nPokémon evolve as his levels rise.", visTrainersRandom),
    TOGGLE(SEC_FOES, 1, trainerSimilarStrength, "Similar Strength", "Replacements have a similar base stat\ntotal (other rules come first).", NULL),
    TOGGLE(SEC_FOES, 1, trainerAvoidDuplicates, "Try to Avoid Duplicates", "A trainer's Pokémon are all different\n(no two from one family).", NULL),
    TOGGLE(SEC_FOES, 1, trainerWeightTypes, "Weight Types by Count", "Types with more Pokémon are picked\nmore often for themes.", visTrainerWeight),
    TOGGLE(SEC_FOES, 1, trainerLocalPokemon, "Use Local Pokémon", "Trainers use the wild Pokémon of the\narea they are in (evolved).", visTrainersRandom),
    TOGGLE(SEC_FOES, 1, trainerAllowAltFormes, "Allow Alternate Formes", "Regional and other alternate forms\ncan be used by trainers.", NULL),
    TOGGLE(SEC_FOES, 1, trainerRandomShiny, "Random Shiny Trainer Mons", "Trainer Pokémon have a 1/256 chance\nto be shiny.", NULL),
    TOGGLE(SEC_FOES, 1, trainerNoLegends, "Don't Use Legendaries", "Trainers never use legendaries.", NULL),
    TOGGLE(SEC_FOES, 1, noEarlyWonderGuard, "No Early Wonder Guard", "No Wonder Guard on trainer Pokémon\nbefore level 20.", NULL),
    SLIDER(SEC_FOES, 1, leagueUnique, 0, 2, 1, SF_PLAIN, "League Unique Pokémon", "E4/Champion's top-level Pokémon are\nused by no other trainer. 0 = off.", visTrainersRandom),
    TOGGLE(SEC_FOES, 1, randomTrainerNames, "Randomize Trainer Names", "Trainers get random names (the same\nname always becomes the same one).", NULL),
    TOGGLE(SEC_FOES, 1, randomTrainerClassNames, "Random Trainer Classes", "Trainer classes get random names.", NULL),
    TOGGLE(SEC_FOES, 1, trainersEvolveOn, "Trainers Evolve Pokémon", "Trainer Pokémon evolve as their level\nallows; fully evolved by the level below.", NULL),
    SLIDER(SEC_FOES, 2, trainersEvolveLevel, 30, 65, 1, SF_LEVEL, "Fully Evolved By", "Latest level for fully evolved\ntrainer Pokémon.", visTrainerEvolve),
    TOGGLE(SEC_FOES, 1, trainerLevelModOn, "Percentage Level Modifier", "Change every trainer Pokémon's level\n(1-100).", NULL),
    SLIDER(SEC_FOES, 2, trainerLevelMod, -100, 150, 5, SF_SIGNED_PERCENT, "Level Change", "Percentage added to trainer levels.", visTrainerLevel),

    // ---------------- Wild ----------------
    TOGGLE(SEC_WILD, 0, wild, "Randomize Wild Pokémon", "Turn on to randomize wild encounters.", NULL),
    HDR(SEC_WILD, "Replacements Per Species"),
    CHOICE(SEC_WILD, 1, wildZone, sWildZone, "Replacements", "Where a Pokémon always gets the same\nreplacement. Max/Always: none.", visWild),
    TOGGLE(SEC_WILD, 2, wildSplitEncounterTypes, "Split by Encounter Types", "Grass, surfing, fishing and Rock Smash\nget separate replacements.", visWildSplit),
    HDR(SEC_WILD, "Type Restrictions"),
    CHOICE(SEC_WILD, 1, wildTypeRestriction, sWildType, "Types", "Zone Themes: each zone gets one random\ntype. Primary: keep the first type.", visWild),
    TOGGLE(SEC_WILD, 2, wildKeepThemes, "Keep Set/Zone Themes", "Encounter sets (or zones) that share a\ntype keep it as their theme.", visWildKeepThemes),
    HDR(SEC_WILD, "Evolution Restrictions"),
    CHOICE(SEC_WILD, 1, wildEvoRestriction, sWildEvo, "Evolutions", "Only Basic: nothing evolves into them.\nSame Stage: keep the evolution stage.", visWild),
    TOGGLE(SEC_WILD, 2, wildKeepRelations, "Keep Relations", "Related Pokémon in a zone are replaced\nby Pokémon related the same way.", visWildKeepRelations),
    HDR(SEC_WILD, "Other"),
    TOGGLE(SEC_WILD, 1, wildAllowAltFormes, "Allow Alternate Formes", "Regional and other alternate forms can\nappear in the wild.", visWild),
    TOGGLE(SEC_WILD, 1, wildNoLegends, "Don't Use Legendaries", "Legendaries never appear in the wild.", visWild),
    TOGGLE(SEC_WILD, 1, wildCatchRateOn, "Set Minimum Catch Rate", "Raise catch rates below the chosen\nlevel (works without wild rando).", NULL),
    SLIDER(SEC_WILD, 2, wildCatchRate, 1, 5, 1, SF_PLAIN, "Catch Rate Level", "1-3: higher minimums, 4: max catch rate\nfor all, 5: every ball always catches.", visWildCatch),
    TOGGLE(SEC_WILD, 1, wildHeldItems, "Randomize Held Items", "Random held items; Pokémon that held\nnothing still hold nothing.", NULL),
    TOGGLE(SEC_WILD, 2, wildBanBadItems, "Ban Bad Items", "No berries, mail or other weak items.", visWildHeldBan),
    TOGGLE(SEC_WILD, 1, wildCatchEmAll, "Catch Em' All Mode", "Every Pokémon appears in the wild\nbefore any repeats (beats Similar Str.).", visWild),
    TOGGLE(SEC_WILD, 1, wildSimilarStrength, "Similar Strength", "Replacements have a similar base\nstat total.", visWild),
    TOGGLE(SEC_WILD, 2, wildBalanceLowLevel, "Balance Low Level", "Caps base stat totals by level: no\nabnormally strong low-level Pokémon.", visWildBalance),
    TOGGLE(SEC_WILD, 1, wildMegas, "Permanent Mega Pokémon", "Mega Evolved Pokémon can appear and\nstay Mega after you catch them.", visWild),
    TOGGLE(SEC_WILD, 1, wildLevelModOn, "Percentage Level Modifier", "Change every wild Pokémon's level.", NULL),
    SLIDER(SEC_WILD, 2, wildLevelMod, -100, 150, 5, SF_SIGNED_PERCENT, "Level Change", "Percentage added to wild levels.", visWildLevel),

    // ---------------- TMs & tutors ----------------
    HDR(SEC_TMS, "TMs & HMs"),
    CHOICE(SEC_TMS, 1, tmMoves, sUR, "TM Moves", "Each TM gets a new unique move.\nHMs never change.", visNotMetronome),
    TOGGLE(SEC_TMS, 2, tmNoGameBreaking, "No Game-Breaking Moves", "No Sonic Boom or Dragon Rage. Gen 1\nrules: also OHKO moves and Spore.", visTmMoves),
    TOGGLE(SEC_TMS, 2, tmKeepFieldMoves, "Keep Field Move TMs", "TMs holding field moves (Dig, Flash...)\nkeep them; not healing moves.", visTmMoves),
    TOGGLE(SEC_TMS, 2, tmGoodDamagingOn, "Force % Good Damaging", "A set share of the TMs will be good\nattacks.", visTmMoves),
    SLIDER(SEC_TMS, 3, tmGoodDamaging, 0, 100, 5, SF_PERCENT, "Good Damaging Moves", "This share of the TMs is forced to be\na good attack.", visTmDamaging),
    CHOICE(SEC_TMS, 1, tmCompat, sCompat, "TM/HM Compat.", "Same type: 90% own type, 50% Normal,\n25% others. Completely: 50% each.", NULL),
    TOGGLE(SEC_TMS, 2, tmLevelupSanity, "TM/Levelup Move Sanity", "A Pokémon can always learn TMs of\nmoves it learns by level up.", visNotFullTm),
    TOGGLE(SEC_TMS, 2, tmCompatFollowEvos, "Follow Evolutions", "Evolutions learn every TM their\npre-evolution learns (+ a few more).", visTmFollow),
    TOGGLE(SEC_TMS, 2, fullHMCompat, "Full HM Compatibility", "Every Pokémon can learn every HM.", visNotFullTm),
    HDR(SEC_TMS, "Move Tutors"),
    CHOICE(SEC_TMS, 1, tutorMoves, sUR, "Move Tutor Moves", "Each tutor gets a new unique move\n(no TM/HM moves).", visNotMetronome),
    TOGGLE(SEC_TMS, 2, tutorNoGameBreaking, "No Game-Breaking Moves", "No Sonic Boom or Dragon Rage. Gen 1\nrules: also OHKO moves and Spore.", visTutorMoves),
    TOGGLE(SEC_TMS, 2, tutorGoodDamagingOn, "Force % Good Damaging", "A set share of the tutor moves will be\ngood attacks.", visTutorMoves),
    SLIDER(SEC_TMS, 3, tutorGoodDamaging, 0, 100, 5, SF_PERCENT, "Good Damaging Moves", "This share of the tutor moves is\nforced to be a good attack.", visTutorDamaging),
    CHOICE(SEC_TMS, 1, tutorCompat, sCompat, "Tutor Compat.", "Same type: 90% own type, 50% Normal,\n25% others. Completely: 50% each.", NULL),
    TOGGLE(SEC_TMS, 2, tutorLevelupSanity, "Tutor/Levelup Move Sanity", "A Pokémon can always learn tutor\nmoves it learns by level up.", visNotFullTutor),
    TOGGLE(SEC_TMS, 2, tutorCompatFollowEvos, "Follow Evolutions", "Evolutions learn all tutor moves their\npre-evolution learns (+ a few more).", visTutorFollow),

    // ---------------- Items ----------------
    CHOICE(SEC_ITEMS, 0, fieldItems, sFieldItems, "Field Items", "Item balls & hidden items. TM spots\nget other TMs; key items stay.", NULL),
    TOGGLE(SEC_ITEMS, 1, fieldBanBad, "Ban Bad Items", "No berries, mail or other weak items.", visFieldRandom),
    CHOICE(SEC_ITEMS, 0, shopItems, sShopItems, "Shop Items", "Shop stock except balls, medicine and\nrepels (incl. Celadon 4F/5F counters).", NULL),
    TOGGLE(SEC_ITEMS, 1, shopBanBad, "Ban Bad Items", "No berries, mail or other weak items.", visShopFilters),
    TOGGLE(SEC_ITEMS, 1, shopBanRegular, "Ban Regular Shop Items", "Random stock never includes normal\nmart items (Potions, Repels...).", visShopFilters),
    TOGGLE(SEC_ITEMS, 1, shopBanOverpowered, "Ban Overpowered Items", "No Lucky Egg, Rare Candy, Master Ball\nor items that sell for a fortune.", visShopFilters),
    TOGGLE(SEC_ITEMS, 1, shopGuaranteeEvo, "Guarantee Evolution Items", "Evolution items stay in regular shops,\nand each is sold in some special shop.", visShopFilters),
    TOGGLE(SEC_ITEMS, 1, shopGuaranteeX, "Guarantee X Items", "X items, Guard Spec. and Dire Hit keep\nbeing sold where they were.", visShopFilters),
    TOGGLE(SEC_ITEMS, 0, shopBalancePrices, "Balance Shop Prices", "Use FVX's balanced prices for items\nthat are far too cheap or expensive.", NULL),
    TOGGLE(SEC_ITEMS, 0, shopAddCheapRareCandy, "Add Cheap Rare Candies", "Poké Marts and the Celadon counters\nsell Rare Candies for ¥10.", NULL),
    TOGGLE(SEC_ITEMS, 0, shopSpecial, "Randomize Special Shops", "Every special seller (evolution items,\nherbs...): random stock, no duplicates.", NULL),
    CHOICE(SEC_ITEMS, 0, pickupItems, sUR, "Pickup Items", "Items found by the Pickup ability are\nrandomized.", NULL),
    TOGGLE(SEC_ITEMS, 1, pickupBanBad, "Ban Bad Items", "No berries, mail or other weak items.", visPickup),

    // ---------------- Graphics ----------------
    { .kind = RK_CHOICE, .section = SEC_GRAPHICS, .offset = OFS(playerGraphics), .size = 1, .label = CS("Player Character"),
      .desc = CS("Play as a different character (UPR\nFVX packs): field, battle, trainer card."), .choices = NULL },
    CHOICE(SEC_GRAPHICS, 1, playerGraphicsReplace, sBoyGirl, "Character to Replace", "Which choice in Oak's intro uses them.\nBoy & Girl: you get them either way.", visPlayerGraphics),
    HDR(SEC_GRAPHICS, "Pokémon Palettes"),
    CHOICE(SEC_GRAPHICS, 0, paletteMode, sUR, "Pokémon Palettes", "Random: each Pokémon gets new colors\n(battle, summary and Pokédex sprites).", NULL),
    TOGGLE(SEC_GRAPHICS, 1, paletteFollowTypes, "Follow Types", "Colors match the Pokémon's (new) types:\na Fire Bulbasaur turns red.", visPalettes),
    TOGGLE(SEC_GRAPHICS, 1, paletteFollowEvos, "Follow Evolutions", "A family keeps similar colors.", visPalettes),
    TOGGLE(SEC_GRAPHICS, 1, paletteShinyFromNormal, "Shiny From Normal", "Shiny Pokémon show their original\ncolors.", visPalettes),

    // ---------------- Misc ----------------
    TOGGLE(SEC_MISC, 0, instantText, "Instantaneous Text", "All text appears instantly, whatever\nthe text speed option says.", NULL),
    TOGGLE(SEC_MISC, 0, runIndoors, "Running Shoes Indoors", "Run with the Running Shoes anywhere,\nincluding inside buildings.", NULL),
    TOGGLE(SEC_MISC, 0, randomPcPotion, "Randomize PC Potion", "The Potion in your bedroom PC becomes\na random useful item.", NULL),
    TOGGLE(SEC_MISC, 0, nationalDexAtStart, "National Dex at Start", "The National Dex from the start. Oak's\naides count National Dex entries.", NULL),
    TOGGLE(SEC_MISC, 0, fastEggs, "Fast Egg Hatching", "Every Egg hatches in as few steps as\npossible (under 256).", NULL),
    TOGGLE(SEC_MISC, 0, lowerCaseNames, "Lower Case Pokémon Names", "“Pikachu” instead of “PIKACHU”.", NULL),
    TOGGLE(SEC_MISC, 0, randomCatchTutorial, "Random Catching Tutorial", "The old man in Viridian catches a\nrandom Pokémon.", NULL),
    TOGGLE(SEC_MISC, 0, banLuckyEgg, "Ban Lucky Egg", "The Lucky Egg is never picked as a\nrandomized item.", NULL),
    TOGGLE(SEC_MISC, 0, balanceStaticLevels, "Balance Static Levels", "Revived fossils come back at level 30\ninstead of 5 (as in UPR FVX).", NULL),
    TOGGLE(SEC_MISC, 0, runWithoutShoes, "Run Without Running Shoes", "Hold B to run from the very start.", NULL),
    TOGGLE(SEC_MISC, 0, reusableTMs, "Infinitely Reusable TMs", "TMs are never used up (they can still\nbe tossed, held and sold).", NULL),
    TOGGLE(SEC_MISC, 0, forgettableTMs, "Forgettable HMs", "HM moves can be forgotten like any\nother move. Careful not to softlock!", NULL),
    TOGGLE(SEC_MISC, 0, noEVs, "No EVs From Pokémon", "Defeated Pokémon give 0 EVs (vitamins\nand Power items still work).", NULL),

    // ---------------- Codes & presets ----------------
    HDR(SEC_CODES, "Settings Code"),
    ACTION(SEC_CODES, ACT_SHOW_CODE, "Show Settings Code", "Shows these settings as a code you can\nwrite down or share with friends."),
    ACTION(SEC_CODES, ACT_ENTER_CODE, "Enter Settings Code", "Type in a code to load someone's\nsettings (seed included)."),
    HDR(SEC_CODES, "Presets (kept on this cartridge)"),
    ACTION(SEC_CODES, ACT_SAVE_1, "Save as Preset 1", "Saves these settings. Presets stay\nwhen you start a new game."),
    ACTION(SEC_CODES, ACT_SAVE_2, "Save as Preset 2", "Saves these settings. Presets stay\nwhen you start a new game."),
    ACTION(SEC_CODES, ACT_SAVE_3, "Save as Preset 3", "Saves these settings. Presets stay\nwhen you start a new game."),
    ACTION(SEC_CODES, ACT_LOAD_1, "Load Preset 1", "Replaces the current settings with\nthe saved ones."),
    ACTION(SEC_CODES, ACT_LOAD_2, "Load Preset 2", "Replaces the current settings with\nthe saved ones."),
    ACTION(SEC_CODES, ACT_LOAD_3, "Load Preset 3", "Replaces the current settings with\nthe saved ones."),
    HDR(SEC_CODES, "Other"),
    ACTION(SEC_CODES, ACT_DEFAULTS, "Reset to Defaults", "Every option goes back to its default\n(the seed is kept)."),

    // ---------------- Begin ----------------
    { .kind = RK_ACTION, .section = SEC_BEGIN, .fmt = ACT_BEGIN, .label = CS("Begin Run"), .desc = CS("Start the adventure with these\nsettings. Press A.") },
};

// ---------------------------------------------------------------------------
// Graphics
// ---------------------------------------------------------------------------
static const struct WindowTemplate sWinTemplates[] =
{
    [WIN_HEADER] = { .bg = 0, .tilemapLeft = 0, .tilemapTop = 0,  .width = 30, .height = 2,  .paletteNum = 1, .baseBlock = 1 },
    [WIN_LIST]   = { .bg = 0, .tilemapLeft = 0, .tilemapTop = 2,  .width = 30, .height = 14, .paletteNum = 1, .baseBlock = 1 + 60 },
    [WIN_DESC]   = { .bg = 0, .tilemapLeft = 0, .tilemapTop = 16, .width = 30, .height = 4,  .paletteNum = 1, .baseBlock = 1 + 60 + 420 },
    [WIN_MENU]   = { .bg = 1, .tilemapLeft = 1, .tilemapTop = 2,  .width = 22, .height = 18, .paletteNum = 1, .baseBlock = 1 + 60 + 420 + 120 },
    DUMMY_WIN_TEMPLATE
};

static const struct BgTemplate sBgTemplates[] =
{
    { .bg = 0, .charBaseIndex = 0, .mapBaseIndex = 31, .screenSize = 0, .paletteMode = 0, .priority = 1, .baseTile = 0 },
    { .bg = 1, .charBaseIndex = 0, .mapBaseIndex = 30, .screenSize = 0, .paletteMode = 0, .priority = 0, .baseTile = 0 },
};

// Dark theme palette.
enum { C_TRANSPARENT, C_PANEL, C_TEXT, C_SHADOW, C_VALUE, C_VALUE_SH, C_DIM, C_HEADER, C_SEL, C_ACCENT, C_HEADBAR, C_OK, C_WARN };
static const u16 sPal[16] = {
    [C_TRANSPARENT] = RGB(2, 3, 4),
    [C_PANEL]       = RGB(3, 4, 5),      // near-black panel
    [C_TEXT]        = RGB(28, 28, 29),
    [C_SHADOW]      = RGB(7, 8, 10),
    [C_VALUE]       = RGB(12, 26, 31),   // cyan
    [C_VALUE_SH]    = RGB(3, 8, 11),
    [C_DIM]         = RGB(13, 14, 16),
    [C_HEADER]      = RGB(31, 22, 10),   // amber
    [C_SEL]         = RGB(7, 9, 13),     // selected row
    [C_ACCENT]      = RGB(20, 13, 31),   // purple accent
    [C_HEADBAR]     = RGB(5, 6, 9),
    [C_OK]          = RGB(10, 27, 12),
    [C_WARN]        = RGB(31, 11, 9),
};
static const u16 sBgColor[] = { RGB(2, 3, 4) };

static const u8 sColText[]    = { C_TRANSPARENT, C_TEXT, C_SHADOW };
static const u8 sColValue[]   = { C_TRANSPARENT, C_VALUE, C_VALUE_SH };
static const u8 sColDim[]     = { C_TRANSPARENT, C_DIM, C_SHADOW };
static const u8 sColHeader[]  = { C_TRANSPARENT, C_HEADER, C_SHADOW };
static const u8 sColAccent[]  = { C_TRANSPARENT, C_ACCENT, C_SHADOW };
static const u8 sColOk[]      = { C_TRANSPARENT, C_OK, C_SHADOW };
static const u8 sColWarn[]    = { C_TRANSPARENT, C_WARN, C_SHADOW };

// ---------------------------------------------------------------------------
// State (kept across the naming screen)
// ---------------------------------------------------------------------------
static EWRAM_DATA u8 sSection = 0;
static EWRAM_DATA u8 sRow = 0;         // index into the visible rows of the section
static EWRAM_DATA u8 sScroll = 0;
static EWRAM_DATA u8 sMenuOpen = 0;
static EWRAM_DATA u8 sMenuCursor = 0;
static EWRAM_DATA u8 sConfirm = 0;     // Begin Run confirmation box: 0 closed, 1 YES selected, 2 NO selected
static EWRAM_DATA u8 sEditingField = 0;
static EWRAM_DATA u8 sInitialized = 0;
static EWRAM_DATA u8 sTextBuf[16] = {0};
static EWRAM_DATA const u8 *sMessage = NULL;  // one-shot status line (e.g. "Pokémon not found")

static EWRAM_DATA u8 sCodeMode = 0;          // 0 off, 1 showing the code, 2 typing a code
static EWRAM_DATA u8 sCodeLen = 0;
static EWRAM_DATA u8 sCodeCursor = 0;        // character grid cursor (0-31)
static EWRAM_DATA u8 sPresetUsed = 0;        // bit per preset slot
static EWRAM_DATA u8 sPendingAction = 0;     // action waiting for the YES/NO box (ACT_BEGIN = Begin Run)

#undef S
#define S (&gRhPendingSettings)

static bool32 RowVisible(const struct RhRow *r)
{
    return r->visible == NULL || r->visible(S);
}

static u32 VisibleRows(u32 section, const struct RhRow **out)
{
    u32 i, n = 0;
    for (i = 0; i < ARRAY_COUNT(sRows); i++)
    {
        if (sRows[i].section != section || !RowVisible(&sRows[i]))
            continue;
        if (sRows[i].kind == RK_HEADER)
        {
            // a header with nothing visible under it is hidden too
            u32 j;
            bool32 any = FALSE;
            for (j = i + 1; j < ARRAY_COUNT(sRows) && sRows[j].section == section && sRows[j].kind != RK_HEADER && !any; j++)
                any = RowVisible(&sRows[j]);
            if (!any)
                continue;
        }
        out[n++] = &sRows[i];
    }
    return n;
}

static bool32 Selectable(const struct RhRow *r)
{
    return r->kind != RK_HEADER;
}

static s32 GetValue(const struct RhRow *r)
{
    const u8 *p = ((const u8 *)S) + r->offset;
    if (r->size == 2)
        return *(const s16 *)p;
    return *p;
}

static void SetValue(const struct RhRow *r, s32 v)
{
    u8 *p = ((u8 *)S) + r->offset;
    if (r->size == 2)
        *(s16 *)p = v;
    else
        *p = v;
    if (r->section != SEC_GENERAL && r->section != SEC_MISC && r->section != SEC_GRAPHICS && r->section != SEC_BEGIN)
        S->enabled = TRUE;             // touching a randomizer option switches the randomizer on
}

// Choice rows whose value is "0 = Off, 1..9 = Gen N" (updateBaseStatsGen etc.) or "1..9 = Gen" (mechanicsGen).
static bool32 IsGenRow(const struct RhRow *r)
{
    return r->choices == sMechGen;
}
static bool32 GenRowHasOff(const struct RhRow *r)
{
    return r->offset != OFS(mechanicsGen);
}
// expCurve uses 0 = off as well.
static bool32 ChoiceHasOff(const struct RhRow *r)
{
    return (IsGenRow(r) && GenRowHasOff(r)) || r->offset == OFS(expCurve);
}

static u32 ChoiceCount(const struct RhRow *r)
{
    if (r->choices == NULL)
        return gRhPlayerGraphicsCount;
    return r->choiceCount + (ChoiceHasOff(r) ? 1 : 0);
}

static const u8 *ChoiceText(const struct RhRow *r, s32 v)
{
    if (r->choices == NULL)
        return gRhPlayerGraphicsNames[v < gRhPlayerGraphicsCount ? v : 0];
    if (IsGenRow(r))
    {
        if (GenRowHasOff(r))
            return v == 0 ? sOffOn[0] : sMechGen[min(v, 9) - 1];
        return sMechGen[min(max(v, 1), 9) - 1];
    }
    if (ChoiceHasOff(r))
        return v == 0 ? sOffOn[0] : r->choices[min(v, r->choiceCount) - 1];
    return r->choices[min(v, r->choiceCount - 1)];
}

static void ChangeChoice(const struct RhRow *r, s32 dir)
{
    s32 v = GetValue(r);
    if (IsGenRow(r) && !GenRowHasOff(r))
    {
        v += dir;
        if (v < 1) v = 9;
        if (v > 9) v = 1;
    }
    else
    {
        s32 n = ChoiceCount(r);
        v = (v + dir + n) % n;
        // FVX: "Randomize Zone Themes" needs zones ("1 In Whole Game" would make the whole game one type)
        if (r->offset == OFS(wildTypeRestriction) && v == 1 && S->wildZone == 0)
            v = (v + dir + n) % n;
    }
    SetValue(r, v);
    if (r->offset == OFS(wildZone) && S->wildZone == 0 && S->wildTypeRestriction == 1)
        S->wildTypeRestriction = 0;
}

static void ChangeSlider(const struct RhRow *r, s32 dir)
{
    s32 v = GetValue(r);
    if (r->fmt == SF_LEVEL && r->min == 0)
    {
        // "Make evolutions easier": 0 = off, otherwise 30..55
        if (v == 0)
            v = (dir > 0) ? 30 : 55;
        else
        {
            v += dir * r->step;
            if (v < 30 || v > 55)
                v = 0;
        }
    }
    else
    {
        v += dir * r->step;
        if (v < r->min) v = r->max;
        if (v > r->max) v = r->min;
    }
    SetValue(r, v);
}

static void FormatValue(const struct RhRow *r, u8 *dst)
{
    s32 v = GetValue(r);
    dst[0] = EOS;
    switch (r->kind)
    {
    case RK_CHOICE:
    case RK_TOGGLE:
        StringCopy(dst, ChoiceText(r, v));
        break;
    case RK_SLIDER:
        if (r->fmt == SF_LEVEL && r->min == 0 && v == 0)
        {
            StringCopy(dst, sOffOn[0]);
            break;
        }
        if (r->fmt == SF_LEVEL)
            dst = StringCopy(dst, CS("Lv. "));
        if (r->fmt == SF_SIGNED_PERCENT && v > 0)
            *dst++ = CHAR_PLUS, *dst = EOS;
        if (v < 0)
            *dst++ = CHAR_HYPHEN, *dst = EOS, v = -v;
        dst = ConvertIntToDecimalStringN(dst, v, STR_CONV_MODE_LEFT_ALIGN, 3);
        if (r->fmt == SF_PERCENT || r->fmt == SF_SIGNED_PERCENT)
            StringCopy(dst, CS("%"));
        break;
    case RK_TEXT:
        switch (r->fmt)
        {
        case TF_SEED:
            StringCopy(dst, S->seedText[0] ? S->seedText : CS("(random)"));
            break;
        case TF_STARTER1: case TF_STARTER2: case TF_STARTER3:
        {
            u16 sp = S->customStarters[r->fmt - TF_STARTER1];
            StringCopy(dst, sp ? GetSpeciesName(sp) : CS("(random)"));
            break;
        }
        case TF_TYPE:
            StringCopy(dst, S->starterSingleType ? gTypesInfo[S->starterSingleType].name : CS("(random)"));
            break;
        case TF_BST_MIN:
            ConvertIntToDecimalStringN(dst, S->starterBstMin, STR_CONV_MODE_LEFT_ALIGN, 3);
            break;
        case TF_BST_MAX:
            ConvertIntToDecimalStringN(dst, S->starterBstMax, STR_CONV_MODE_LEFT_ALIGN, 3);
            break;
        }
        break;
    }
}

static const u8 *ActionValueText(u32 act);
static void DrawCodeScreen(void);
static void DrawCodeDesc(void);

static void DrawHeader(void)
{
    u8 buf[48];
    FillWindowPixelBuffer(WIN_HEADER, PIXEL_FILL(C_TRANSPARENT));
    FillWindowPixelRect(WIN_HEADER, PIXEL_FILL(C_HEADBAR), 0, 0, 240, 15);
    AddTextPrinterParameterized3(WIN_HEADER, FONT_NORMAL, 4, 1, sColAccent, TEXT_SKIP_DRAW, CS("="));
    AddTextPrinterParameterized3(WIN_HEADER, FONT_NORMAL, 14, 1, sColText, TEXT_SKIP_DRAW, sSectionNames[sSection]);
    StringCopy(buf, CS("SELECT Menu  START Begin"));
    if (14 + GetStringWidth(FONT_NORMAL, sSectionNames[sSection], 0) + 6 > GetStringRightAlignXOffset(FONT_SMALL, buf, 236))
        StringCopy(buf, CS("SELECT Menu"));
    AddTextPrinterParameterized3(WIN_HEADER, FONT_SMALL, GetStringRightAlignXOffset(FONT_SMALL, buf, 236), 3, sColDim, TEXT_SKIP_DRAW, buf);
    CopyWindowToVram(WIN_HEADER, COPYWIN_FULL);
}

static void DrawList(void)
{
    const struct RhRow *rows[ARRAY_COUNT(sRows)];
    u32 n = VisibleRows(sSection, rows), i;
    bool32 dimAll = !S->enabled && sSection != SEC_GENERAL && sSection != SEC_MISC && sSection != SEC_GRAPHICS && sSection != SEC_BEGIN
                   && sSection != SEC_CODES;

    FillWindowPixelBuffer(WIN_LIST, PIXEL_FILL(C_PANEL));
    for (i = 0; i < LIST_ROWS && sScroll + i < n; i++)
    {
        const struct RhRow *r = rows[sScroll + i];
        u32 y = i * ROW_H;
        u32 x = 8 + r->indent * 8;
        u8 buf[40];
        bool32 selected = (sScroll + i == sRow);
        if (selected)
        {
            FillWindowPixelRect(WIN_LIST, PIXEL_FILL(C_SEL), 0, y, 240, ROW_H);
            FillWindowPixelRect(WIN_LIST, PIXEL_FILL(C_ACCENT), 0, y, 3, ROW_H);
        }
        if (r->kind == RK_HEADER)
        {
            AddTextPrinterParameterized3(WIN_LIST, FONT_NORMAL, 6, y + 1, sColHeader, TEXT_SKIP_DRAW, r->label);
            continue;
        }
        if (r->kind == RK_ACTION)
        {
            if (r->fmt == ACT_BEGIN)
                AddTextPrinterParameterized3(WIN_LIST, FONT_NORMAL, 88, y + 1, sColOk, TEXT_SKIP_DRAW, CS("▶ BEGIN RUN"));
            else
            {
                const u8 *v = ActionValueText(r->fmt);
                AddTextPrinterParameterized3(WIN_LIST, FONT_NORMAL, x, y + 1, sColText, TEXT_SKIP_DRAW, r->label);
                AddTextPrinterParameterized3(WIN_LIST, FONT_NORMAL, GetStringRightAlignXOffset(FONT_NORMAL, v, 228), y + 1,
                                             (r->fmt >= ACT_LOAD_1 && r->fmt <= ACT_LOAD_3 && v[0] == CHAR_E) ? sColDim : sColValue,
                                             TEXT_SKIP_DRAW, v);
            }
            continue;
        }
        AddTextPrinterParameterized3(WIN_LIST, FONT_NORMAL, x, y + 1, dimAll ? sColDim : sColText, TEXT_SKIP_DRAW, r->label);
        FormatValue(r, buf);
        if (r->kind == RK_CHOICE && r->choices != sOffOn)
        {
            // "‹ value ›" for multi-choice rows
            u8 tmp[44];
            StringCopy(tmp, CS("{LEFT_ARROW} "));
            StringAppend(tmp, buf);
            StringAppend(tmp, CS(" {RIGHT_ARROW}"));
            StringCopy(buf, tmp);
        }
        {
            // a value that would run into its label is printed in the narrow font
            u32 font = FONT_NORMAL;
            if (x + GetStringWidth(FONT_NORMAL, r->label, 0) + 4 + GetStringWidth(FONT_NORMAL, buf, 0) > 228)
                font = FONT_SMALL;
            AddTextPrinterParameterized3(WIN_LIST, font, GetStringRightAlignXOffset(font, buf, 228), y + 1,
                                         dimAll ? sColDim : (r->kind == RK_TEXT ? sColAccent : sColValue), TEXT_SKIP_DRAW, buf);
        }
    }
    if (sScroll > 0)
        AddTextPrinterParameterized3(WIN_LIST, FONT_SMALL, 233, 0, sColDim, TEXT_SKIP_DRAW, CS("{UP_ARROW}"));
    if (sScroll + LIST_ROWS < n)
        AddTextPrinterParameterized3(WIN_LIST, FONT_SMALL, 233, LIST_ROWS * ROW_H - 10, sColDim, TEXT_SKIP_DRAW, CS("{DOWN_ARROW}"));
    CopyWindowToVram(WIN_LIST, COPYWIN_GFX);
}

#ifndef RELEASE
// Self-test: every description line fits the description box, and every label leaves room for its widest value.
// Returns the number of problems; *first gets the index of the first bad row.
u32 RH_DebugMenuRowCount(void)
{
    return ARRAY_COUNT(sRows);
}

// 0 = ok, 1 = description too wide, 2 = label runs into the value
u32 RH_DebugMenuRowCheck(u32 i)
{
    const struct RhRow *r = &sRows[i];
    u32 labelEnd = 8 + r->indent * 8 + (r->label ? GetStringWidth(FONT_NORMAL, r->label, 0) : 0);
    u32 widest = 0, k;
    u8 buf[48];
    if (r->desc != NULL && GetStringWidth(FONT_NORMAL, r->desc, 0) > 232 - 8)
        return 1;
    if (r->kind == RK_CHOICE)
    {
        for (k = 0; k < ChoiceCount(r); k++)
        {
            StringCopy(buf, ChoiceText(r, k));
            if (r->choices != sOffOn)
            {
                u8 tmp[48];
                StringCopy(tmp, CS("{LEFT_ARROW} "));
                StringAppend(tmp, buf);
                StringAppend(tmp, CS(" {RIGHT_ARROW}"));
                StringCopy(buf, tmp);
            }
            // the list switches to the narrow font when the normal one doesn't fit
            widest = max(widest, (u32)min(GetStringWidth(FONT_NORMAL, buf, 0),
                         labelEnd + 4 + GetStringWidth(FONT_NORMAL, buf, 0) > 228 ? GetStringWidth(FONT_SMALL, buf, 0) : 999));
        }
    }
    else if (r->kind == RK_SLIDER)
    {
        widest = GetStringWidth(FONT_NORMAL, CS("+150%"), 0);
    }
    else if (r->kind == RK_TEXT)
    {
        widest = GetStringWidth(FONT_NORMAL, CS("CRABOMINABLE"), 0);   // longest species name
    }
    if (r->kind != RK_HEADER && r->kind != RK_ACTION && labelEnd + 4 + widest > 228)
        return 2;
    return 0;
}
#endif

static void DrawDesc(void)
{
    const struct RhRow *rows[ARRAY_COUNT(sRows)];
    u32 n = VisibleRows(sSection, rows);
    FillWindowPixelBuffer(WIN_DESC, PIXEL_FILL(C_HEADBAR));
    if (sConfirm)
    {
        const u8 *q;
        if (sPendingAction >= ACT_SAVE_1 && sPendingAction <= ACT_SAVE_3)
            q = CS("Overwrite this preset?");
        else if (sPendingAction == ACT_DEFAULTS)
            q = CS("Reset every option to its default?");
        else
            q = S->enabled ? CS("Begin a RANDOMIZED run?") : CS("Begin a normal (not randomized) run?");
        AddTextPrinterParameterized3(WIN_DESC, FONT_NORMAL, 8, 1, sColText, TEXT_SKIP_DRAW, q);
        FillWindowPixelRect(WIN_DESC, PIXEL_FILL(sConfirm == 1 ? C_SEL : C_HEADBAR), 60, 16, 48, 14);
        FillWindowPixelRect(WIN_DESC, PIXEL_FILL(sConfirm == 2 ? C_SEL : C_HEADBAR), 132, 16, 48, 14);
        AddTextPrinterParameterized3(WIN_DESC, FONT_NORMAL, 72, 17, sConfirm == 1 ? sColOk : sColDim, TEXT_SKIP_DRAW, CS("YES"));
        AddTextPrinterParameterized3(WIN_DESC, FONT_NORMAL, 148, 17, sConfirm == 2 ? sColWarn : sColDim, TEXT_SKIP_DRAW, CS("NO"));
    }
    else if (sMessage != NULL)
    {
        AddTextPrinterParameterized3(WIN_DESC, FONT_NORMAL, 8, 1, sColWarn, TEXT_SKIP_DRAW, sMessage);
    }
    else if (sRow < n && rows[sRow]->desc != NULL)
    {
        AddTextPrinterParameterized3(WIN_DESC, FONT_NORMAL, 8, 1, sColText, TEXT_SKIP_DRAW, rows[sRow]->desc);
    }
    CopyWindowToVram(WIN_DESC, COPYWIN_GFX);
}

static void DrawMenu(void)
{
    u32 i, top = 0;
    if (!sMenuOpen)
    {
        ClearWindowTilemap(WIN_MENU);
        CopyBgTilemapBufferToVram(1);
        return;
    }
    if (sMenuCursor >= MENU_ROWS)
        top = sMenuCursor - MENU_ROWS + 1;
    FillWindowPixelBuffer(WIN_MENU, PIXEL_FILL(C_HEADBAR));
    FillWindowPixelRect(WIN_MENU, PIXEL_FILL(C_ACCENT), 0, 0, 176, 2);
    for (i = 0; i < MENU_ROWS && top + i < SEC_COUNT; i++)
    {
        u32 sec = top + i, y = 4 + i * 15;
        if (sec == sMenuCursor)
            FillWindowPixelRect(WIN_MENU, PIXEL_FILL(C_SEL), 0, y - 1, 176, 15);
        AddTextPrinterParameterized3(WIN_MENU, FONT_NORMAL, 8, y, sec == SEC_BEGIN ? sColOk : (sec == sSection ? sColValue : sColText),
                                     TEXT_SKIP_DRAW, sSectionNames[sec]);
    }
    PutWindowTilemap(WIN_MENU);
    CopyWindowToVram(WIN_MENU, COPYWIN_FULL);
}

static void DrawAll(void)
{
    if (sCodeMode)
    {
        DrawHeader();
        DrawCodeScreen();
        DrawCodeDesc();
        DrawMenu();
        return;
    }
    DrawHeader();
    DrawList();
    DrawDesc();
    DrawMenu();
}

// Keeps sRow on a selectable row and inside the scroll window.
static void FixCursor(s32 dir)
{
    const struct RhRow *rows[ARRAY_COUNT(sRows)];
    u32 n = VisibleRows(sSection, rows), guard;
    if (n == 0)
    {
        sRow = sScroll = 0;
        return;
    }
    if (sRow >= n)
        sRow = n - 1;
    for (guard = 0; guard < n && !Selectable(rows[sRow]); guard++)
        sRow = (dir < 0) ? (sRow == 0 ? n - 1 : sRow - 1) : (sRow + 1) % n;
    if (sRow < sScroll)
        sScroll = sRow;
    if (sRow >= sScroll + LIST_ROWS)
        sScroll = sRow - LIST_ROWS + 1;
    if (sScroll > 0 && sRow == sScroll && !Selectable(rows[sScroll - 1]) == FALSE)
        ;
    if (sRow > 0 && sScroll == sRow && rows[sRow - 1]->kind == RK_HEADER)
        sScroll--;                                   // show the section header above the first row
}

static void GoToSection(u32 sec)
{
    sSection = sec;
    sRow = 0;
    sScroll = 0;
    FixCursor(1);
}

// ---------------------------------------------------------------------------
// Seed / text handling
// ---------------------------------------------------------------------------
static u32 HashText(const u8 *s)
{
    u32 h = 2166136261u;
    while (*s != EOS)
        h = (h ^ *s++) * 16777619u;
    return h ? h : 1;
}

static void RandomSeedText(void)
{
    static const u8 sChars[] = _("ABCDEFGHJKLMNPQRSTUVWXYZ23456789");
    u32 i, r = Random32() ^ (gMain.vblankCounter1 * 2654435761u);
    for (i = 0; i < 8; i++)
    {
        S->seedText[i] = sChars[r % 32];
        r = r / 32 ^ (Random32() << 3);
    }
    S->seedText[8] = EOS;
    S->seed = HashText(S->seedText);
}

static bool32 NamesEqual(const u8 *a, const u8 *b)
{
    for (; *a != EOS && *b != EOS; a++, b++)
    {
        u8 ca = *a, cb = *b;
        if (ca >= CHAR_a && ca <= CHAR_z) ca -= CHAR_a - CHAR_A;
        if (cb >= CHAR_a && cb <= CHAR_z) cb -= CHAR_a - CHAR_A;
        if (ca == CHAR_e_ACUTE) ca = CHAR_E;
        if (cb == CHAR_e_ACUTE) cb = CHAR_E;
        if (ca != cb)
            return FALSE;
    }
    return *a == EOS && *b == EOS;
}

static u16 FindSpeciesByName(const u8 *name)
{
    u32 i;
    for (i = 1; i < NUM_SPECIES; i++)
        if (IsSpeciesEnabled(i) && NamesEqual(GetSpeciesName(i), name))
            return i;
    return SPECIES_NONE;
}

static u8 FindTypeByName(const u8 *name)
{
    u32 i;
    for (i = 1; i < NUMBER_OF_MON_TYPES; i++)
        if (i != TYPE_MYSTERY && i != TYPE_STELLAR && NamesEqual(gTypesInfo[i].name, name))
            return i;
    return TYPE_NONE;
}

static u16 ParseNumber(const u8 *s)
{
    u32 v = 0;
    for (; *s != EOS; s++)
        if (*s >= CHAR_0 && *s <= CHAR_9)
            v = v * 10 + (*s - CHAR_0);
    return min(v, 999);
}


// ---------------------------------------------------------------------------
// Settings codes and presets
// A code is every option row's value bit-packed in row order, plus the seed, the custom starters / type / BST
// fields, a format version and a checksum, written with 32 unambiguous characters (5 bits each).
// Presets keep the same packed bits in a spare flash sector (the Trainer Hill sector, unused in FireRed), so
// they are independent of the save file and survive starting a new game.
// ---------------------------------------------------------------------------
#define CODE_VERSION      1
#define CODE_MAX_BYTES    96
#define CODE_MAX_CHARS    (CODE_MAX_BYTES * 8 / 5)
#define CODE_CHECK_BITS   10
#define PRESET_COUNT      3
#define PRESET_MAGIC      0x52485053   // "RHPS"

static const u8 sCodeChars[] = _("ABCDEFGHJKLMNPQRSTUVWXYZ23456789");

struct RhPresetSector
{
    u32 magic;
    u16 bits[PRESET_COUNT];            // 0 = empty slot
    u8 data[PRESET_COUNT][CODE_MAX_BYTES];
};

static EWRAM_DATA u8 sCode[CODE_MAX_CHARS + 1] = {0};

static u32 BitsFor(u32 maxValue)
{
    u32 b = 0;
    while ((1u << b) <= maxValue)
        b++;
    return b;
}

static void PutBits(u8 *buf, u32 *pos, u32 value, u32 n)
{
    u32 i;
    for (i = 0; i < n; i++, (*pos)++)
    {
        if (*pos >= CODE_MAX_BYTES * 8)
            return;
        if (value & (1u << i))
            buf[*pos / 8] |= 1 << (*pos % 8);
        else
            buf[*pos / 8] &= ~(1 << (*pos % 8));
    }
}

static u32 GetBits(const u8 *buf, u32 *pos, u32 n)
{
    u32 i, v = 0;
    for (i = 0; i < n; i++, (*pos)++)
        if (*pos < CODE_MAX_BYTES * 8 && (buf[*pos / 8] & (1 << (*pos % 8))))
            v |= 1u << i;
    return v;
}

// Encoded range of an option row: its value is stored as (value - base) in BitsFor(span) bits.
static bool32 RowRange(const struct RhRow *r, s32 *base, u32 *span)
{
    switch (r->kind)
    {
    case RK_CHOICE:
    case RK_TOGGLE:
        *base = 0;
        *span = (IsGenRow(r) && !GenRowHasOff(r)) ? 9 : ChoiceCount(r) - 1;
        return TRUE;
    case RK_SLIDER:
        *base = min(r->min, 0);
        *span = r->max - *base;
        return TRUE;
    }
    return FALSE;
}

static s32 RowGet(const struct RhRow *r, const struct RhSettings *st)
{
    const u8 *p = ((const u8 *)st) + r->offset;
    return r->size == 2 ? *(const s16 *)p : *p;
}

static void RowSet(const struct RhRow *r, struct RhSettings *st, s32 v)
{
    u8 *p = ((u8 *)st) + r->offset;
    if (r->size == 2)
        *(s16 *)p = v;
    else
        *p = v;
}

static bool32 SeedTextIsCode(const u8 *t)
{
    u32 i, k;
    for (i = 0; t[i] != EOS; i++)
    {
        for (k = 0; k < 32 && sCodeChars[k] != t[i]; k++)
            ;
        if (k == 32)
            return FALSE;
    }
    return i > 0 && i <= RH_SEED_TEXT_LENGTH;
}

static u32 CodeCheck(const u8 *buf, u32 bits)
{
    u32 h = 2166136261u, i, pos = 0;
    for (i = 0; i < bits; i++)
        h = (h ^ GetBits(buf, &pos, 1) ^ (i << 1)) * 16777619u;
    return (h ^ (h >> 13)) & ((1 << CODE_CHECK_BITS) - 1);
}

static bool32 ExtrasAreDefault(const struct RhSettings *st, const struct RhSettings *def)
{
    return st->customStarters[0] == SPECIES_NONE && st->customStarters[1] == SPECIES_NONE && st->customStarters[2] == SPECIES_NONE
        && st->starterSingleType == def->starterSingleType
        && st->starterBstMin == def->starterBstMin && st->starterBstMax == def->starterBstMax;
}

static u32 RowCode(const struct RhRow *r, const struct RhSettings *st, s32 base, u32 span)
{
    return min((u32)(RowGet(r, st) - base), span);
}

// Packs st into buf; returns the number of bits (payload + checksum).
// Rows are stored either all in order, or (usually shorter) as a list of the rows that differ from the defaults.
static u32 PackSettings(const struct RhSettings *st, u8 *buf)
{
    u32 pos = 0, i, fullBits = 0, sparseBits, changed = 0;
    const u32 idxBits = BitsFor(ARRAY_COUNT(sRows) - 1);
    struct RhSettings *def = AllocZeroed(sizeof(*def));
    s32 base;
    u32 span;
    if (def == NULL)
        return 0;
    RH_SetDefaultSettings(def);
    memset(buf, 0, CODE_MAX_BYTES);
    PutBits(buf, &pos, CODE_VERSION, 3);
    if (SeedTextIsCode(st->seedText))
    {
        // the usual random seed text: keep the text itself (the seed is its hash)
        u32 len = StringLength(st->seedText);
        PutBits(buf, &pos, 1, 1);
        PutBits(buf, &pos, len - 1, 4);
        for (i = 0; i < len; i++)
        {
            u32 k;
            for (k = 0; sCodeChars[k] != st->seedText[i]; k++)
                ;
            PutBits(buf, &pos, k, 5);
        }
    }
    else
    {
        PutBits(buf, &pos, 0, 1);
        PutBits(buf, &pos, st->seed, 32);
    }
    for (i = 0; i < ARRAY_COUNT(sRows); i++)
    {
        if (!RowRange(&sRows[i], &base, &span))
            continue;
        fullBits += BitsFor(span);
        if (RowCode(&sRows[i], st, base, span) != RowCode(&sRows[i], def, base, span))
            changed++;
    }
    sparseBits = idxBits;
    for (i = 0; i < ARRAY_COUNT(sRows); i++)
        if (RowRange(&sRows[i], &base, &span) && RowCode(&sRows[i], st, base, span) != RowCode(&sRows[i], def, base, span))
            sparseBits += idxBits + BitsFor(span);
    if (sparseBits < fullBits)
    {
        PutBits(buf, &pos, 1, 1);
        PutBits(buf, &pos, changed, idxBits);
        for (i = 0; i < ARRAY_COUNT(sRows); i++)
        {
            if (RowRange(&sRows[i], &base, &span) && RowCode(&sRows[i], st, base, span) != RowCode(&sRows[i], def, base, span))
            {
                PutBits(buf, &pos, i, idxBits);
                PutBits(buf, &pos, RowCode(&sRows[i], st, base, span), BitsFor(span));
            }
        }
    }
    else
    {
        PutBits(buf, &pos, 0, 1);
        for (i = 0; i < ARRAY_COUNT(sRows); i++)
            if (RowRange(&sRows[i], &base, &span))
                PutBits(buf, &pos, RowCode(&sRows[i], st, base, span), BitsFor(span));
    }
    if (ExtrasAreDefault(st, def))
    {
        PutBits(buf, &pos, 0, 1);
    }
    else
    {
        PutBits(buf, &pos, 1, 1);
        for (i = 0; i < 3; i++)
            PutBits(buf, &pos, st->customStarters[i] < NUM_SPECIES ? st->customStarters[i] : 0, BitsFor(NUM_SPECIES - 1));
        PutBits(buf, &pos, st->starterSingleType, BitsFor(NUMBER_OF_MON_TYPES - 1));
        PutBits(buf, &pos, min(st->starterBstMin, 1023), 10);
        PutBits(buf, &pos, min(st->starterBstMax, 1023), 10);
    }
    Free(def);
    i = CodeCheck(buf, pos);
    PutBits(buf, &pos, i, CODE_CHECK_BITS);
    return pos;
}

// Unpacks buf (bits long) into st (which keeps its other fields). Returns FALSE for a wrong or damaged code.
static bool32 UnpackSettings(struct RhSettings *st, const u8 *buf, u32 bits)
{
    u32 pos = 0, i, payload, v;
    const u32 idxBits = BitsFor(ARRAY_COUNT(sRows) - 1);
    struct RhSettings *tmp, *def;
    s32 base;
    u32 span;
    if (bits < 3 + CODE_CHECK_BITS || GetBits(buf, &pos, 3) != CODE_VERSION)
        return FALSE;
    tmp = AllocZeroed(sizeof(*tmp));
    def = AllocZeroed(sizeof(*def));
    if (tmp == NULL || def == NULL)
    {
        TRY_FREE_AND_SET_NULL(tmp);
        TRY_FREE_AND_SET_NULL(def);
        return FALSE;
    }
    RH_SetDefaultSettings(def);
    *tmp = *st;
    if (GetBits(buf, &pos, 1))
    {
        u32 len = GetBits(buf, &pos, 4) + 1;
        for (i = 0; i < len && i < RH_SEED_TEXT_LENGTH; i++)
            tmp->seedText[i] = sCodeChars[GetBits(buf, &pos, 5)];
        tmp->seedText[i] = EOS;
        tmp->seed = HashText(tmp->seedText);
    }
    else
    {
        tmp->seed = GetBits(buf, &pos, 32);
        if (tmp->seed == 0)
            tmp->seed = 1;
        StringCopy(tmp->seedText, CS("(code)"));
    }
    // every option starts from its default (sparse codes only list the changed ones)
    for (i = 0; i < ARRAY_COUNT(sRows); i++)
        if (RowRange(&sRows[i], &base, &span))
            RowSet(&sRows[i], tmp, RowGet(&sRows[i], def));
    if (GetBits(buf, &pos, 1))
    {
        u32 count = GetBits(buf, &pos, idxBits), k;
        for (k = 0; k < count && pos < bits; k++)
        {
            i = GetBits(buf, &pos, idxBits);
            if (i >= ARRAY_COUNT(sRows) || !RowRange(&sRows[i], &base, &span))
            {
                pos = bits + 1;       // damaged
                break;
            }
            v = GetBits(buf, &pos, BitsFor(span));
            RowSet(&sRows[i], tmp, base + min(v, span));
        }
    }
    else
    {
        for (i = 0; i < ARRAY_COUNT(sRows); i++)
        {
            if (RowRange(&sRows[i], &base, &span))
            {
                v = GetBits(buf, &pos, BitsFor(span));    // (min() would read the bits twice)
                RowSet(&sRows[i], tmp, base + min(v, span));
            }
        }
    }
    for (i = 0; i < 3; i++)
        tmp->customStarters[i] = SPECIES_NONE;
    tmp->starterSingleType = def->starterSingleType;
    tmp->starterBstMin = def->starterBstMin;
    tmp->starterBstMax = def->starterBstMax;
    if (GetBits(buf, &pos, 1))
    {
        for (i = 0; i < 3; i++)
        {
            v = GetBits(buf, &pos, BitsFor(NUM_SPECIES - 1));
            tmp->customStarters[i] = (v < NUM_SPECIES && IsSpeciesEnabled(v)) ? v : SPECIES_NONE;
        }
        v = GetBits(buf, &pos, BitsFor(NUMBER_OF_MON_TYPES - 1));
        tmp->starterSingleType = v < NUMBER_OF_MON_TYPES ? v : TYPE_NONE;
        v = GetBits(buf, &pos, 10);
        tmp->starterBstMin = min(v, 999);
        v = GetBits(buf, &pos, 10);
        tmp->starterBstMax = min(v, 999);
    }
    payload = pos;
    Free(def);
    // the checksum covers the payload; a code must be exactly as long as its payload + checksum (+ padding)
    if (payload + CODE_CHECK_BITS > bits || bits - (payload + CODE_CHECK_BITS) >= 5
     || GetBits(buf, &pos, CODE_CHECK_BITS) != CodeCheck(buf, payload))
    {
        Free(tmp);
        return FALSE;
    }
    tmp->version = RH_SETTINGS_VERSION;
    *st = *tmp;
    Free(tmp);
    return TRUE;
}

static u32 SettingsToCode(const struct RhSettings *st, u8 *chars)
{
    u8 buf[CODE_MAX_BYTES];
    u32 bits = PackSettings(st, buf), n = (bits + 4) / 5, i, pos = 0;
    for (i = 0; i < n; i++)
        chars[i] = GetBits(buf, &pos, 5);
    return n;
}

static bool32 CodeToSettings(struct RhSettings *st, const u8 *chars, u32 n)
{
    u8 buf[CODE_MAX_BYTES];
    u32 i, pos = 0;
    if (n == 0 || n > CODE_MAX_CHARS)
        return FALSE;
    memset(buf, 0, sizeof(buf));
    for (i = 0; i < n; i++)
        PutBits(buf, &pos, chars[i], 5);
    return UnpackSettings(st, buf, n * 5);
}

#ifndef RELEASE
// Self-test hooks: code round trip on any settings struct.
u32 RH_DebugSettingsToCode(const struct RhSettings *st, u8 *chars) { return SettingsToCode(st, chars); }
bool32 RH_DebugCodeToSettings(struct RhSettings *st, const u8 *chars, u32 n) { return CodeToSettings(st, chars, n); }
u32 RH_DebugSettingsCodeMaxChars(void) { return CODE_MAX_CHARS; }
// Every option row gets a random legal value; returns how many rows differ between a and b.
void RH_DebugRandomSettings(struct RhSettings *st, u32 salt)
{
    u32 i;
    for (i = 0; i < ARRAY_COUNT(sRows); i++)
    {
        s32 base;
        u32 span;
        if (RowRange(&sRows[i], &base, &span))
            RowSet(&sRows[i], st, base + (RH_Hash(0x5E77, salt, i) % (span + 1)));
    }
}
u32 RH_DebugCompareSettings(const struct RhSettings *a, const struct RhSettings *b)
{
    u32 i, diff = 0;
    s32 base;
    u32 span;
    for (i = 0; i < ARRAY_COUNT(sRows); i++)
        if (RowRange(&sRows[i], &base, &span) && RowGet(&sRows[i], a) != RowGet(&sRows[i], b))
            diff++;
    return diff;
}
#endif

// Presets ------------------------------------------------------------------
static struct RhPresetSector *ReadPresets(void)
{
    u8 *sector = Alloc(SECTOR_SIZE);
    struct RhPresetSector *p = (struct RhPresetSector *)sector;
    if (sector == NULL)
        return NULL;
    if (TryReadSpecialSaveSector(SECTOR_ID_TRAINER_HILL, sector) != SAVE_STATUS_OK || p->magic != PRESET_MAGIC)
    {
        memset(sector, 0, SECTOR_SIZE);
        p->magic = PRESET_MAGIC;
    }
    return p;
}

static void RefreshPresetFlags(void)
{
    struct RhPresetSector *p = ReadPresets();
    u32 i;
    sPresetUsed = 0;
    if (p == NULL)
        return;
    for (i = 0; i < PRESET_COUNT; i++)
        if (p->bits[i] != 0)
            sPresetUsed |= 1 << i;
    Free(p);
}

static bool32 SavePreset(u32 slot)
{
    struct RhPresetSector *p = ReadPresets();
    bool32 ok;
    if (p == NULL)
        return FALSE;
    p->bits[slot] = PackSettings(S, p->data[slot]);
    ok = (TryWriteSpecialSaveSector(SECTOR_ID_TRAINER_HILL, (u8 *)p) == SAVE_STATUS_OK);
    Free(p);
    RefreshPresetFlags();
    return ok;
}

static bool32 LoadPreset(u32 slot)
{
    struct RhPresetSector *p = ReadPresets();
    bool32 ok;
    if (p == NULL)
        return FALSE;
    ok = p->bits[slot] != 0 && UnpackSettings(S, p->data[slot], p->bits[slot]);
    Free(p);
    return ok;
}

// Code screens -------------------------------------------------------------
#define CODE_PER_LINE 20

static void DrawCodeChars(u32 y0, bool32 cursor)
{
    u32 i;
    for (i = 0; i <= sCodeLen && i < CODE_MAX_CHARS; i++)
    {
        u32 line = i / CODE_PER_LINE, col = i % CODE_PER_LINE;
        u32 x = 10 + col * 10 + (col / 5) * 8, y = y0 + line * 15;
        u8 str[2];
        if (i == sCodeLen)
        {
            if (cursor)
                AddTextPrinterParameterized3(WIN_LIST, FONT_NORMAL, x, y, sColAccent, TEXT_SKIP_DRAW, CS("_"));
            break;
        }
        str[0] = sCodeChars[sCode[i] & 31];
        str[1] = EOS;
        AddTextPrinterParameterized3(WIN_LIST, FONT_NORMAL, x, y, sColValue, TEXT_SKIP_DRAW, str);
    }
}

static void DrawCodeScreen(void)
{
    u32 i;
    FillWindowPixelBuffer(WIN_LIST, PIXEL_FILL(C_PANEL));
    if (sCodeMode == 1)
    {
        AddTextPrinterParameterized3(WIN_LIST, FONT_NORMAL, 6, 1, sColHeader, TEXT_SKIP_DRAW, CS("Settings Code"));
        DrawCodeChars(24, FALSE);
    }
    else
    {
        AddTextPrinterParameterized3(WIN_LIST, FONT_NORMAL, 6, 1, sColHeader, TEXT_SKIP_DRAW, CS("Enter Settings Code"));
        DrawCodeChars(18, TRUE);
        for (i = 0; i < 32; i++)
        {
            u32 x = 8 + (i % 16) * 14, y = 80 + (i / 16) * 16;
            u8 str[2];
            if (i == sCodeCursor)
                FillWindowPixelRect(WIN_LIST, PIXEL_FILL(C_SEL), x - 3, y, 13, 15);
            str[0] = sCodeChars[i];
            str[1] = EOS;
            AddTextPrinterParameterized3(WIN_LIST, FONT_NORMAL, x, y + 1, i == sCodeCursor ? sColAccent : sColText, TEXT_SKIP_DRAW, str);
        }
    }
    CopyWindowToVram(WIN_LIST, COPYWIN_GFX);
}

static void DrawCodeDesc(void)
{
    FillWindowPixelBuffer(WIN_DESC, PIXEL_FILL(C_HEADBAR));
    if (sMessage != NULL)
        AddTextPrinterParameterized3(WIN_DESC, FONT_NORMAL, 8, 1, sColWarn, TEXT_SKIP_DRAW, sMessage);
    else if (sCodeMode == 1)
        AddTextPrinterParameterized3(WIN_DESC, FONT_NORMAL, 8, 1, sColText, TEXT_SKIP_DRAW,
                                     CS("Write it down or share it. Friends can\ntype it in to play the same game. B: back"));
    else
        AddTextPrinterParameterized3(WIN_DESC, FONT_NORMAL, 8, 1, sColText, TEXT_SKIP_DRAW,
                                     CS("A: type  B: delete  START: load code\nSELECT: cancel"));
    CopyWindowToVram(WIN_DESC, COPYWIN_GFX);
}

static void OpenCodeScreen(u32 mode)
{
    sCodeMode = mode;
    sMessage = NULL;
    if (mode == 1)
        sCodeLen = SettingsToCode(S, sCode);
    else
        sCodeLen = 0;
    sCodeCursor = 0;
    DrawCodeScreen();
    DrawCodeDesc();
}

static void DrawAll(void);

// Returns TRUE while the code screen handles the input.
static bool32 CodeScreenInput(void)
{
    if (sCodeMode == 0)
        return FALSE;
    if (sMessage != NULL && JOY_NEW(A_BUTTON | B_BUTTON | START_BUTTON | SELECT_BUTTON | DPAD_ANY))
    {
        sMessage = NULL;
        DrawCodeDesc();
        return TRUE;
    }
    if (sCodeMode == 1)
    {
        if (JOY_NEW(A_BUTTON | B_BUTTON | START_BUTTON | SELECT_BUTTON))
        {
            sCodeMode = 0;
            PlaySE(SE_SELECT);
            DrawAll();
        }
        return TRUE;
    }
    if (JOY_REPEAT(DPAD_LEFT))
        sCodeCursor = (sCodeCursor & 16) | ((sCodeCursor + 15) & 15);
    else if (JOY_REPEAT(DPAD_RIGHT))
        sCodeCursor = (sCodeCursor & 16) | ((sCodeCursor + 1) & 15);
    else if (JOY_REPEAT(DPAD_UP | DPAD_DOWN))
        sCodeCursor ^= 16;
    else if (JOY_REPEAT(A_BUTTON))
    {
        if (sCodeLen < CODE_MAX_CHARS)
            sCode[sCodeLen++] = sCodeCursor;
    }
    else if (JOY_REPEAT(B_BUTTON))
    {
        if (sCodeLen > 0)
            sCodeLen--;
    }
    else if (JOY_NEW(SELECT_BUTTON))
    {
        sCodeMode = 0;
        PlaySE(SE_SELECT);
        DrawAll();
        return TRUE;
    }
    else if (JOY_NEW(START_BUTTON))
    {
        if (CodeToSettings(S, sCode, sCodeLen))
        {
            sCodeMode = 0;
            PlaySE(SE_SUCCESS);
            DrawAll();
            sMessage = CS("Code loaded! Check the options, then\nBegin Run.");
            DrawDesc();
        }
        else
        {
            PlaySE(SE_FAILURE);
            sMessage = CS("That code doesn't work. Check every\ncharacter (and its length) and retry.");
            DrawCodeDesc();
        }
        return TRUE;
    }
    else
        return TRUE;
    PlaySE(SE_SELECT);
    DrawCodeScreen();
    return TRUE;
}

static const u8 *ActionValueText(u32 act)
{
    if (act >= ACT_SAVE_1 && act <= ACT_SAVE_3)
        return (sPresetUsed & (1 << (act - ACT_SAVE_1))) ? CS("In use") : CS("");
    if (act >= ACT_LOAD_1 && act <= ACT_LOAD_3)
        return (sPresetUsed & (1 << (act - ACT_LOAD_1))) ? CS("Saved") : CS("Empty");
    return CS("");
}

void CB2_InitRandomizerMenu(void);

static void CB2_ReturnFromNaming(void)
{
    const u8 *buf = sTextBuf;
    sMessage = NULL;
    switch (sEditingField)
    {
    case TF_SEED:
        if (buf[0] == EOS)
            RandomSeedText();
        else
        {
            StringCopyN(S->seedText, buf, RH_SEED_TEXT_LENGTH);
            S->seedText[RH_SEED_TEXT_LENGTH] = EOS;
            S->seed = HashText(S->seedText);
        }
        break;
    case TF_STARTER1: case TF_STARTER2: case TF_STARTER3:
        if (buf[0] == EOS)
            S->customStarters[sEditingField - TF_STARTER1] = SPECIES_NONE;
        else
        {
            u16 sp = FindSpeciesByName(buf);
            if (sp == SPECIES_NONE)
                sMessage = CS("No Pokémon has that name.\nCheck the spelling and try again.");
            else
                S->customStarters[sEditingField - TF_STARTER1] = sp;
        }
        break;
    case TF_TYPE:
        if (buf[0] == EOS)
            S->starterSingleType = TYPE_NONE;
        else if ((S->starterSingleType = FindTypeByName(buf)) == TYPE_NONE)
            sMessage = CS("That's not a type. Try e.g. FIRE,\nDRAGON or FAIRY. Blank = random.");
        break;
    case TF_BST_MIN:
        S->starterBstMin = ParseNumber(buf);
        break;
    case TF_BST_MAX:
        S->starterBstMax = ParseNumber(buf);
        break;
    }
    gMain.state = 0;
    SetMainCallback2(CB2_InitRandomizerMenu);
}

static void Task_OpenNaming(u8 taskId);

static void StartTextEntry(u8 taskId, u32 field)
{
    sEditingField = field;
    sTextBuf[0] = EOS;
    switch (field)
    {
    case TF_SEED:
        StringCopy(sTextBuf, S->seedText);
        gRhNamingTitle = CS("Seed (any text):");
        gRhNamingMaxChars = RH_SEED_TEXT_LENGTH;
        break;
    case TF_STARTER1: case TF_STARTER2: case TF_STARTER3:
        if (S->customStarters[field - TF_STARTER1])
            StringCopy(sTextBuf, GetSpeciesName(S->customStarters[field - TF_STARTER1]));
        gRhNamingTitle = CS("Starter Pokémon:");
        gRhNamingMaxChars = POKEMON_NAME_LENGTH;
        break;
    case TF_TYPE:
        if (S->starterSingleType)
            StringCopy(sTextBuf, gTypesInfo[S->starterSingleType].name);
        gRhNamingTitle = CS("Starter type:");
        gRhNamingMaxChars = 8;
        break;
    default:
        ConvertIntToDecimalStringN(sTextBuf, field == TF_BST_MIN ? S->starterBstMin : S->starterBstMax, STR_CONV_MODE_LEFT_ALIGN, 3);
        gRhNamingTitle = CS("Base stat total:");
        gRhNamingMaxChars = 3;
        break;
    }
    BeginNormalPaletteFade(PALETTES_ALL, 0, 0, 16, RGB_BLACK);
    gTasks[taskId].func = Task_OpenNaming;
}

static void Task_OpenNaming(u8 taskId)
{
    if (!gPaletteFade.active)
    {
        DestroyTask(taskId);
        FreeAllWindowBuffers();
        DoNamingScreen(NAMING_SCREEN_RH_TEXT, sTextBuf, 0, 0, 0, CB2_ReturnFromNaming);
    }
}

// ---------------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------------
static void MainCB2(void)
{
    RunTasks();
    AnimateSprites();
    BuildOamBuffer();
    UpdatePaletteFade();
}

static void VBlankCB(void)
{
    LoadOam();
    ProcessSpriteCopyRequests();
    TransferPlttBuffer();
}

static void Task_Input(u8 taskId);
static void Task_Begin(u8 taskId);

static void Task_FadeIn(u8 taskId)
{
    if (!gPaletteFade.active)
        gTasks[taskId].func = Task_Input;
}

static void Redraw(void)
{
    DrawList();
    DrawDesc();
}

// Runs a Codes & Presets action; confirmed = the YES/NO box was already answered.
static void DoAction(u32 act, bool32 confirmed)
{
    sMessage = NULL;
    switch (act)
    {
    case ACT_SHOW_CODE:
        PlaySE(SE_SELECT);
        OpenCodeScreen(1);
        DrawHeader();
        return;
    case ACT_ENTER_CODE:
        PlaySE(SE_SELECT);
        OpenCodeScreen(2);
        DrawHeader();
        return;
    case ACT_SAVE_1: case ACT_SAVE_2: case ACT_SAVE_3:
        if (!confirmed && (sPresetUsed & (1 << (act - ACT_SAVE_1))))
        {
            sPendingAction = act;
            sConfirm = 2;                 // default NO for overwriting
            PlaySE(SE_SELECT);
            DrawDesc();
            return;
        }
        if (SavePreset(act - ACT_SAVE_1))
        {
            PlaySE(SE_SAVE);
            sMessage = CS("Preset saved.");
        }
        else
        {
            PlaySE(SE_FAILURE);
            sMessage = CS("Couldn't save. Is the save type set\nto Flash 128K in your emulator?");
        }
        break;
    case ACT_LOAD_1: case ACT_LOAD_2: case ACT_LOAD_3:
        if (!(sPresetUsed & (1 << (act - ACT_LOAD_1))))
        {
            PlaySE(SE_FAILURE);
            sMessage = CS("This preset is empty.");
        }
        else if (LoadPreset(act - ACT_LOAD_1))
        {
            PlaySE(SE_SUCCESS);
            sMessage = CS("Preset loaded.");
        }
        else
        {
            PlaySE(SE_FAILURE);
            sMessage = CS("This preset is from another version\nand can't be loaded.");
        }
        break;
    case ACT_DEFAULTS:
        if (!confirmed)
        {
            sPendingAction = act;
            sConfirm = 2;
            PlaySE(SE_SELECT);
            DrawDesc();
            return;
        }
        {
            u8 seedText[RH_SEED_TEXT_LENGTH + 1];
            u32 seed = S->seed;
            StringCopy(seedText, S->seedText);
            RH_SetDefaultSettings(S);
            S->seed = seed;
            StringCopy(S->seedText, seedText);
        }
        PlaySE(SE_SELECT);
        sMessage = CS("Every option is back to its default.");
        break;
    }
    FixCursor(1);
    DrawAll();
}

static void Task_Input(u8 taskId)
{
    const struct RhRow *rows[ARRAY_COUNT(sRows)];
    u32 n = VisibleRows(sSection, rows);
    const struct RhRow *r = (sRow < n) ? rows[sRow] : NULL;

    if (CodeScreenInput())
        return;
    if (sMenuOpen)
    {
        if (JOY_NEW(DPAD_UP))
            sMenuCursor = (sMenuCursor + SEC_COUNT - 1) % SEC_COUNT;
        else if (JOY_NEW(DPAD_DOWN))
            sMenuCursor = (sMenuCursor + 1) % SEC_COUNT;
        else if (JOY_NEW(A_BUTTON))
        {
            sMenuOpen = FALSE;
            GoToSection(sMenuCursor);
            PlaySE(SE_SELECT);
            DrawAll();
            return;
        }
        else if (JOY_NEW(B_BUTTON | SELECT_BUTTON))
            sMenuOpen = FALSE;
        else
            return;
        PlaySE(SE_SELECT);
        DrawMenu();
        return;
    }

    if (sConfirm)
    {
        if (JOY_NEW(DPAD_LEFT | DPAD_RIGHT))
        {
            sConfirm = (sConfirm == 1) ? 2 : 1;
            PlaySE(SE_SELECT);
            DrawDesc();
        }
        else if (JOY_NEW(A_BUTTON))
        {
            if (sConfirm == 1 && sPendingAction == ACT_BEGIN)
            {
                PlaySE(SE_SELECT);
                BeginNormalPaletteFade(PALETTES_ALL, 0, 0, 16, RGB_BLACK);
                gTasks[taskId].func = Task_Begin;
                return;
            }
            if (sConfirm == 1)
            {
                sConfirm = 0;
                DoAction(sPendingAction, TRUE);
            }
            else
            {
                sConfirm = 0;
                DrawDesc();
            }
        }
        else if (JOY_NEW(B_BUTTON))
        {
            sConfirm = 0;
            DrawDesc();
        }
        return;
    }

    if (sMessage != NULL && JOY_NEW(A_BUTTON | B_BUTTON | DPAD_ANY))
    {
        sMessage = NULL;
        DrawDesc();
    }

    if (JOY_NEW(SELECT_BUTTON))
    {
        sMenuOpen = TRUE;
        sMenuCursor = sSection;
        PlaySE(SE_SELECT);
        DrawMenu();
    }
    else if (JOY_NEW(START_BUTTON))
    {
        GoToSection(SEC_BEGIN);
        sConfirm = 1;
        sPendingAction = ACT_BEGIN;
        PlaySE(SE_SELECT);
        DrawAll();
    }
    else if (JOY_NEW(L_BUTTON) || JOY_NEW(R_BUTTON))
    {
        GoToSection((sSection + (JOY_NEW(R_BUTTON) ? 1 : SEC_COUNT - 1)) % SEC_COUNT);
        PlaySE(SE_SELECT);
        DrawAll();
    }
    else if (JOY_REPEAT(DPAD_UP) || JOY_REPEAT(DPAD_DOWN))
    {
        s32 dir = JOY_REPEAT(DPAD_UP) ? -1 : 1;
        if (n == 0)
            return;
        sRow = (dir < 0) ? (sRow == 0 ? n - 1 : sRow - 1) : (sRow + 1) % n;
        if (dir > 0 && sRow == 0)
            sScroll = 0;
        FixCursor(dir);
        PlaySE(SE_SELECT);
        Redraw();
    }
    else if (r != NULL && JOY_REPEAT(DPAD_LEFT | DPAD_RIGHT))
    {
        s32 dir = JOY_REPEAT(DPAD_RIGHT) ? 1 : -1;
        switch (r->kind)
        {
        case RK_CHOICE:
        case RK_TOGGLE:
            ChangeChoice(r, dir);
            break;
        case RK_SLIDER:
            ChangeSlider(r, dir);
            break;
        default:
            return;
        }
        FixCursor(1);
        PlaySE(SE_SELECT);
        Redraw();
    }
    else if (r != NULL && JOY_NEW(A_BUTTON))
    {
        switch (r->kind)
        {
        case RK_CHOICE:   // A toggles On/Off and steps other choices forward
        case RK_TOGGLE:
            ChangeChoice(r, 1);
            FixCursor(1);
            PlaySE(SE_SELECT);
            Redraw();
            break;
        case RK_TEXT:
            PlaySE(SE_SELECT);
            StartTextEntry(taskId, r->fmt);
            break;
        case RK_ACTION:
            if (r->fmt == ACT_BEGIN)
            {
                sConfirm = 1;
                sPendingAction = ACT_BEGIN;
                PlaySE(SE_SELECT);
                DrawDesc();
            }
            else
            {
                DoAction(r->fmt, FALSE);
            }
            break;
        }
    }
}

static void Task_Begin(u8 taskId)
{
    if (!gPaletteFade.active)
    {
        DestroyTask(taskId);
        FreeAllWindowBuffers();
        sInitialized = FALSE;
        RH_ApplyPendingSettings();   // so the intro already uses them (instant text, player graphics)
        StartNewGameSceneFrlg();
    }
}

void CB2_InitRandomizerMenu(void)
{
    switch (gMain.state)
    {
    default:
    case 0:
        SetVBlankCallback(NULL);
        gMain.state++;
        break;
    case 1:
        DmaClearLarge16(3, (void *)(VRAM), VRAM_SIZE, 0x1000);
        DmaClear32(3, OAM, OAM_SIZE);
        DmaClear16(3, PLTT, PLTT_SIZE);
        SetGpuReg(REG_OFFSET_DISPCNT, 0);
        ResetBgsAndClearDma3BusyFlags(0);
        InitBgsFromTemplates(0, sBgTemplates, ARRAY_COUNT(sBgTemplates));
        ChangeBgX(0, 0, BG_COORD_SET);
        ChangeBgY(0, 0, BG_COORD_SET);
        ChangeBgX(1, 0, BG_COORD_SET);
        ChangeBgY(1, 0, BG_COORD_SET);
        InitWindows(sWinTemplates);
        DeactivateAllTextPrinters();
        SetGpuReg(REG_OFFSET_BLDCNT, 0);
        SetGpuReg(REG_OFFSET_DISPCNT, DISPCNT_OBJ_ON | DISPCNT_OBJ_1D_MAP);
        ShowBg(0);
        ShowBg(1);
        gMain.state++;
        break;
    case 2:
        ResetPaletteFade();
        ScanlineEffect_Stop();
        ResetTasks();
        ResetSpriteData();
        gMain.state++;
        break;
    case 3:
        LoadPalette(sBgColor, BG_PLTT_ID(0), sizeof(sBgColor));
        LoadPalette(sPal, BG_PLTT_ID(1), sizeof(sPal));
        gMain.state++;
        break;
    case 4:
        if (!sInitialized || gRhPendingSettings.version != RH_SETTINGS_VERSION)
        {
            RH_SetDefaultSettings(&gRhPendingSettings);
#if defined(RH_TEST_MENU_PRESET) && !defined(RELEASE)
            RH_TEST_MENU_PRESET(&gRhPendingSettings);     // test builds: start from a preset
#endif
            RandomSeedText();
            sSection = SEC_GENERAL;
            sRow = sScroll = 0;
            sMessage = NULL;
            sInitialized = TRUE;
        }
        sMenuOpen = FALSE;
        sConfirm = 0;
        sCodeMode = 0;
        RefreshPresetFlags();
        FixCursor(1);
        PutWindowTilemap(WIN_HEADER);
        PutWindowTilemap(WIN_LIST);
        PutWindowTilemap(WIN_DESC);
        DrawAll();
        CopyBgTilemapBufferToVram(0);
        gMain.state++;
        break;
    case 5:
        CreateTask(Task_FadeIn, 0);
        BeginNormalPaletteFade(PALETTES_ALL, 0, 16, 0, RGB_BLACK);
        SetVBlankCallback(VBlankCB);
        SetMainCallback2(MainCB2);
        return;
    }
}
