// This file should not include a header guard.  

#ifndef DEFINE_SKETCH_TYPE
#define DEFINE_SKETCH_TYPE(\
        sketch_name, \
        sketch_class_name, \
        alternative_sketch_name, \
        factory_func)
#endif

DEFINE_SKETCH_TYPE(PCM, PCMSketch, persistent_count_min, PCMSketch::create)
DEFINE_SKETCH_TYPE(PAMS, PAMSketch, persistent_AMS_sketch, PAMSketch::create)

