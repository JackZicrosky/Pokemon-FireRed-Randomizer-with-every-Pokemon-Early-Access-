# Shop / item distribution design for the FireRed build.
# Everything here is consumed by gen_shops.py, which writes the scripts, C tables and map objects.

# ---------------------------------------------------------------------------
# 1) Progressive stock added to EVERY regular Poke Mart clerk (and Indigo / Trainer Tower),
#    unlocked by number of badges, like Great/Ultra Balls in the base game.
#    (minBadges, item)
# ---------------------------------------------------------------------------
PROGRESSIVE_ALL_MARTS = [
    (0, "HEAL_BALL"), (0, "EXP_CANDY_XS"), (0, "LURE"),
    (1, "NEST_BALL"), (1, "NET_BALL"), (1, "EXP_CANDY_S"),
    (1, "HEALTH_FEATHER"), (1, "MUSCLE_FEATHER"), (1, "RESIST_FEATHER"),
    (1, "GENIUS_FEATHER"), (1, "CLEVER_FEATHER"), (1, "SWIFT_FEATHER"),
    (2, "REPEAT_BALL"), (2, "TIMER_BALL"), (2, "DIVE_BALL"), (2, "SUPER_LURE"),
    (2, "HEALTH_MOCHI"), (2, "MUSCLE_MOCHI"), (2, "RESIST_MOCHI"),
    (2, "GENIUS_MOCHI"), (2, "CLEVER_MOCHI"), (2, "SWIFT_MOCHI"), (2, "FRESH_START_MOCHI"),
    (3, "QUICK_BALL"), (3, "DUSK_BALL"), (3, "LUXURY_BALL"), (3, "EXP_CANDY_M"),
    (4, "LEVEL_BALL"), (4, "LURE_BALL"), (4, "MOON_BALL"), (4, "FRIEND_BALL"),
    (4, "LOVE_BALL"), (4, "FAST_BALL"), (4, "HEAVY_BALL"), (4, "MAX_LURE"),
    (5, "EXP_CANDY_L"), (5, "MAX_ETHER"), (5, "ELIXIR"),
    (8, "EXP_CANDY_XL"), (8, "MAX_ELIXIR"),
]

# ---------------------------------------------------------------------------
# 2) Town-themed held items, "peppered" across Kanto/Sevii marts.
#    Each town's mart clerk gets its themed list (after its minimum badge count).
#    Themes follow the local Gym type / town flavour.
# ---------------------------------------------------------------------------
TOWN_THEMED = {
    "VIRIDIAN":  [(0, "EVERSTONE"), (7, "SOFT_SAND"), (7, "GROUND_GEM"), (7, "IRON_BALL"),
                  (7, "TERRAIN_EXTENDER"), (7, "BINDING_BAND"), (7, "GRIP_CLAW")],
    "PEWTER":    [(0, "HARD_STONE"), (0, "ROCK_GEM"), (0, "ROCK_INCENSE"), (0, "SMOOTH_ROCK"),
                  (0, "PROTECTIVE_PADS"), (0, "FLOAT_STONE"), (0, "SILK_SCARF"), (0, "NORMAL_GEM")],
    "CERULEAN":  [(0, "MYSTIC_WATER"), (0, "WATER_GEM"), (0, "SEA_INCENSE"), (0, "WAVE_INCENSE"),
                  (0, "DAMP_ROCK"), (0, "ABSORB_BULB"), (0, "SHELL_BELL"), (0, "BIG_ROOT")],
    "VERMILION": [(0, "MAGNET"), (0, "ELECTRIC_GEM"), (0, "CELL_BATTERY"), (0, "ELECTRIC_SEED"),
                  (0, "AIR_BALLOON"), (0, "SHARP_BEAK"), (0, "FLYING_GEM"), (0, "METRONOME"), (0, "ZOOM_LENS")],
    "LAVENDER":  [(0, "SPELL_TAG"), (0, "GHOST_GEM"), (0, "CLEANSE_TAG"), (0, "SMOKE_BALL"),
                  (0, "BLACK_GLASSES"), (0, "DARK_GEM"), (0, "SOOTHE_BELL")],
    "CELADON":   [(0, "MIRACLE_SEED"), (0, "GRASS_GEM"), (0, "ROSE_INCENSE"), (0, "GRASSY_SEED"),
                  (0, "LUMINOUS_MOSS"), (0, "MUSCLE_BAND"), (0, "WISE_GLASSES")],
    "FUCHSIA":   [(0, "POISON_BARB"), (0, "POISON_GEM"), (0, "BLACK_SLUDGE"), (0, "TOXIC_ORB"),
                  (0, "SILVER_POWDER"), (0, "BUG_GEM"), (0, "QUICK_CLAW"), (0, "WIDE_LENS"), (0, "STICKY_BARB")],
    "SAFFRON":   [(0, "TWISTED_SPOON"), (0, "PSYCHIC_GEM"), (0, "ODD_INCENSE"), (0, "PSYCHIC_SEED"),
                  (0, "LIGHT_CLAY"), (0, "SCOPE_LENS"), (0, "RED_CARD"), (0, "EJECT_BUTTON"),
                  (0, "ROOM_SERVICE"), (0, "BLACK_BELT"), (0, "FIGHTING_GEM"), (0, "PUNCHING_GLOVE")],
    "CINNABAR":  [(0, "CHARCOAL"), (0, "FIRE_GEM"), (0, "HEAT_ROCK"), (0, "FLAME_ORB"),
                  (0, "UTILITY_UMBRELLA"), (0, "HEAVY_DUTY_BOOTS"), (0, "SAFETY_GOGGLES"), (0, "THROAT_SPRAY")],
    "INDIGO":    [(0, "DRAGON_FANG"), (0, "DRAGON_GEM"), (0, "NEVER_MELT_ICE"), (0, "ICE_GEM"),
                  (0, "BLUNDER_POLICY"), (0, "ADRENALINE_ORB"), (0, "EJECT_PACK"), (0, "ABILITY_SHIELD")],
    "THREE_ISLAND": [(0, "METAL_COAT"), (0, "STEEL_GEM"), (0, "SHED_SHELL"), (0, "RING_TARGET"),
                     (0, "LAGGING_TAIL"), (0, "BRIGHT_POWDER"), (0, "FOCUS_BAND")],
    "FOUR_ISLAND":  [(0, "ICY_ROCK"), (0, "SNOWBALL"), (0, "FULL_INCENSE"), (0, "LAX_INCENSE"),
                     (0, "DESTINY_KNOT")],
    "SIX_ISLAND":   [(0, "FAIRY_GEM"), (0, "MISTY_SEED"), (0, "FAIRY_FEATHER"), (0, "PURE_INCENSE"),
                     (0, "LUCK_INCENSE"), (0, "KINGS_ROCK")],
    "SEVEN_ISLAND": [(0, "MACHO_BRACE"), (0, "POWER_WEIGHT"), (0, "POWER_BRACER"), (0, "POWER_BELT"),
                     (0, "POWER_LENS"), (0, "POWER_BAND"), (0, "POWER_ANKLET")],
    "TRAINER_TOWER": [],
}

# Regular clerk script/list per mart id: (mart id, map, clerk script label, vanilla list label)
MARTS = [
    ("VIRIDIAN", "ViridianCity_Mart_Frlg", "ViridianCity_Mart_Items"),
    ("PEWTER", "PewterCity_Mart_Frlg", "PewterCity_Mart_Items"),
    ("CERULEAN", "CeruleanCity_Mart_Frlg", "CeruleanCity_Mart_Items"),
    ("VERMILION", "VermilionCity_Mart_Frlg", "VermilionCity_Mart_Items"),
    ("LAVENDER", "LavenderTown_Mart_Frlg", "LavenderTown_Mart_Items"),
    ("CELADON", "CeladonCity_DepartmentStore_2F_Frlg", "CeladonCity_DepartmentStore_2F_Items"),
    ("FUCHSIA", "FuchsiaCity_Mart_Frlg", "FuchsiaCity_Mart_Items"),
    ("SAFFRON", "SaffronCity_Mart_Frlg", "SaffronCity_Mart_Items"),
    ("CINNABAR", "CinnabarIsland_Mart_Frlg", "CinnabarIsland_Mart_Items"),
    ("INDIGO", "IndigoPlateau_PokemonCenter_1F_Frlg", "IndigoPlateau_PokemonCenter_1F_Items"),
    ("THREE_ISLAND", "ThreeIsland_Mart_Frlg", "ThreeIsland_Mart_Items"),
    ("FOUR_ISLAND", "FourIsland_Mart_Frlg", "FourIsland_Mart_Items"),
    ("SIX_ISLAND", "SixIsland_Mart_Frlg", "SixIsland_Mart_Items"),
    ("SEVEN_ISLAND", "SevenIsland_Mart_Frlg", "SevenIsland_Mart_Items"),
    ("TRAINER_TOWER", "TrainerTower_Lobby_Frlg", "TrainerTower_Lobby_Mart_Items"),
]

# ---------------------------------------------------------------------------
# 3) Evolution-item specialist: stands in EVERY mart, sells everything that
#    triggers an evolution, from the start of the game.
# ---------------------------------------------------------------------------
EVO_ITEMS = """FIRE_STONE WATER_STONE THUNDER_STONE LEAF_STONE MOON_STONE SUN_STONE SHINY_STONE DUSK_STONE
DAWN_STONE ICE_STONE OVAL_STONE EVERSTONE LINKING_CORD KINGS_ROCK METAL_COAT DRAGON_SCALE UPGRADE
DUBIOUS_DISC PROTECTOR ELECTIRIZER MAGMARIZER REAPER_CLOTH RAZOR_CLAW RAZOR_FANG DEEP_SEA_TOOTH
DEEP_SEA_SCALE PRISM_SCALE WHIPPED_DREAM SACHET STRAWBERRY_SWEET LOVE_SWEET BERRY_SWEET CLOVER_SWEET
FLOWER_SWEET STAR_SWEET RIBBON_SWEET SWEET_APPLE TART_APPLE SYRUPY_APPLE CRACKED_POT CHIPPED_POT
UNREMARKABLE_TEACUP MASTERPIECE_TEACUP GALARICA_CUFF GALARICA_WREATH BLACK_AUGURITE PEAT_BLOCK
AUSPICIOUS_ARMOR MALICIOUS_ARMOR METAL_ALLOY LEADERS_CREST GIMMIGHOUL_COIN SCROLL_OF_DARKNESS
SCROLL_OF_WATERS""".split()

# (mapname, x, y, facing movement) for the evo specialist in each shop.
EVO_NPC_POS = {
    "ViridianCity_Mart_Frlg": (2, 2), "PewterCity_Mart_Frlg": (2, 2), "CeruleanCity_Mart_Frlg": (2, 2),
    "VermilionCity_Mart_Frlg": (2, 2), "LavenderTown_Mart_Frlg": (2, 2), "FuchsiaCity_Mart_Frlg": (2, 2),
    "SaffronCity_Mart_Frlg": (2, 2), "CinnabarIsland_Mart_Frlg": (2, 2), "ThreeIsland_Mart_Frlg": (2, 2),
    "FourIsland_Mart_Frlg": (2, 2), "SixIsland_Mart_Frlg": (2, 2), "SevenIsland_Mart_Frlg": (2, 2),
    "IndigoPlateau_PokemonCenter_1F_Frlg": (0, 6), "TrainerTower_Lobby_Frlg": (13, 9),
}

# ---------------------------------------------------------------------------
# 4) Herb shop (Lavender Town Mart): every herb-type item in the series.
# ---------------------------------------------------------------------------
HERB_ITEMS = """ENERGY_POWDER ENERGY_ROOT HEAL_POWDER REVIVAL_HERB REMEDY FINE_REMEDY SUPERB_REMEDY
WHITE_HERB MENTAL_HERB POWER_HERB MIRROR_HERB BIG_ROOT TINY_MUSHROOM BIG_MUSHROOM
LONELY_MINT ADAMANT_MINT NAUGHTY_MINT BRAVE_MINT BOLD_MINT IMPISH_MINT LAX_MINT RELAXED_MINT
MODEST_MINT MILD_MINT RASH_MINT QUIET_MINT CALM_MINT GENTLE_MINT CAREFUL_MINT SASSY_MINT
TIMID_MINT HASTY_MINT JOLLY_MINT NAIVE_MINT SERIOUS_MINT""".split()

# ---------------------------------------------------------------------------
# 5) Celadon Department Store: the strong stuff and "everything else".
# ---------------------------------------------------------------------------
CELADON_COMPETITIVE = """LEFTOVERS LIFE_ORB CHOICE_BAND CHOICE_SPECS CHOICE_SCARF FOCUS_SASH ASSAULT_VEST
ROCKY_HELMET EVIOLITE WEAKNESS_POLICY EXPERT_BELT LOADED_DICE CLEAR_AMULET COVERT_CLOAK
BOOSTER_ENTERGY_PLACEHOLDER LUCKY_EGG AMULET_COIN""".replace("BOOSTER_ENTERGY_PLACEHOLDER", "BOOSTER_ENERGY").split()

CELADON_FORMS = """GRACIDEA REVEAL_GLASS DNA_SPLICERS PRISON_BOTTLE N_SOLARIZER N_LUNARIZER REINS_OF_UNITY
ROTOM_CATALOG ZYGARDE_CUBE RED_ORB BLUE_ORB ADAMANT_ORB LUSTROUS_ORB GRISEOUS_ORB ADAMANT_CRYSTAL
LUSTROUS_GLOBE GRISEOUS_CORE RUSTED_SWORD RUSTED_SHIELD CORNERSTONE_MASK WELLSPRING_MASK HEARTHFLAME_MASK
RED_NECTAR YELLOW_NECTAR PINK_NECTAR PURPLE_NECTAR
FLAME_PLATE SPLASH_PLATE ZAP_PLATE MEADOW_PLATE ICICLE_PLATE FIST_PLATE TOXIC_PLATE EARTH_PLATE
SKY_PLATE MIND_PLATE INSECT_PLATE STONE_PLATE SPOOKY_PLATE DRACO_PLATE DREAD_PLATE IRON_PLATE PIXIE_PLATE
DOUSE_DRIVE SHOCK_DRIVE BURN_DRIVE CHILL_DRIVE
FIRE_MEMORY WATER_MEMORY ELECTRIC_MEMORY GRASS_MEMORY ICE_MEMORY FIGHTING_MEMORY POISON_MEMORY
GROUND_MEMORY FLYING_MEMORY PSYCHIC_MEMORY BUG_MEMORY ROCK_MEMORY GHOST_MEMORY DRAGON_MEMORY
DARK_MEMORY STEEL_MEMORY FAIRY_MEMORY
LIGHT_BALL LEEK THICK_CLUB LUCKY_PUNCH METAL_POWDER QUICK_POWDER SOUL_DEW""".split()

CELADON_TRAINING = """ABILITY_CAPSULE ABILITY_PATCH BOTTLE_CAP GOLD_BOTTLE_CAP RARE_CANDY PP_UP PP_MAX
SACRED_ASH MAX_HONEY X_SP_DEF""".split()

# Mega Stones / Z-Crystals are pulled from items.h automatically (holdEffect / section).

LOCAL_SPECIALTIES = """PEWTER_CRUNCHIES RAGE_CANDY_BAR LAVA_COOKIE OLD_GATEAU CASTELIACONE LUMIOSE_GALETTE
SHALOUR_SABLE BIG_MALASADA JUBILIFE_MUFFIN""".split()

# Berries sold on the roof (all except Enigma variants).
BERRY_EXCLUDE = {"ENIGMA_BERRY", "ENIGMA_BERRY_E_READER"}

# Price overrides for items that are free / unsellable in the base data.
PRICE_OVERRIDES = {
    "SCROLL_OF_DARKNESS": 20000, "SCROLL_OF_WATERS": 20000, "GIMMIGHOUL_COIN": 20,
    "GRACIDEA": 5000, "REVEAL_GLASS": 10000, "DNA_SPLICERS": 10000, "PRISON_BOTTLE": 10000,
    "N_SOLARIZER": 10000, "N_LUNARIZER": 10000, "REINS_OF_UNITY": 10000, "ROTOM_CATALOG": 5000,
    "ZYGARDE_CUBE": 10000, "RED_ORB": 20000, "BLUE_ORB": 20000, "ADAMANT_ORB": 10000,
    "LUSTROUS_ORB": 10000, "GRISEOUS_ORB": 10000, "ADAMANT_CRYSTAL": 20000, "LUSTROUS_GLOBE": 20000,
    "GRISEOUS_CORE": 20000, "RUSTED_SWORD": 20000, "RUSTED_SHIELD": 20000, "CORNERSTONE_MASK": 20000,
    "WELLSPRING_MASK": 20000, "HEARTHFLAME_MASK": 20000, "DOUSE_DRIVE": 1000, "SHOCK_DRIVE": 1000,
    "BURN_DRIVE": 1000, "CHILL_DRIVE": 1000, "SOUL_DEW": 20000, "BOOSTER_ENERGY": 30000,
    "LUCKY_EGG": 30000, "EVERSTONE": 3000, "LURE_BALL": 300, "LEVEL_BALL": 300, "MOON_BALL": 300,
    "FRIEND_BALL": 300, "LOVE_BALL": 300, "FAST_BALL": 300, "HEAVY_BALL": 300,
    "LEADERS_CREST": 3000, "ESCAPE_ROPE": 550, "MEGA_STONE_DEFAULT": 10000, "Z_CRYSTAL_DEFAULT": 10000,
}
