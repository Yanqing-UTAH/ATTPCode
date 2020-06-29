// This file should not include a header guard.

#ifndef DEFINE_CONFIG_ENTRY
#define DEFINE_CONFIG_ENTRY(...)
#define HAS_DEFINE_CONFIG_ENTRY_STUB 1

// DEFINE_CONFIG_ENTRY(
//        key /* required */ ,
//        type_name, /* required, see below for CONFIG_VALUE_TYPE */
//        is_optional, /* required, true, false or a dependent key*/
//        can_be_list, /* optional, defaults to false */
//        default_value, /* optional */
//        min_inclusive, /* optional */
//        min_value, /* required if min_inclusive set */
//        max_inclusive, /* optional */
//        max_value /* rquired if max_inclusive set */ )
#endif

// Test configs
DEFINE_CONFIG_ENTRY(infile, string, false)
DEFINE_CONFIG_ENTRY(outfile, string, true)
DEFINE_CONFIG_ENTRY(out_limit, u64, true, false, 0) // 0 for unlimited
DEFINE_CONFIG_ENTRY(test_name, string, false)

// Test heavy hitters (ATTP/BITP)
// Valid values: "IP", "uint32"
DEFINE_CONFIG_ENTRY(HH.input_type, string, true, false, "IP")

// exact heavy hitter
DEFINE_CONFIG_ENTRY(EXACT_HH.enabled, boolean, true, false, false)

// uniform sampling sketch
DEFINE_CONFIG_ENTRY(SAMPLING.enabled, boolean, true, false, false)
DEFINE_CONFIG_ENTRY(SAMPLING.sample_size, u32, SAMPLING.enabled, true, , true, 1u)
DEFINE_CONFIG_ENTRY(SAMPLING.seed, u32, SAMPLING.enabled, false, 19950810u)

// uniform sampling sketch BITP
DEFINE_CONFIG_ENTRY(SAMPLING_BITP.enabled, boolean, true, false, false)
DEFINE_CONFIG_ENTRY(SAMPLING_BITP.sample_size, u32, SAMPLING_BITP.enabled, true, , true, 1u)
DEFINE_CONFIG_ENTRY(SAMPLING_BITP.seed, u32, true, false, 19950810u)
DEFINE_CONFIG_ENTRY(SAMPLING_BITP.use_new_impl, u32, SAMPLING_BITP.enabled, true, 2)

// persistent count-min
DEFINE_CONFIG_ENTRY(PCM_HH.enabled, boolean, true, false, false)
DEFINE_CONFIG_ENTRY(PCM_HH.log_universe_size, u32, PCM_HH.enabled, true, , true, 1u, true, 32u)
DEFINE_CONFIG_ENTRY(PCM_HH.epsilon, double, PCM_HH.enabled, true, , false, 0, false, 1)
DEFINE_CONFIG_ENTRY(PCM_HH.delta, double, PCM_HH.enabled, true, , false, 0, false, 1)
DEFINE_CONFIG_ENTRY(PCM_HH.Delta, double, PCM_HH.enabled, true, , false, 0)
DEFINE_CONFIG_ENTRY(PCM_HH.seed, u32, PCM_HH.enabled, false, 19950810u)

// Chain Misra Gries
DEFINE_CONFIG_ENTRY(CMG.enabled, boolean, true, false, false)
DEFINE_CONFIG_ENTRY(CMG.epsilon, double, CMG.enabled, true, , false, 0, false, 1)
DEFINE_CONFIG_ENTRY(CMG.use_update_new, boolean, CMG.enabled, true, true)

// Tree Misra Gries
DEFINE_CONFIG_ENTRY(TMG.enabled, boolean, true, false, false)
DEFINE_CONFIG_ENTRY(TMG.epsilon, double, TMG.enabled, true, , false, 0, false, 1)

// Don't use, for debugging only
DEFINE_CONFIG_ENTRY(DUMMY_PMG.enabled, boolean, true, false, false)
DEFINE_CONFIG_ENTRY(DUMMY_PMG.epsilon, double, DUMMY_PMG.enabled, true, , false, 0, false, 1)

// Tree Misra Gries (BITP)
DEFINE_CONFIG_ENTRY(TMG_BITP.enabled, boolean, true, false, false)
DEFINE_CONFIG_ENTRY(TMG_BITP.epsilon, double, TMG_BITP.enabled, true, , false, 0, false, 1)

// Persistent AMSketch (for frequency estimation)
DEFINE_CONFIG_ENTRY(PAMS.enabled, boolean, true, false, false)
DEFINE_CONFIG_ENTRY(PAMS.epsilon, double, PAMS.enabled, true, , false, 0, false, 1)
DEFINE_CONFIG_ENTRY(PAMS.delta, double, PAMS.enabled, true, , false, 0, false, 1)
DEFINE_CONFIG_ENTRY(PAMS.Delta, double, PAMS.enabled, true, , false, 0)
DEFINE_CONFIG_ENTRY(PAMS.seed, u32, true, false, 19950810u)

// Persistent Count-Min (for frequency estimation)
DEFINE_CONFIG_ENTRY(PCM.enabled, boolean, true, false, false)
DEFINE_CONFIG_ENTRY(PCM.epsilon, double, PCM.enabled, true, , false, 0, false, 1)
DEFINE_CONFIG_ENTRY(PCM.delta, double, PCM.enabled, true, , false, 0, false, 1)
DEFINE_CONFIG_ENTRY(PCM.Delta, double, PCM.enabled, true, , false, 0)
DEFINE_CONFIG_ENTRY(PCM.seed, u32, true, false, 19950810u)

// Test matrix sketch (ATTP)
DEFINE_CONFIG_ENTRY(MS.dimension, u32, true, false, , true, 1)
DEFINE_CONFIG_ENTRY(MS.use_analytic_error, boolean, true, false, false)
DEFINE_CONFIG_ENTRY(MS.ground_truth_file, string, MS.use_analytic_error)

// exact covariance matrix
DEFINE_CONFIG_ENTRY(EXACT_MS.enabled, boolean, true, false, false)

// norm sampling
DEFINE_CONFIG_ENTRY(NORM_SAMPLING.enabled, boolean, true, false, false)
DEFINE_CONFIG_ENTRY(NORM_SAMPLING.sample_size, u32, NORM_SAMPLING.enabled, true, , true, 1u)
DEFINE_CONFIG_ENTRY(NORM_SAMPLING.seed, u32, true, false, 19950810u)

// norm sampling w/ replacement
DEFINE_CONFIG_ENTRY(NORM_SAMPLING_WR.enabled, boolean, true, false, false)
DEFINE_CONFIG_ENTRY(NORM_SAMPLING_WR.sample_size, u32, NORM_SAMPLING_WR.enabled, true, , true, 1u)
DEFINE_CONFIG_ENTRY(NORM_SAMPLING_WR.seed, u32, true, false, 19950810u)

// ATTP FD
DEFINE_CONFIG_ENTRY(PFD.enabled, boolean, true, false, false)
DEFINE_CONFIG_ENTRY(PFD.half_sketch_size, u32, PFD.enabled, true, , true, 1)

// misc settings
DEFINE_CONFIG_ENTRY(perf.measure_time, boolean, true, false, false)
DEFINE_CONFIG_ENTRY(misc.suppress_progress_bar, boolean, true, false, false)
DEFINE_CONFIG_ENTRY(misc.fftw3.import_wisdom, boolean, true, false, true)
DEFINE_CONFIG_ENTRY(misc.fftw3.export_wisdom, boolean, true, false, true)
DEFINE_CONFIG_ENTRY(misc.fftw3.wisdom_filename, string, true, false, "driver.fftw3")

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
CONFIG_VALUE_TYPE(u64, uint64_t)
CONFIG_VALUE_TYPE(double, double)
CONFIG_VALUE_TYPE(string, std::string)

#ifdef HAS_CONFIG_VALUE_TYPE_STUB
#undef HAS_CONFIG_VALUE_TYPE_STUB
#undef CONFIG_VALUE_TYPE
#endif
