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
#endif

DEFINE_SKETCH_TYPE(PCM, PCMSketch, persistent_count_min)
DEFINE_SKETCH_TYPE(PAMS, PAMSketch, persistent_AMS_sketch)
DEFINE_SKETCH_TYPE(SAMPLING, SamplingSketch, persistent_sampling_sketch)
DEFINE_SKETCH_TYPE(PCM_HH, HeavyHitters, PCM_based_heavy_hitters)
DEFINE_SKETCH_TYPE(EXACT_HH, ExactHeavyHitters, exact_heavy_hitters)

