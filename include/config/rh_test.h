// Test hook: normally empty. Test builds may define RH_TEST_MAP / RH_TEST_X / RH_TEST_Y / RH_TEST_BADGES /
// RH_TEST_EXTRA to start a quickstart game somewhere else. Ignored in RELEASE builds.
#define RH_TEST_MAP MAP_VIRIDIAN_FOREST
#define RH_TEST_X 41
#define RH_TEST_Y 45
#define RH_TEST_BADGES 2
#define RH_TEST_EXTRA RH_SelfTest()
void RH_SelfTest(void);
