#ifndef GUARD_CONFIG_CHANGES_H
#define GUARD_CONFIG_CHANGES_H

#include "constants/config_changes.h"
#include "config/battle.h"
#include "config/pokerus.h"
#include "config/ai.h"

#define UNPACK_CONFIG_STRUCT(_name, _field, _typeMaxValue, ...) INVOKE_WITH_(UNPACK_CONFIG_STRUCT_, _field, UNPACK_B(_typeMaxValue));
#define UNPACK_CONFIG_STRUCT_(_field, _type, ...) _type FIRST(__VA_OPT__(_field:BIT_SIZE(FIRST(__VA_ARGS__)),) _field)

struct ConfigChanges
{
    BATTLE_CONFIG_DEFINITIONS(UNPACK_CONFIG_STRUCT)
    POKEMON_CONFIG_DEFINITIONS(UNPACK_CONFIG_STRUCT)
    AI_CONFIG_DEFINITIONS(UNPACK_CONFIG_STRUCT)
    // Expands to:
    // u32 critChance:4;
    // u32 critMultiplier:4;
    // ...
};

#if TESTING
extern struct ConfigChanges *gConfigChangesTestOverride;
#define GET_CONFIG_VALUE(_field, _default) (gConfigChangesTestOverride == NULL ? (_default) : gConfigChangesTestOverride->_field)
#else
#define GET_CONFIG_VALUE(_field, _default) (_default)
#endif

// Romhack: generation-valued configs are capped to the "Battle Mechanics" generation picked on the randomizer screen.
u32 RH_MechanicsGenConfig(void);
#define RH_CONFIG_MAX_(_type, _max, ...) (_max)
#define RH_CONFIG_MAX(_typeMaxValue) INVOKE_WITH_(RH_CONFIG_MAX_, UNPACK_B(_typeMaxValue))
#define RH_CLAMP_GEN(_typeMaxValue, _value) ((RH_CONFIG_MAX(_typeMaxValue) == GEN_COUNT - 1) ? min((u32)(_value), RH_MechanicsGenConfig()) : (u32)(_value))
#define UNPACK_BATTLE_CONFIG_GETTER(_name, _field, _typeMaxValue, ...) static inline u32 GetConfig_##_name(void) { return RH_CLAMP_GEN(_typeMaxValue, GET_CONFIG_VALUE(_field, _name)); }
#define UNPACK_POKEMON_CONFIG_GETTER(_name, _field, _typeMaxValue, ...) static inline u32 GetConfig_##_name(void) { return RH_CLAMP_GEN(_typeMaxValue, GET_CONFIG_VALUE(_field, P_##_name)); }

BATTLE_CONFIG_DEFINITIONS(UNPACK_BATTLE_CONFIG_GETTER)
POKEMON_CONFIG_DEFINITIONS(UNPACK_POKEMON_CONFIG_GETTER)
AI_CONFIG_DEFINITIONS(UNPACK_BATTLE_CONFIG_GETTER)

#undef UNPACK_BATTLE_CONFIG_GETTER
#undef UNPACK_POKEMON_CONFIG_GETTER
#undef GET_CONFIG_VALUE

#define GetConfig(name) GetConfig_##name()

ARM_FUNC u32 GetConfigInternal(enum ConfigTag configTag);
void SetConfig(enum ConfigTag configTag, u32 value);

#if TESTING
void TestInitConfigData(void);
void TestFreeConfigData(void);
#endif

#endif // GUARD_CONFIG_CHANGES_H
