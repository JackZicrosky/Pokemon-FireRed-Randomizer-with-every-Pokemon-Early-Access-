// New Game randomizer settings screen, modelled on Universal Pokemon Randomizer FVX.
// Dark UI. SELECT opens the section menu (hamburger), L/R jump between sections, UP/DOWN pick an option,
// LEFT/RIGHT change it, A opens text fields / toggles, START jumps to "Begin Run".
#include "global.h"
#include "bg.h"
#include "gpu_regs.h"
#include "international_string_util.h"
#include "main.h"
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
    SEC_GRAPHICS, SEC_MISC, SEC_BEGIN, SEC_COUNT
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
LIST(sTypeChart, CS("Unchanged"), CS("Random"), CS("Random (balanced)"), CS("Keep Type Identities"), CS("Inverse"));
LIST(sStarters, CS("Unchanged"), CS("Custom"), CS("Random (completely)"), CS("Random (2 evolutions)"), CS("Random (incl. regional)"));
LIST(sStarterTypes, CS("None"), CS("Fire, Water, Grass"), CS("Any Type Triangle"), CS("Unique"), CS("Single Type"));
LIST(sStatics, CS("Unchanged"), CS("Swap Legends & Standards"), CS("Random (completely)"), CS("Random (similar str.)"));
LIST(sTrades, CS("Unchanged"), CS("Given Pokémon Only"), CS("Requested & Given"));
LIST(sMovesets, CS("Unchanged"), CS("Random (same type)"), CS("Random (completely)"), CS("Metronome Only"));
LIST(sTrainers, CS("Unchanged"), CS("Random"), CS("Random (even)"), CS("Type Themed"), CS("Type Themed (E4/Gyms)"),
     CS("Keep Themed Trainers"), CS("Keep Themes Or Primary"));
LIST(sBattleStyle, CS("Unchanged"), CS("Random"), CS("Single Style"));
LIST(sSinglesDoubles, CS("Single Battles"), CS("Double Battles"));
LIST(sWildZone, CS("1 In Whole Game"), CS("1 Per Named Location"), CS("1 Per Encounter Set"), CS("Maximum Possible"),
     CS("Completely Random Always"));
LIST(sWildType, CS("None"), CS("Randomize Zone Themes"), CS("Keep Primary Type"));
LIST(sWildEvo, CS("None"), CS("Only Basic Pokémon"), CS("Same Evolution Stage"));
LIST(sCompat, CS("Unchanged"), CS("Random (same type)"), CS("Random (completely)"), CS("Full Compatibility"));
LIST(sFieldItems, CS("Unchanged"), CS("Shuffle"), CS("Random"), CS("Random (even)"));
LIST(sShopItems, CS("Unchanged"), CS("Shuffle"), CS("Random"));

// Player graphics pack names (index 0 = default). Kept in sync with src/data/rh_player_graphics.h.
extern const u8 *const gRhPlayerGraphicsNames[];
extern const u8 gRhPlayerGraphicsCount;

// ---------------------------------------------------------------------------
// Visibility rules
// ---------------------------------------------------------------------------
#define VIS(name, expr) static bool32 name(const struct RhSettings *s) { return (expr); }
VIS(visStatsRandomized, s->baseStats != 0)
VIS(visExpCurve, s->expCurve != 0)
VIS(visTypesRandom, s->types != 0)
VIS(visAbilities, s->abilities != 0)
VIS(visEvos, s->evolutions != 0)
VIS(visInverse, s->typeChart == 4)
VIS(visCustomStarters, s->starters == 1)
VIS(visRandomStarters, s->starters >= 2)
VIS(visStarterItems, s->starterHeldItems)
VIS(visBstMin, s->starters >= 2 && s->starterBstMinOn)
VIS(visBstMax, s->starters >= 2 && s->starterBstMaxOn)
VIS(visSingleType, s->starters >= 2 && s->starterTypes == 4)
VIS(visStatics, s->statics != 0)
VIS(visStaticLevel, s->statics != 0 && s->staticLevelModOn)
VIS(visTrades, s->trades != 0)
VIS(visMovesets, s->movesets != 0 && s->movesets != 3)
VIS(visLevel1, s->movesets != 0 && s->movesets != 3 && s->guaranteedLevel1On)
VIS(visMovesetDamaging, s->movesets != 0 && s->movesets != 3 && s->movesetGoodDamagingOn)
VIS(visSingleStyle, s->battleStyle == 2)
VIS(visTrainerEvolve, s->trainersEvolveOn)
VIS(visTrainerLevel, s->trainerLevelModOn)
VIS(visWild, s->wild)
VIS(visWildSplit, s->wild && s->wildZone == 1)
VIS(visWildKeepThemes, s->wild && s->wildTypeRestriction == 0)
VIS(visWildKeepRelations, s->wild && s->wildEvoRestriction == 0)
VIS(visWildCatch, s->wild && s->wildCatchRateOn)
VIS(visWildHeldBan, s->wild && s->wildHeldItems)
VIS(visWildBalance, s->wild && s->wildSimilarStrength)
VIS(visWildLevel, s->wild && s->wildLevelModOn)
VIS(visTmMoves, s->tmMoves != 0)
VIS(visTmDamaging, s->tmMoves != 0 && s->tmGoodDamagingOn)
VIS(visTmCompat, s->tmCompat != 0)
VIS(visTutorMoves, s->tutorMoves != 0)
VIS(visTutorDamaging, s->tutorMoves != 0 && s->tutorGoodDamagingOn)
VIS(visFieldItems, s->fieldItems != 0)
VIS(visShopItems, s->shopItems != 0)
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
#define TEXT(sec, ind, tf, text, d, vis) { .kind = RK_TEXT, .section = sec, .indent = ind, .fmt = tf, .label = CS(text), .desc = CS(d), .visible = vis }

static const struct RhRow sRows[] =
{
    // ---------------- General ----------------
    CHOICE(SEC_GENERAL, 0, mechanicsGen, sMechGen, "Battle Mechanics", "Which generation's battle rules to use.\nPhys./Special split + Fairy always on.", NULL),
    TOGGLE(SEC_GENERAL, 0, enabled, "Randomizer", "Master switch. Off = the options below\nare ignored and the game is not randomized.", NULL),
    CHOICE(SEC_GENERAL, 0, speciesPool, sPool, "Pokémon Pool", "Which Pokémon the randomizer may use.\n+ Forms adds regional & alternate forms.", NULL),
    TEXT(SEC_GENERAL, 0, TF_SEED, "Seed", "Type any text. Same seed + same options\n= the exact same randomized game.", NULL),
    TOGGLE(SEC_GENERAL, 0, nuzlocke, "Nuzlocke Mode", "Adds two key items. One is an infinite\nRare Candy, one heals your whole party.", NULL),

    // ---------------- Pokemon Traits ----------------
    HDR(SEC_TRAITS, "Pokémon Base Statistics"),
    CHOICE(SEC_TRAITS, 1, baseStats, sUSR, "Base Stats", "Shuffle: stats swap around.\nRandom: the total is redistributed.", NULL),
    TOGGLE(SEC_TRAITS, 2, baseStatsFollowEvos, "Follow Evolutions", "Evolutions use the same shuffle / the\nsame stat proportions as their family.", visStatsRandomized),
    TOGGLE(SEC_TRAITS, 2, baseStatsRandomAdded, "Rand. Added Stats on Evo", "The stats a Pokémon gains when it\nevolves are distributed randomly.", visStatsRandomized),
    CHOICE(SEC_TRAITS, 1, updateBaseStatsGen, sMechGen, "Update Base Stats to Gen", "Use base stats from this generation.\nOff = same as Battle Mechanics.", NULL),
    CHOICE(SEC_TRAITS, 1, expCurve, sExpCurves, "Standardize EXP Curves", "Give every Pokémon the same EXP\ncurve (Off = unchanged).", NULL),
    CHOICE(SEC_TRAITS, 2, expCurveWho, sExpWho, "Applies To", "Legendaries: Slow keeps legendaries on\nthe Slow curve, everyone else standard.", visExpCurve),
    HDR(SEC_TRAITS, "Pokémon Types"),
    CHOICE(SEC_TRAITS, 1, types, sTypesC, "Types", "Follow evos keeps a family's types\nconsistent as it evolves.", NULL),
    TOGGLE(SEC_TRAITS, 2, forceDualTypes, "Force Dual Types", "Every Pokémon gets two types.", visTypesRandom),
    HDR(SEC_TRAITS, "Pokémon Abilities"),
    CHOICE(SEC_TRAITS, 1, abilities, sUR, "Abilities", "Give every Pokémon random abilities.", NULL),
    TOGGLE(SEC_TRAITS, 2, allowWonderGuard, "Allow Wonder Guard", "Wonder Guard can be handed out.", visAbilities),
    TOGGLE(SEC_TRAITS, 2, combineDuplicateAbilities, "Combine Duplicate Abil.", "Abilities with the same effect (Clear\nBody/White Smoke...) count as one.", visAbilities),
    TOGGLE(SEC_TRAITS, 2, ensureTwoAbilities, "Ensure Two Abilities", "Every Pokémon gets two abilities.", visAbilities),
    TOGGLE(SEC_TRAITS, 2, abilitiesFollowEvos, "Follow Evolutions", "Evolutions keep their family's\nrandom abilities.", visAbilities),
    TOGGLE(SEC_TRAITS, 2, banTrapAbilities, "Ban Trapping Abilities", "No Arena Trap, Magnet Pull or\nShadow Tag.", visAbilities),
    TOGGLE(SEC_TRAITS, 2, banNegativeAbilities, "Ban Negative Abilities", "No Defeatist, Slow Start, Truant,\nKlutz or Stall.", visAbilities),
    TOGGLE(SEC_TRAITS, 2, banBadAbilities, "Ban Bad Abilities", "No Minus, Plus, Anticipation, Forewarn,\nFrisk, Honey Gather, Aura Break...", visAbilities),
    HDR(SEC_TRAITS, "Pokémon Evolutions"),
    CHOICE(SEC_TRAITS, 1, evolutions, sEvosC, "Evolutions", "Every Level: every Pokémon evolves\ninto a random Pokémon each level up.", NULL),
    TOGGLE(SEC_TRAITS, 2, evoSimilarStrength, "Similar Strength", "New evolutions have a similar base\nstat total to the original.", visEvos),
    TOGGLE(SEC_TRAITS, 2, evoSameTyping, "Same Typing", "New evolutions share a type with\nthe Pokémon evolving.", visEvos),
    TOGGLE(SEC_TRAITS, 2, evoLimitThreeStages, "Limit to Three Stages", "No evolution chain longer than\nthree stages.", visEvos),
    TOGGLE(SEC_TRAITS, 2, evoNoConvergence, "No Convergence", "No two Pokémon evolve into the\nsame Pokémon.", visEvos),
    TOGGLE(SEC_TRAITS, 2, evoForceChange, "Force Change", "Every evolution is different from\nthe original.", visEvos),
    TOGGLE(SEC_TRAITS, 2, evoForceGrowth, "Force Growth", "Evolutions always have a higher base\nstat total than before.", visEvos),
    TOGGLE(SEC_TRAITS, 1, evoChangeImpossible, "Change Impossible Evos", "Trade and other hard evolutions\nbecome level-up or stone evolutions.", NULL),
    SLIDER(SEC_TRAITS, 1, evoMakeEasier, 0, 55, 5, SF_LEVEL, "Make Evolutions Easier", "Every Pokémon is fully evolved by this\nlevel (Off = unchanged).", NULL),
    TOGGLE(SEC_TRAITS, 1, evoEstimatedLevels, "Use Estimated Evo Levels", "Changed evolutions use levels estimated\nfrom the game instead of fixed ones.", NULL),
    TOGGLE(SEC_TRAITS, 1, evoRemoveTimeBased, "Remove Time-Based Evos", "Day/night evolutions work any time;\nsplit ones become stone evolutions.", NULL),

    // ---------------- Type effectiveness ----------------
    CHOICE(SEC_TYPES, 0, typeChart, sTypeChart, "Type Effectiveness", "Random keeps the same number of\nweaknesses, resistances and immunities.", NULL),
    TOGGLE(SEC_TYPES, 1, inverseRandomImmunities, "Add Random Immunities", "A few weaknesses become immunities\ninstead (as many as the base game).", visInverse),
    TOGGLE(SEC_TYPES, 0, updateTypeChart, "Update Type Effectiveness", "Use the Gen 9 type chart before any\nrandomization. Off = the older chart.", NULL),

    // ---------------- Starters, statics & trades ----------------
    HDR(SEC_STARTERS, "Starter Pokémon"),
    CHOICE(SEC_STARTERS, 1, starters, sStarters, "Starters", "Custom: type 3 names. Blank slots are\nBulbasaur, Charmander, Squirtle.", NULL),
    TEXT(SEC_STARTERS, 2, TF_STARTER1, "Starter 1", "Name of the first starter (all caps).", visCustomStarters),
    TEXT(SEC_STARTERS, 2, TF_STARTER2, "Starter 2", "Name of the second starter (all caps).", visCustomStarters),
    TEXT(SEC_STARTERS, 2, TF_STARTER3, "Starter 3", "Name of the third starter (all caps).", visCustomStarters),
    TOGGLE(SEC_STARTERS, 2, starterNoLegends, "Don't Use Legendaries", "Random starters are never legendary.", visRandomStarters),
    TOGGLE(SEC_STARTERS, 1, starterHeldItems, "Random Starter Held Items", "The starters come holding a random\nitem.", NULL),
    TOGGLE(SEC_STARTERS, 2, starterBanBadItems, "Ban Bad Items", "No berries, mail or other weak items.", visStarterItems),
    TOGGLE(SEC_STARTERS, 2, starterBstMinOn, "Limit BST: Minimum", "Random starters have at least this\nbase stat total.", visRandomStarters),
    TEXT(SEC_STARTERS, 3, TF_BST_MIN, "Minimum BST", "Type a number (e.g. 300).", visBstMin),
    TOGGLE(SEC_STARTERS, 2, starterBstMaxOn, "Limit BST: Maximum", "Random starters have at most this\nbase stat total.", visRandomStarters),
    TEXT(SEC_STARTERS, 3, TF_BST_MAX, "Maximum BST", "Type a number (e.g. 350).", visBstMax),
    CHOICE(SEC_STARTERS, 2, starterTypes, sStarterTypes, "Type Restrictions", "Triangle: the 3 starters beat each other\nin a circle. Unique: 3 different types.", visRandomStarters),
    TEXT(SEC_STARTERS, 3, TF_TYPE, "Single Type", "Type name in caps (e.g. DRAGON).\nBlank = a random type.", visSingleType),
    TOGGLE(SEC_STARTERS, 2, starterNoDualTypes, "No Dual Types", "Random starters have only one type.", visRandomStarters),
    HDR(SEC_STARTERS, "Static Pokémon"),
    CHOICE(SEC_STARTERS, 1, statics, sStatics, "Static Pokémon", "Gifts, legendaries, Snorlax, Game\nCorner prizes, fossils, the Togepi Egg...", NULL),
    TOGGLE(SEC_STARTERS, 2, staticRandomize600, "Randomize 600+ BST", "Pokémon with 600+ BST (Dragonite...)\nswap like legendaries do.", visStatics),
    TOGGLE(SEC_STARTERS, 2, staticLimitMainGameLegends, "Limit Main-Game Legends", "Legendaries in the main story are\nreplaced by different legendaries.", visStatics),
    TOGGLE(SEC_STARTERS, 2, staticFixMusic, "Fix Music", "Static battles play legendary music\nonly when the new Pokémon is legendary.", visStatics),
    TOGGLE(SEC_STARTERS, 2, staticLevelModOn, "Percentage Level Modifier", "Change the level of static Pokémon.", visStatics),
    SLIDER(SEC_STARTERS, 3, staticLevelMod, -100, 150, 5, SF_SIGNED_PERCENT, "Level Change", "Percentage added to static levels.", visStaticLevel),
    HDR(SEC_STARTERS, "In-Game Trades"),
    CHOICE(SEC_STARTERS, 1, trades, sTrades, "In-Game Trades", "Randomize the Pokémon NPCs trade.", NULL),
    TOGGLE(SEC_STARTERS, 2, tradeNicknames, "Randomize Nicknames", "Traded Pokémon get a random nickname.", visTrades),
    TOGGLE(SEC_STARTERS, 2, tradeOTs, "Randomize OTs", "Traded Pokémon get a random Original\nTrainer name.", visTrades),
    TOGGLE(SEC_STARTERS, 2, tradeIVs, "Randomize IVs", "Traded Pokémon get random IVs.", visTrades),
    TOGGLE(SEC_STARTERS, 2, tradeItems, "Randomize Items", "Traded Pokémon hold a random item.", visTrades),

    // ---------------- Moves ----------------
    HDR(SEC_MOVES, "Move Data"),
    TOGGLE(SEC_MOVES, 1, movePower, "Randomize Move Power", "Attacks get random power (20-150).", NULL),
    TOGGLE(SEC_MOVES, 1, moveAccuracy, "Randomize Move Accuracy", "Moves get random accuracy (60-100%).", NULL),
    TOGGLE(SEC_MOVES, 1, movePP, "Randomize Move PP", "Moves get random PP (5-40).", NULL),
    TOGGLE(SEC_MOVES, 1, moveType, "Randomize Move Types", "Every move gets a random type.", NULL),
    TOGGLE(SEC_MOVES, 1, moveNames, "Randomize Move Names", "Moves get silly random names.", NULL),
    CHOICE(SEC_MOVES, 1, updateMovesGen, sMechGen, "Update Moves to Gen", "Move power/accuracy/PP/type from this\ngeneration. Off = same as Battle Mechanics.", NULL),
    HDR(SEC_MOVES, "Pokémon Movesets"),
    CHOICE(SEC_MOVES, 1, movesets, sMovesets, "Movesets", "Every Pokémon learns random moves.\nEach gets a decent attack early on.", NULL),
    TOGGLE(SEC_MOVES, 2, guaranteedLevel1On, "Guaranteed Level 1 Moves", "Every Pokémon knows a set number of\nmoves at level 1.", visMovesets),
    SLIDER(SEC_MOVES, 3, guaranteedLevel1Moves, 2, 4, 1, SF_PLAIN, "Moves at Level 1", "How many moves at level 1 (2-4).", visLevel1),
    TOGGLE(SEC_MOVES, 2, reorderDamagingMoves, "Reorder Damaging Moves", "Stronger attacks are learned later.", visMovesets),
    TOGGLE(SEC_MOVES, 2, movesetNoGameBreaking, "No Game-Breaking Moves", "No OHKO, fixed-damage or Sketch-type\nmoves in random movesets.", visMovesets),
    TOGGLE(SEC_MOVES, 2, movesetGoodDamagingOn, "Force % Good Damaging", "Guarantee a share of strong attacks.", visMovesets),
    SLIDER(SEC_MOVES, 3, movesetGoodDamaging, 0, 100, 5, SF_PERCENT, "Good Damaging Moves", "Share of learned moves that are good\nattacks.", visMovesetDamaging),

    // ---------------- Foes ----------------
    CHOICE(SEC_FOES, 0, trainers, sTrainers, "Trainer Pokémon", "Themed: each trainer (or gym) gets a\nrandom type. Keep: keep existing themes.", NULL),
    HDR(SEC_FOES, "Better Movesets for..."),
    TOGGLE(SEC_FOES, 1, betterMovesets[0], "Boss Trainers", "Gym Leaders, Elite Four, Champion:\nTM/egg moves that fit the Pokémon.", NULL),
    TOGGLE(SEC_FOES, 1, betterMovesets[1], "Important Trainers", "Rival, Giovanni, Team Rocket bosses.", NULL),
    TOGGLE(SEC_FOES, 1, betterMovesets[2], "Regular Trainers", "Every other trainer.", NULL),
    HDR(SEC_FOES, "Additional Pokémon for..."),
    SLIDER(SEC_FOES, 1, additionalMons[0], 0, 5, 1, SF_PLAIN, "Boss Trainers", "Extra Pokémon (up to 6 total).\n0 = off.", NULL),
    SLIDER(SEC_FOES, 1, additionalMons[1], 0, 5, 1, SF_PLAIN, "Important Trainers", "Extra Pokémon (up to 6 total).\n0 = off.", NULL),
    SLIDER(SEC_FOES, 1, additionalMons[2], 0, 5, 1, SF_PLAIN, "Regular Trainers", "Extra Pokémon (up to 6 total).\n0 = off.", NULL),
    HDR(SEC_FOES, "Add Held Items to..."),
    TOGGLE(SEC_FOES, 1, heldItemsFor[0], "Boss Trainers", "Give boss trainers' Pokémon items.", NULL),
    TOGGLE(SEC_FOES, 1, heldItemsFor[1], "Important Trainers", "Give important trainers' Pokémon items.", NULL),
    TOGGLE(SEC_FOES, 1, heldItemsFor[2], "Regular Trainers", "Give regular trainers' Pokémon items.", NULL),
    TOGGLE(SEC_FOES, 1, heldConsumableOnly, "Consumable Only", "Only items that get used up (berries,\nFocus Sash, gems...).", NULL),
    TOGGLE(SEC_FOES, 1, heldSensible, "Sensible Items", "Items that suit the Pokémon (Choice\nSpecs on special attackers, etc.).", NULL),
    TOGGLE(SEC_FOES, 1, heldHighestOnly, "Highest Level Only", "Only the trainer's highest-level\nPokémon gets an item.", NULL),
    HDR(SEC_FOES, "Force Diverse Types for..."),
    TOGGLE(SEC_FOES, 1, diverseTypes[0], "Boss Trainers", "No two Pokémon on the team share\na primary type.", NULL),
    TOGGLE(SEC_FOES, 1, diverseTypes[1], "Important Trainers", "No two Pokémon on the team share\na primary type.", NULL),
    TOGGLE(SEC_FOES, 1, diverseTypes[2], "Regular Trainers", "No two Pokémon on the team share\na primary type.", NULL),
    HDR(SEC_FOES, "Battle Style"),
    CHOICE(SEC_FOES, 1, battleStyle, sBattleStyle, "Battle Style", "Random: each trainer randomly single or\ndouble. Trainers need 2+ Pokémon.", NULL),
    CHOICE(SEC_FOES, 2, battleStyleDoubles, sSinglesDoubles, "Style", "Every trainer battle uses this style.", visSingleStyle),
    HDR(SEC_FOES, "Other"),
    TOGGLE(SEC_FOES, 1, rivalCarriesTeam, "Rival Carries Team", "Your rival keeps his starter and team\nthroughout the game, evolving them.", NULL),
    TOGGLE(SEC_FOES, 1, trainerSimilarStrength, "Similar Strength", "Replacements have a similar base\nstat total.", NULL),
    TOGGLE(SEC_FOES, 1, trainerAvoidDuplicates, "Try to Avoid Duplicates", "A trainer's Pokémon are all different.", NULL),
    TOGGLE(SEC_FOES, 1, trainerWeightTypes, "Weight Types by Count", "Themed trainers pick types with many\nPokémon more often.", NULL),
    TOGGLE(SEC_FOES, 1, trainerLocalPokemon, "Use Local Pokémon", "Trainers use Pokémon that can be\ncaught in the wild nearby.", NULL),
    TOGGLE(SEC_FOES, 1, trainerNoLegends, "Don't Use Legendaries", "Trainers never use legendaries.", NULL),
    TOGGLE(SEC_FOES, 1, noEarlyWonderGuard, "No Early Wonder Guard", "No Wonder Guard on trainer Pokémon\nbefore level 20.", NULL),
    SLIDER(SEC_FOES, 1, leagueUnique, 0, 2, 1, SF_PLAIN, "League Unique Pokémon", "Elite Four/Champion get this many\nPokémon nobody else uses. 0 = off.", NULL),
    TOGGLE(SEC_FOES, 1, randomTrainerNames, "Randomize Trainer Names", "Trainers get random names.", NULL),
    TOGGLE(SEC_FOES, 1, randomTrainerClassNames, "Random Trainer Classes", "Trainer classes get random names.", NULL),
    TOGGLE(SEC_FOES, 1, trainersEvolveOn, "Trainers Evolve Pokémon", "Trainer Pokémon evolve as their level\nallows; fully evolved by the level below.", NULL),
    SLIDER(SEC_FOES, 2, trainersEvolveLevel, 30, 65, 1, SF_LEVEL, "Fully Evolved By", "Latest level for fully evolved\ntrainer Pokémon.", visTrainerEvolve),
    TOGGLE(SEC_FOES, 1, trainerLevelModOn, "Percentage Level Modifier", "Change every trainer Pokémon's level.", NULL),
    SLIDER(SEC_FOES, 2, trainerLevelMod, -100, 150, 5, SF_SIGNED_PERCENT, "Level Change", "Percentage added to trainer levels.", visTrainerLevel),

    // ---------------- Wild ----------------
    TOGGLE(SEC_WILD, 0, wild, "Randomize Wild Pokémon", "Turn on to randomize wild encounters.", NULL),
    HDR(SEC_WILD, "Replacements Per Species"),
    CHOICE(SEC_WILD, 1, wildZone, sWildZone, "Replacements", "How often a species gets a new\nreplacement. Always = no fixed pools.", visWild),
    TOGGLE(SEC_WILD, 2, wildSplitEncounterTypes, "Split by Encounter Types", "Grass, surfing, fishing and Rock Smash\nget separate replacements.", visWildSplit),
    HDR(SEC_WILD, "Type Restrictions"),
    CHOICE(SEC_WILD, 1, wildTypeRestriction, sWildType, "Types", "Zone Themes: every area gets a random\ntype. Primary: keep the first type.", visWild),
    TOGGLE(SEC_WILD, 2, wildKeepThemes, "Keep Set/Zone Themes", "Areas full of one type (e.g. caves)\nkeep that type.", visWildKeepThemes),
    HDR(SEC_WILD, "Evolution Restrictions"),
    CHOICE(SEC_WILD, 1, wildEvoRestriction, sWildEvo, "Evolutions", "Only Basic: only unevolved Pokémon.\nSame Stage: keep the evolution stage.", visWild),
    TOGGLE(SEC_WILD, 2, wildKeepRelations, "Keep Relations", "Related Pokémon in an area are\nreplaced by related Pokémon.", visWildKeepRelations),
    HDR(SEC_WILD, "Other"),
    TOGGLE(SEC_WILD, 1, wildNoLegends, "Don't Use Legendaries", "Legendaries never appear in the wild.", visWild),
    TOGGLE(SEC_WILD, 1, wildCatchRateOn, "Set Minimum Catch Rate", "Make hard-to-catch Pokémon easier.", NULL),
    SLIDER(SEC_WILD, 2, wildCatchRate, 1, 5, 1, SF_PLAIN, "Catch Rate Level", "1 = normal ... 4 = equal for all,\n5 = always caught.", visWildCatch),
    TOGGLE(SEC_WILD, 1, wildHeldItems, "Randomize Held Items", "Wild Pokémon hold random items.", NULL),
    TOGGLE(SEC_WILD, 2, wildBanBadItems, "Ban Bad Items", "No berries, mail or other weak items.", visWildHeldBan),
    TOGGLE(SEC_WILD, 1, wildCatchEmAll, "Catch Em' All Mode", "Every Pokémon appears somewhere before\nany repeats.", visWild),
    TOGGLE(SEC_WILD, 1, wildSimilarStrength, "Similar Strength", "Replacements have a similar base\nstat total.", visWild),
    TOGGLE(SEC_WILD, 2, wildBalanceLowLevel, "Balance Low Level", "No abnormally strong Pokémon at\nlow levels.", visWildBalance),
    TOGGLE(SEC_WILD, 1, wildMegas, "Permanent Mega Pokémon", "Mega Evolved Pokémon can appear and\nstay Mega after you catch them.", visWild),
    TOGGLE(SEC_WILD, 1, wildLevelModOn, "Percentage Level Modifier", "Change every wild Pokémon's level.", NULL),
    SLIDER(SEC_WILD, 2, wildLevelMod, -100, 150, 5, SF_SIGNED_PERCENT, "Level Change", "Percentage added to wild levels.", visWildLevel),

    // ---------------- TMs & tutors ----------------
    HDR(SEC_TMS, "TMs & HMs"),
    CHOICE(SEC_TMS, 1, tmMoves, sUR, "TM Moves", "TMs teach random moves.\nHMs never change.", NULL),
    TOGGLE(SEC_TMS, 2, tmNoGameBreaking, "No Game-Breaking Moves", "No OHKO or fixed-damage TMs.", visTmMoves),
    TOGGLE(SEC_TMS, 2, tmKeepFieldMoves, "Keep Field Move TMs", "TMs with field uses (Dig, Flash...)\nkeep their move.", visTmMoves),
    TOGGLE(SEC_TMS, 2, tmGoodDamagingOn, "Force % Good Damaging", "Guarantee a share of strong attacks.", visTmMoves),
    SLIDER(SEC_TMS, 3, tmGoodDamaging, 0, 100, 5, SF_PERCENT, "Good Damaging Moves", "Share of TMs that are good attacks.", visTmDamaging),
    CHOICE(SEC_TMS, 1, tmCompat, sCompat, "TM/HM Compatibility", "Which Pokémon can learn each TM.\nSame type: 90% same, 50% Normal, 25%.", NULL),
    TOGGLE(SEC_TMS, 2, tmLevelupSanity, "TM/Levelup Move Sanity", "A Pokémon can always learn TMs of\nmoves it learns by level up.", visTmCompat),
    TOGGLE(SEC_TMS, 2, tmCompatFollowEvos, "Follow Evolutions", "Evolutions keep their family's\ncompatibility.", visTmCompat),
    TOGGLE(SEC_TMS, 2, fullHMCompat, "Full HM Compatibility", "Every Pokémon can learn every HM.", visTmCompat),
    HDR(SEC_TMS, "Move Tutors"),
    CHOICE(SEC_TMS, 1, tutorMoves, sUR, "Move Tutor Moves", "Move tutors teach random moves.", NULL),
    TOGGLE(SEC_TMS, 2, tutorNoGameBreaking, "No Game-Breaking Moves", "No OHKO or fixed-damage tutor moves.", visTutorMoves),
    TOGGLE(SEC_TMS, 2, tutorKeepFieldMoves, "Keep Field Move Tutors", "Tutors of field moves keep them.", visTutorMoves),
    TOGGLE(SEC_TMS, 2, tutorGoodDamagingOn, "Force % Good Damaging", "Guarantee a share of strong attacks.", visTutorMoves),
    SLIDER(SEC_TMS, 3, tutorGoodDamaging, 0, 100, 5, SF_PERCENT, "Good Damaging Moves", "Share of tutor moves that are good\nattacks.", visTutorDamaging),
    CHOICE(SEC_TMS, 1, tutorCompat, sCompat, "Tutor Compatibility", "Which Pokémon can learn tutor moves.", NULL),

    // ---------------- Items ----------------
    CHOICE(SEC_ITEMS, 0, fieldItems, sFieldItems, "Field Items", "Items in Poké Balls and hidden items.\nEven: every item about equally often.", NULL),
    TOGGLE(SEC_ITEMS, 1, fieldBanBad, "Ban Bad Items", "No berries, mail or other weak items.", visFieldItems),
    CHOICE(SEC_ITEMS, 0, shopItems, sShopItems, "Shop Items", "Regular clerks keep balls & medicine.\nSpecial shops are fully randomized.", NULL),
    TOGGLE(SEC_ITEMS, 1, shopBanBad, "Ban Bad Items", "No berries, mail or other weak items.", visShopItems),
    TOGGLE(SEC_ITEMS, 1, shopBanRegular, "Ban Regular Shop Items", "Random stock never includes normal\nmart items (Potions, Repels...).", visShopItems),
    TOGGLE(SEC_ITEMS, 1, shopBanOverpowered, "Ban Overpowered Items", "No Lucky Egg, Rare Candy, Master Ball\nor items that sell for a fortune.", visShopItems),
    TOGGLE(SEC_ITEMS, 1, shopGuaranteeEvo, "Guarantee Evolution Items", "Every evolution item is sold somewhere.", visShopItems),
    TOGGLE(SEC_ITEMS, 1, shopGuaranteeX, "Guarantee X Items", "Every X item, Guard Spec. and Dire Hit\nis sold somewhere.", visShopItems),
    TOGGLE(SEC_ITEMS, 1, shopBalancePrices, "Balance Shop Prices", "Rebalance prices of items that are far\ntoo cheap or expensive.", visShopItems),
    TOGGLE(SEC_ITEMS, 0, shopSpecial, "Randomize Special Shops", "Evolution, herb and Celadon counters\nsell random items, no duplicates.", NULL),
    CHOICE(SEC_ITEMS, 0, pickupItems, sUR, "Pickup Items", "Items found by the Pickup ability.", NULL),
    TOGGLE(SEC_ITEMS, 1, pickupBanBad, "Ban Bad Items", "No berries, mail or other weak items.", visPickup),

    // ---------------- Graphics ----------------
    { .kind = RK_CHOICE, .section = SEC_GRAPHICS, .offset = OFS(playerGraphics), .size = 1, .label = CS("Player Character"),
      .desc = CS("Replace your character's sprites.\nFrom UPR FVX's graphics packs."), .choices = NULL },

    // ---------------- Misc ----------------
    TOGGLE(SEC_MISC, 0, instantText, "Instantaneous Text", "All text appears instantly, whatever\nthe text speed option says.", NULL),
    TOGGLE(SEC_MISC, 0, runIndoors, "Running Shoes Indoors", "Run inside buildings and caves too.", NULL),
    TOGGLE(SEC_MISC, 0, randomPcPotion, "Randomize PC Potion", "The item in your bedroom PC is random.", NULL),
    TOGGLE(SEC_MISC, 0, nationalDexAtStart, "National Dex at Start", "The Pokédex starts in National mode.", NULL),
    TOGGLE(SEC_MISC, 0, fastEggs, "Fast Egg Hatching", "Eggs hatch in very few steps.", NULL),
    TOGGLE(SEC_MISC, 0, lowerCaseNames, "Lower Case Pokémon Names", "“Pikachu” instead of “PIKACHU”.", NULL),
    TOGGLE(SEC_MISC, 0, randomCatchTutorial, "Random Catching Tutorial", "The old man in Viridian catches a\nrandom Pokémon.", NULL),
    TOGGLE(SEC_MISC, 0, banLuckyEgg, "Ban Lucky Egg", "The Lucky Egg never appears anywhere.", NULL),
    TOGGLE(SEC_MISC, 0, balanceStaticLevels, "Balance Static Levels", "Revived fossils come back at level 30\ninstead of 5 (as in UPR FVX).", NULL),
    TOGGLE(SEC_MISC, 0, runWithoutShoes, "Run Without Running Shoes", "Hold B to run from the very start.", NULL),
    TOGGLE(SEC_MISC, 0, reusableTMs, "Infinitely Reusable TMs", "TMs are never used up.", NULL),
    TOGGLE(SEC_MISC, 0, forgettableTMs, "Forgettable HMs", "HM moves can be forgotten like\nany other move.", NULL),
    TOGGLE(SEC_MISC, 0, noEVs, "No EVs From Pokémon", "Defeating Pokémon gives no EVs.", NULL),

    // ---------------- Begin ----------------
    { .kind = RK_ACTION, .section = SEC_BEGIN, .label = CS("Begin Run"), .desc = CS("Start the adventure with these\nsettings. Press A.") },
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

#define S (&gRhPendingSettings)

static bool32 RowVisible(const struct RhRow *r)
{
    return r->visible == NULL || r->visible(S);
}

static u32 VisibleRows(u32 section, const struct RhRow **out)
{
    u32 i, n = 0;
    for (i = 0; i < ARRAY_COUNT(sRows); i++)
        if (sRows[i].section == section && RowVisible(&sRows[i]))
            out[n++] = &sRows[i];
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
    }
    SetValue(r, v);
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
            static const u16 sDefaults[] = { SPECIES_BULBASAUR, SPECIES_CHARMANDER, SPECIES_SQUIRTLE };
            u16 sp = S->customStarters[r->fmt - TF_STARTER1];
            StringCopy(dst, GetSpeciesName(sp ? sp : sDefaults[r->fmt - TF_STARTER1]));
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

static void DrawHeader(void)
{
    u8 buf[48];
    FillWindowPixelBuffer(WIN_HEADER, PIXEL_FILL(C_TRANSPARENT));
    FillWindowPixelRect(WIN_HEADER, PIXEL_FILL(C_HEADBAR), 0, 0, 240, 15);
    AddTextPrinterParameterized3(WIN_HEADER, FONT_NORMAL, 4, 1, sColAccent, TEXT_SKIP_DRAW, CS("="));
    AddTextPrinterParameterized3(WIN_HEADER, FONT_NORMAL, 14, 1, sColText, TEXT_SKIP_DRAW, sSectionNames[sSection]);
    StringCopy(buf, CS("SELECT Menu  START Begin"));
    AddTextPrinterParameterized3(WIN_HEADER, FONT_SMALL, GetStringRightAlignXOffset(FONT_SMALL, buf, 236), 3, sColDim, TEXT_SKIP_DRAW, buf);
    CopyWindowToVram(WIN_HEADER, COPYWIN_FULL);
}

static void DrawList(void)
{
    const struct RhRow *rows[ARRAY_COUNT(sRows)];
    u32 n = VisibleRows(sSection, rows), i;
    bool32 dimAll = !S->enabled && sSection != SEC_GENERAL && sSection != SEC_MISC && sSection != SEC_GRAPHICS && sSection != SEC_BEGIN;

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
            AddTextPrinterParameterized3(WIN_LIST, FONT_NORMAL, 88, y + 1, sColOk, TEXT_SKIP_DRAW, CS("▶ BEGIN RUN"));
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
        AddTextPrinterParameterized3(WIN_LIST, FONT_NORMAL, GetStringRightAlignXOffset(FONT_NORMAL, buf, 228), y + 1,
                                     dimAll ? sColDim : (r->kind == RK_TEXT ? sColAccent : sColValue), TEXT_SKIP_DRAW, buf);
    }
    if (sScroll > 0)
        AddTextPrinterParameterized3(WIN_LIST, FONT_SMALL, 233, 0, sColDim, TEXT_SKIP_DRAW, CS("{UP_ARROW}"));
    if (sScroll + LIST_ROWS < n)
        AddTextPrinterParameterized3(WIN_LIST, FONT_SMALL, 233, LIST_ROWS * ROW_H - 10, sColDim, TEXT_SKIP_DRAW, CS("{DOWN_ARROW}"));
    CopyWindowToVram(WIN_LIST, COPYWIN_GFX);
}

static void DrawDesc(void)
{
    const struct RhRow *rows[ARRAY_COUNT(sRows)];
    u32 n = VisibleRows(sSection, rows);
    FillWindowPixelBuffer(WIN_DESC, PIXEL_FILL(C_HEADBAR));
    if (sConfirm)
    {
        AddTextPrinterParameterized3(WIN_DESC, FONT_NORMAL, 8, 1, sColText, TEXT_SKIP_DRAW,
            S->enabled ? CS("Begin a RANDOMIZED run?") : CS("Begin a normal (not randomized) run?"));
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

static void Task_Input(u8 taskId)
{
    const struct RhRow *rows[ARRAY_COUNT(sRows)];
    u32 n = VisibleRows(sSection, rows);
    const struct RhRow *r = (sRow < n) ? rows[sRow] : NULL;

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
            if (sConfirm == 1)
            {
                PlaySE(SE_SELECT);
                BeginNormalPaletteFade(PALETTES_ALL, 0, 0, 16, RGB_BLACK);
                gTasks[taskId].func = Task_Begin;
                return;
            }
            sConfirm = 0;
            DrawDesc();
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
            sConfirm = 1;
            PlaySE(SE_SELECT);
            DrawDesc();
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
            RandomSeedText();
            sSection = SEC_GENERAL;
            sRow = sScroll = 0;
            sMessage = NULL;
            sInitialized = TRUE;
        }
        sMenuOpen = FALSE;
        sConfirm = 0;
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
