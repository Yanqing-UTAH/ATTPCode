#ifndef SKETCH_LIB_H
#define SKETCH_LIB_H

void setup_sketch_lib();

typedef int SKETCH_TYPE;
#define ST_INVALID -1
SKETCH_TYPE sketch_name_to_sketch_type(const char *sketch_name);
const char *sketch_type_to_sketch_name(SKETCH_TYPE st);
const char *sketch_type_to_altname(SKETCH_TYPE st);

std::vector<SKETCH_TYPE>
check_query_type(
    const char *query_type,
    const char **help_str);

#endif // SKETCH_LIB_H

