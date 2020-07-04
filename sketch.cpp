#include "sketch.h"
#include "util.h"
#include "pcm.h"
#include "pams.h"
#include "sampling.h"
#include "heavyhitters.h"
#include "exact_query.h"
#include "pmmg.h"
#include "dummy_persistent_misra_gries.h"
#include "norm_sampling.h"
#include "fd.h"
#include "norm_sampling_wr.h"
#include <unordered_map>
#include <cassert>
#include <utility>
#include <memory>

char help_str_buffer[help_str_bufsize];

// The sketch enum literal is ST_XXX for a sketch named XXX
#define ST_LITERAL(_1) CONCAT(ST_, _1)

enum SKETCH_TYPES_ENUM: int {
    unused = ST_INVALID,
#   define DEFINE_SKETCH_TYPE(stname, ...) ST_LITERAL(stname),
#   include "sketch_list.h"
#   undef DEFINE_SKETCH_TYPE
    
    NUM_SKETCH_TYPES
};

static std::unordered_map<std::string, SKETCH_TYPE> stname2st;

#define DEFINE_SKETCH_TYPE(stname, ...) STRINGIFY(stname),
static const char *st2stname[NUM_SKETCH_TYPES] = {
#   include "sketch_list.h" 
};
#undef DEFINE_SKETCH_TYPE
const char *sketch_type_to_sketch_name(SKETCH_TYPE st)
{
    return st2stname[st];
}

#define DEFINE_SKETCH_TYPE(_1, _2, staltname) STRINGIFY(staltname),
static const char *st2staltname[NUM_SKETCH_TYPES] = {
#   include "sketch_list.h"
};
#undef DEFINE_SKETCH_TYPE
const char *sketch_type_to_altname(SKETCH_TYPE st)
{
    return st2staltname[st];
}

void setup_sketch_lib() {
#   define DEFINE_SKETCH_TYPE(stname, _2, staltname) \
        stname2st[STRINGIFY(stname)] = ST_LITERAL(stname); \
        stname2st[STRINGIFY(staltname)] = ST_LITERAL(stname);

#   include "sketch_list.h"
#   undef DEFINE_SKETCH_TYPE
}

SKETCH_TYPE sketch_name_to_sketch_type(const char *sketch_name) {
    auto iter = stname2st.find(sketch_name);
    if (iter == stname2st.end()) return ST_INVALID;
    return iter->second;
}

IPersistentSketch*
create_persistent_sketch(
    SKETCH_TYPE st,
    int &argi,
    int argc,
    char *argv[],
    const char **help_str)
{
    *help_str = nullptr;
    switch (st) {
#   define ST_REQUIRE_CREATE
#   define DEFINE_SKETCH_TYPE(stname, clsname, _3) \
    case ST_LITERAL(stname): \
        return static_cast<IPersistentSketch*>(clsname::create(argi, argc, argv, help_str));
#   include "sketch_list.h"
#   undef DEFINE_SKETCH_TYPE
#   undef ST_REQUIRE_CREATE
    default:
        return nullptr;
    }
    
    /* shouldn't really get here!! */
    assert(false);
    return nullptr;
}

std::vector<SKETCH_TYPE>
check_query_type(
    const char *query_type,
    const char **help_str)
{
    std::vector<SKETCH_TYPE> ret;
    std::vector<ResourceGuard<IPersistentSketch>> test_instances;
#       define DEFINE_SKETCH_TYPE(_1, clsname, _3) \
            test_instances.emplace_back(clsname::get_test_instance()),
#       include "sketch_list.h"
#       undef DEFINE_SKETCH_TYPE
    assert(test_instances.size() == NUM_SKETCH_TYPES);
    
    if (help_str) *help_str = nullptr;
    if (!strcmp(query_type, "point_interval"))
    {
        for (SKETCH_TYPE st = 0; st < NUM_SKETCH_TYPES; ++st)
        {
            IPersistentPointQueryable *ippq =
                dynamic_cast<IPersistentPointQueryable*>(test_instances[st].get());
            if (ippq && !std::isnan(ippq->estimate_point_in_interval("", 0, 1)))
            {
                ret.push_back(st);
            }
        }
    }
    else if (!strcmp(query_type, "point_att"))
    {
        for (SKETCH_TYPE st = 0; st < NUM_SKETCH_TYPES; ++st)
        {
            IPersistentPointQueryable *ippq =
                dynamic_cast<IPersistentPointQueryable*>(test_instances[st].get());
            if (ippq && !std::isnan(ippq->estimate_point_at_the_time("", 1)))
            {
                ret.push_back(st);
            }
        }
    }
    else if (!strcmp(query_type, "heavy_hitter"))
    {
        for (SKETCH_TYPE st = 0; st < NUM_SKETCH_TYPES; ++st)
        {
            IPersistentHeavyHitterSketch *iphh =
                dynamic_cast<IPersistentHeavyHitterSketch*>(test_instances[st].get());
            if (iphh)
            {
                ret.push_back(st);
            }
        }
    }
    else if (!strcmp(query_type, "heavy_hitter_bitp"))
    {
        for (SKETCH_TYPE st = 0; st < NUM_SKETCH_TYPES; ++st)
        {
            IPersistentHeavyHitterSketchBITP *ist =
                dynamic_cast<IPersistentHeavyHitterSketchBITP*>(
                    test_instances[st].get());
            if (ist)
            {
                ret.push_back(st);
            }
        }
    }
    else if (!strcmp(query_type, "matrix_sketch"))
    {
        for (SKETCH_TYPE st = 0; st < NUM_SKETCH_TYPES; ++st)
        {
            IPersistentMatrixSketch *ipms =
                dynamic_cast<IPersistentMatrixSketch*>(
                    test_instances[st].get());
            if (ipms)
            {
                ret.push_back(st);
            }
        }
    }
    else if (!strcmp(query_type, "frequency_estimation"))
    {
        for (SKETCH_TYPE st = 0; st < NUM_SKETCH_TYPES; ++st)
        {
            IPersistentFrequencyEstimationSketch *ipfes =
                dynamic_cast<IPersistentFrequencyEstimationSketch*>(
                    test_instances[st].get());
            if (ipfes)
            {
                ret.push_back(st);
            }
        }
    }
    else if (!strcmp(query_type, "frequency_estimation_bitp"))
    {
        for (SKETCH_TYPE st = 0; st < NUM_SKETCH_TYPES; ++st)
        {
            IPersistentFrequencyEstimationSketchBITP *ipfes =
                dynamic_cast<IPersistentFrequencyEstimationSketchBITP*>(
                    test_instances[st].get());
            if (ipfes)
            {
                ret.push_back(st);
            }
        }
    }
    else if (help_str)
    {
        snprintf(help_str_buffer, help_str_bufsize,
            "\n[ERROR] Unknown query type: %s\n", query_type);
        *help_str = help_str_buffer;
    }

    return std::move(ret);
}

std::vector<IPersistentSketch*>
create_persistent_sketch_from_config(
    SKETCH_TYPE st)
{
    std::vector<IPersistentSketch*> ret;
    int num_configs;
    switch (st)
    {
#   define DEFINE_SKETCH_TYPE(stname, clsname, _3) \
    case ST_LITERAL(stname): \
        num_configs = clsname::num_configs_defined(); \
        if (num_configs == -1) \
        { \
            ret.emplace_back(clsname::create_from_config(-1)); \
        } \
        else \
        { \
            for (int i = 0; i < num_configs; ++i) { \
                ret.emplace_back(clsname::create_from_config(i)); \
            } \
        } \
        break;
#   include "sketch_list.h"
#   undef DEFINE_SKETCH_TYPE
    }

    return std::move(ret);
}

