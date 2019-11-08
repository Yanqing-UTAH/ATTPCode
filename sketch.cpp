#include "sketch.h"
#include "util.h"
#include "pcm.h"
#include "pams.h"
#include "sampling.h"
#include "heavyhitters.h"
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

#define DEFINE_SKETCH_TYPE(_1, _2, staltname, ...) STRINGIFY(staltname),
static const char *st2staltname[NUM_SKETCH_TYPES] = {
#   include "sketch_list.h"
};
#undef DEFINE_SKETCH_TYPE
const char *sketch_type_to_altname(SKETCH_TYPE st)
{
    return st2staltname[st];
}

void setup_sketch_lib() {
#   define DEFINE_SKETCH_TYPE(stname, _2, staltname, ...) \
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
#   define DEFINE_SKETCH_TYPE(stname, _2, _3, create, ...) \
    case ST_LITERAL(stname): \
        return static_cast<IPersistentSketch*>(create(argi, argc, argv, help_str));
#   include "sketch_list.h"
#   undef DEFINE_SKETCH_TYPE
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
#       define DEFINE_SKETCH_TYPE(_1, _2, _3, _4, get_test_instance, ...) \
            test_instances.emplace_back(get_test_instance()),
#       include "sketch_list.h"
#       undef DEFINE_SKETCH_TYPE
    assert(test_instances.size() == NUM_SKETCH_TYPES);
    
    *help_str = nullptr;
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
    else
    {
        snprintf(help_str_buffer, help_str_bufsize,
            "\n[ERROR] Unknown query type: %s\n", query_type);
        *help_str = help_str_buffer;
    }

    return std::move(ret);
}

