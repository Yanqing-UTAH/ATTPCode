// This file should not include a header guard.  

#ifndef DEFINE_SKETCH_TYPE
#define DEFINE_SKETCH_TYPE(\
        sketch_name, \
        sketch_class_name, \
        alternative_sketch_name)

// factory_func for old impl. is
// SomeSketch::create(int &argi, int argc, char *argv[], const char **help_str)
// factory method for test instance is
// SomeSketch::get_test_instance()
// factory method for new impl. is
// SomeSketch::create_from_config(int idx = -1)
// method for determining the number of configs for a particular sketch is
// SomeSketch::num_configs_defined() // returns -1 if it's not a list
#endif

// exact queries need to be first if available
DEFINE_SKETCH_TYPE(EXACT_HH, ExactHeavyHitters, exact_heavy_hitters)

#ifndef ST_REQUIRE_CREATE
DEFINE_SKETCH_TYPE(EXACT_MS, ExactMatrix, exact_matrix)
#endif

// other sketches
DEFINE_SKETCH_TYPE(PCM, PCMSketch, persistent_count_min)
DEFINE_SKETCH_TYPE(PAMS, PAMSketch, persistent_AMS_sketch)
DEFINE_SKETCH_TYPE(SAMPLING, SamplingSketch, persistent_sampling_sketch)
DEFINE_SKETCH_TYPE(PCM_HH, HeavyHitters, PCM_based_heavy_hitters)

// new sketches will not have create factory method
#ifndef ST_REQUIRE_CREATE
DEFINE_SKETCH_TYPE(CMG, ChainMisraGries, chain_misra_gries)
DEFINE_SKETCH_TYPE(TMG, TreeMisraGries, tree_misra_gries)
DEFINE_SKETCH_TYPE(DUMMY_PMG, DummyPersistentMisraGries, dummy_persistent_misra_gries)
DEFINE_SKETCH_TYPE(TMG_BITP, TreeMisraGriesBITP, tree_misra_gries_bitp)
DEFINE_SKETCH_TYPE(SAMPLING_BITP, SamplingSketchBITP, persistent_sampling_sketch_bitp)

DEFINE_SKETCH_TYPE(NORM_SAMPLING, NormSamplingSketch, persistent_norm_sampling_sketch)
DEFINE_SKETCH_TYPE(PFD, FD_ATTP, persistent_frequent_direction)
DEFINE_SKETCH_TYPE(NORM_SAMPLING_WR, NormSamplingWRSketch, persistent_norm_sampling_with_replacement_sketch)
#endif

