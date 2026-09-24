// Test hook: normally empty. Ignored in RELEASE builds.
#define RH_TEST_MAP MAP_FUCHSIA_CITY_SAFARI_ZONE_ENTRANCE
#define RH_TEST_X 4
#define RH_TEST_Y 5
#define RH_TEST_BADGES 5
#define RH_TEST_EXTRA do { VarSet(VAR_RH_SAFARI_GEN, 10); ScriptGiveMon(SPECIES_PIKACHU, 30, ITEM_NONE); } while (0)
