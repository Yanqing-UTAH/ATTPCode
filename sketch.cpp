#include "sketch.h"
#include "util.h"
#include "pcm.h"
#include <unordered_map>
#include <cassert>

enum SKETCH_TYPES_ENUM: int {
    ST_PCM = 0,
    //ST_PAMS,
    
    NUM_SKETCH_TYPES
};

static std::unordered_map<std::string, SKETCH_TYPE> stname2st;

#define ST_LITERAL(_1) CONCAT(ST_, _1)

#define DEFINE_SKETCH_TYPE(stname, ...) STRINGIFY(stname),
static const char *st2stname[NUM_SKETCH_TYPES] = {
#   include "sketch_list.h" 
};
#undef DEFINE_SKETCH_TYPE

#define DEFINE_SKETCH_TYPE(_1, _2, staltname, ...) STRINGIFY(staltname),
static const char *st2staltname[NUM_SKETCH_TYPES] = {
#   include "sketch_list.h"
};
#undef DEFINE_SKETCH_TYPE

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

IPersistentPointQueryable*
create_persistent_point_queryable(
    SKETCH_TYPE st,
    int argc,
    char *argv[],
    const char **help_str) {

    switch (st) {
#   define DEFINE_SKETCH_TYPE(stname, _2, _3, create) \
    case ST_LITERAL(stname): \
        return static_cast<IPersistentPointQueryable*>(create(argc, argv, help_str));
#   include "sketch_list.h"
#   undef DEFINE_SKETCH_TYPE
    }
    
    /* shouldn't really get here!! */
    assert(false);
    return nullptr;
}

