#ifndef CONF_H
#define CONF_H

#include <string>
#include <functional>
#include "hashtable.h"

struct ConfigEntry;

class Config
{
public:
    Config();

    bool
    is_list(
        const std::string &key) const;

    bool
    is_assigned(
        const std::string &key) const;
    
    /*
     * >=0: list length
     * -1: assigned but not a list
     * -2: key not found
     * -3: not assigned
     */
    int
    list_length(
        const std::string &key) const;
    
    std::optional<std::string>
    get(
        const std::string &key,
        int idx = -1) const;

    std::optional<bool>
    get_boolean(
        const std::string &key,
        int idx = -1) const;
    
    void
    set_boolean(
        const std::string &key,
        bool new_value,
        int idx = -1);

    std::optional<uint32_t>
    get_u32(
        const std::string &key,
        int idx = -1) const;

    void
    set_u32(
        const std::string &key,
        uint32_t new_value,
        int idx = -1);

    std::optional<int64_t>
    get_i64(
        const std::string &key,
        int idx = -1) const;

    void
    set_i64(
        const std::string &key,
        int64_t new_value,
        int idx = -1);

    std::optional<uint64_t>
    get_u64(
        const std::string &key,
        int idx = -1) const;

    void
    set_u64(
        const std::string &key,
        uint64_t new_value,
        int idx = -1);
    
    std::optional<double>
    get_double(
        const std::string &key,
        int idx = -1) const;

    void
    set_double(
        const std::string &key,
        double new_value,
        int idx = -1);

    void
    set_string(
        const std::string &key,
        std::string new_value,
        int idx = -1);
    
    bool
    parse_file(
        const std::string &file_name,
        const char **help_str);

private:
    bool
    parse_line(
        const std::string &str,
        int lineno);

    /* should not be used */
    std::optional<std::string>
    get_string(
        const std::string &key,
        int idx = -1) const;

    __HashSet<ConfigEntry*,
        std::function<const std::string&(ConfigEntry*)>>
                                        m_entry_map;

    std::string                         m_help_str;
};

void setup_config();

extern Config *g_config;

#endif // CONF_H

