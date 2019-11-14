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
    
    std::optional<std::string>
    get(
        const std::string &key) const;

    std::optional<bool>
    get_boolean(
        const std::string &key) const;

    std::optional<uint32_t>
    get_u32(
        const std::string &key) const;

    std::optional<int64_t>
    get_i64(
        const std::string &key) const;
    
    std::optional<double>
    get_double(
        const std::string &key) const;
    
    bool
    parse_file(
        const std::string &file_name,
        const char **help_str);

private:
    __HashSet<ConfigEntry*,
        std::function<const std::string&(ConfigEntry*)>>
                                        m_entry_map;
    
    bool
    parse_line(
        std::string &str,
        int lineno);

    /* should not be used */
    std::optional<std::string>
    get_string(
        const std::string &key) const;

    std::string                         m_help_str;
};

void setup_config();

extern Config *g_config;

#endif // CONF_H

