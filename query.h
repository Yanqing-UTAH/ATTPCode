#ifndef QUERY_H
#define QUERY_H

// Now query.h is a very thing interface that driver depends on and
// does not change upon new queries being added.

#include <string>
#include <vector>

bool
is_supported_query_type(
    std::string query_type);

const std::vector<std::string>&
get_supported_query_type_list();

int
run_query(
    std::string query_type);

#endif // QUEYR_H

