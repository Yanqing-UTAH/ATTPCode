// This file should not include a header guard.

#ifndef DEFINE_CONFIG_ENTRY
#define DEFINE_CONFIG_ENTRY(...)
#define HAS_DEFINE_CONFIG_ENTRY_STUB 1

// DEFINE_CONFIG_ENTRY(
//        key /* required */ ,
//        type_name, /* required, see below for CONFIG_VALUE_TYPE */
//        is_optional, /* required, true, false or a dependent key*/
//        default_value, /* optional */
//        min_inclusive, /* optional */
//        min_value, /* required if min_inclusive set */
//        max_inclusive, /* optional */
//        max_value /* rquired if max_inclusive set */ )
#endif

DEFINE_CONFIG_ENTRY(infile, string, false)
DEFINE_CONFIG_ENTRY(outfile, string, true)
DEFINE_CONFIG_ENTRY(test_name, string, false)
DEFINE_CONFIG_ENTRY(sampling.enabled, boolean, true, false)
DEFINE_CONFIG_ENTRY(sampling.sample_size, u32, sampling.enabled, , true, 1u)
DEFINE_CONFIG_ENTRY(sampling.seed, u32, true, 19950810u)
DEFINE_CONFIG_ENTRY(PCM_HH.enabled, boolean, true, false)
DEFINE_CONFIG_ENTRY(PCM_HH.log_universe_size, u32, PCM_HH.enabled, , true, 1u, true, 32u)
DEFINE_CONFIG_ENTRY(PCM_HH.epsilon, double, PCM_HH.enabled, , false, 0, false, 1)
DEFINE_CONFIG_ENTRY(PCM_HH.delta, double, PCM_HH.enabled, , false, 0, false, 1)
DEFINE_CONFIG_ENTRY(PCM_HH.Delta, double, PCM_HH.enabled, , false, 0)
DEFINE_CONFIG_ENTRY(EXACT_HH.enabled, boolean, true, false)

#ifdef HAS_DEFINE_CONFIG_ENTRY_STUB
#undef HAS_DEFINE_CONFIG_ENTRY_STUB
#undef DEFINE_CONFIG_ENTRY
#endif

#ifndef CONFIG_VALUE_TYPE
#define CONFIG_VALUE_TYPE(...)
#define HAS_CONFIG_VALUE_TYPE_STUB 1
// CONFIG_VALUE_TYPE(
//        type_name,
//        underlying_type)
#endif

// A new scalar type can be added by simply adding a CONFIG_VALUE_TYPE()
// declaration here without touching conf.cpp.
// However, Config::get_xxx() must be declared manually as those functions
// are not automatically declared in conf.h.
//
CONFIG_VALUE_TYPE(boolean, bool)
CONFIG_VALUE_TYPE(u32, uint32_t)
CONFIG_VALUE_TYPE(i64, int64_t)
CONFIG_VALUE_TYPE(double, double)
CONFIG_VALUE_TYPE(string, std::string)

#ifdef HAS_CONFIG_VALUE_TYPE_STUB
#undef HAS_CONFIG_VALUE_TYPE_STUB
#undef CONFIG_VALUE_TYPE
#endif
