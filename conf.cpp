#include "conf.h"
#include <variant>
#include <cstdint>
#include <optional>
#include <cctype>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <cassert>
#include "util.h"

Config *g_config = nullptr;

void setup_config() {
    g_config = new Config();
}

#define CVT(_1) CONCAT(CVT_, _1)

enum ConfigValueType: uint8_t
{
#   define CONFIG_VALUE_TYPE(name, typ) CVT(name),
#   include "config_list.h"
#   undef CONFIG_VALUE_TYPE
    NUM_CVT
};

template<ConfigValueType CVT>
struct CVT2Type {};

#define CONFIG_VALUE_TYPE(name, typ) \
template<> \
struct CVT2Type<CVT(name)> { \
    typedef typ type; \
};
#include "config_list.h"
#undef CONFIG_VALUE_TYPE

template<ConfigValueType CVT>
using CVT2Type_t = typename CVT2Type<CVT>::type;

struct ConfigEntry
{
    std::string         m_key;
    std::string         m_dependent_key;

    uint8_t             m_value_type;
    bool                m_optional: 1;
    bool                m_has_dependent: 1;
    bool                m_has_min: 1;
    bool                m_min_inclusive: 1;
    bool                m_has_max: 1;
    bool                m_max_inclusive: 1;
    bool                m_is_set: 1;

    std::variant<
#       define CONFIG_VALUE_TYPE(name, typ) typ,
#       include "config_list.h"
#       undef CONFIG_VALUE_TYPE
        nullptr_t>      m_min,
                        m_max,
                        m_value;
};

template<typename T>
std::string config_to_string(T t) { return std::to_string(t); }

template<>
std::string config_to_string(bool t) { return t ? "true" : "false"; }

template<>
std::string config_to_string(std::string t) { return t; }

/* Some sanity checks on the static config entries */
#define DEFINE_CONFIG_ENTRY(...) \
    IF_EMPTY(CAR(__VA_ARGS__), \
        static_assert(false, "missing key on line " STRINGIFY(__LINE__) " in config_list.h");, \
    IF_EMPTY(CADR(__VA_ARGS__), \
        static_assert(false, "missing type_name in entry " STRINGIFY(CAR(__VA_ARGS__)));, \
    IF_EMPTY(CADDR(__VA_ARGS__), \
        static_assert(false, "missing is_optional in entry " STRINGIFY(CAR(__VA_ARGS__)));))) \
    IF_NONEMPTY(CADDDDR(__VA_ARGS__), IF_EMPTY(CAR(CDDDDDR(__VA_ARGS__)), \
        static_assert(false, "missing min value with min_inclusive specified in entry " STRINGIFY(CAR(__VA_ARGS__)));)) \
    IF_NONEMPTY(CADR(CDDDDDR(__VA_ARGS__)), IF_EMPTY(CADDR(CDDDDDR(__VA_ARGS__)), \
        static_assert(false, "missing max value with max_inclusive specified in entry " STRINGIFY(CAR(__VA_ARGS__)));))
#include "config_list.h"
#undef DEFINE_CONFIG_ENTRY

/* Definition of config entries */
static ConfigEntry EntryList[] = {
#   define DEFINE_CONFIG_ENTRY(...) DEFINE_ENTRY_LIST_ENTRY(__VA_ARGS__, )
#   define DEFINE_ENTRY_LIST_ENTRY(key, typ, is_optional, ...) \
    { \
        .m_key = STRINGIFY(key), \
        .m_dependent_key = \
            IF_BOOLEAN_LITERAL(is_optional, "", STRINGIFY(is_optional)), \
        .m_value_type = CVT(typ), \
        .m_optional = IF_BOOLEAN_LITERAL(is_optional, is_optional, false), \
        .m_has_dependent = IF_BOOLEAN_LITERAL(is_optional, false, true), \
        .m_has_min = IS_NONEMPTY(CADR(__VA_ARGS__)), \
        IF_NONEMPTY_COMMA(CADR(__VA_ARGS__), .m_min_inclusive = CADR(__VA_ARGS__))  \
        .m_has_max = IS_NONEMPTY(CADDDR(__VA_ARGS__)), \
        IF_NONEMPTY_COMMA(CADDDR(__VA_ARGS__), .m_max_inclusive = CADDDR(__VA_ARGS__)) \
        .m_is_set = NOT(IS_EMPTY(CAR(__VA_ARGS__))), \
        IF_NONEMPTY_COMMA(CADR(__VA_ARGS__), .m_min = (CVT2Type_t<CVT(typ)>) CADDR(__VA_ARGS__)) \
        IF_NONEMPTY_COMMA(CADDDR(__VA_ARGS__), .m_max = (CVT2Type_t<CVT(typ)>) CADDDDR(__VA_ARGS__)) \
        IF_NONEMPTY_COMMA(CAR(__VA_ARGS__), .m_value = (CVT2Type_t<CVT(typ)>) CAR(__VA_ARGS__)) \
    },

#   include "config_list.h"
#   undef DEFINE_CONFIG_ENTRY
};

static const std::string &ConfigEntry2Key(ConfigEntry *entry) {
    return entry->m_key;
}

Config::Config():
    m_entry_map(ConfigEntry2Key)
{
    for (size_t i = 0; i < sizeof(EntryList) / sizeof(EntryList[0]); ++i) {
        m_entry_map.insert(EntryList + i);
    }
}

std::optional<std::string>
Config::get(
    const std::string &key) const
{
    auto iter = m_entry_map.find(key);
    if (iter == m_entry_map.end())
    {
        return std::optional<std::string>();
    }
    
    ConfigEntry *entry = *iter;
    if (!entry->m_is_set)
    {
        return std::optional<std::string>();
    }

    switch (entry->m_value_type)
    {
#   define CONFIG_VALUE_TYPE(name, typ) \
    case CVT(name): \
        return config_to_string(std::get<typ>(entry->m_value));
#   include "config_list.h"
#   undef CONFIG_VALUE_TYPE
    }

    return std::optional<std::string>();
}

#define CONFIG_VALUE_TYPE(name, typ) \
std::optional<typ> \
Config::CONCAT(get_, name)( \
    const std::string &key) const \
{ \
    auto iter = m_entry_map.find(key); \
    if (iter == m_entry_map.end()) \
    { \
        return std::optional<typ>(); \
    } \
    \
    ConfigEntry *entry = *iter; \
    if (!entry->m_is_set) \
    { \
        return std::optional<typ>(); \
    } \
    \
    return std::optional<typ>(std::get<typ>(entry->m_value)); \
}
#include "config_list.h"
#undef CONFIG_VALUE_TYPE

template<typename T, typename = void>
struct config_parse_value_impl{};

template<typename T>
struct config_parse_value_impl<T, std::enable_if_t<std::is_arithmetic_v<T>>>
{
    static int
    parse(
        ConfigEntry *entry,
        std::string &&value)
    {
        T ret;
        std::istringstream in(value);
        in >> std::boolalpha;

        if (!(in >> ret))
        {
            entry->m_is_set = false;
            return 1; // mal-format
        }

        if (entry->m_has_min && (
            entry->m_min_inclusive ? std::get<T>(entry->m_min) > ret :
            std::get<T>(entry->m_min) >= ret))
        {
            return 2; // lower than min value
        }

        if (entry->m_has_max && (
            entry->m_max_inclusive ? std::get<T>(entry->m_max) < ret :
            std::get<T>(entry->m_max) <= ret))
        {
            return 3; // higher than max value
        }

        entry->m_value.emplace<T>(ret);

        return 0;
    }
};

template<>
struct config_parse_value_impl<std::string, void> {
    static int
    parse(
        ConfigEntry *entry,
        std::string &&value)
    {
        entry->m_value.emplace<std::string>(std::move(value));
        return 0;
    }
};

template<typename T>
int config_parse_value(
    ConfigEntry *entry,
    std::string &&value)
{
    
    return config_parse_value_impl<T>::parse(entry, std::forward<std::string>(value));
}

template<typename, typename = void>
struct config_get_valid_range_string_impl
{
    static std::string
    get(ConfigEntry *entry)
    {
        assert(false);
        return "";
    }
};

template<typename T>
struct config_get_valid_range_string_impl<T,
    std::enable_if_t<std::is_arithmetic_v<T>>> {
    static std::string
    get(ConfigEntry *entry)
    {
        std::string ret;
        if (entry->m_has_min)
        {
            ret.append(entry->m_min_inclusive ? "[" : "(")
               .append(std::to_string(std::get<T>(entry->m_min)));
        }
        else
        {
            ret += "(-oo";
        }

        ret += ", ";

        if (entry->m_has_max)
        {
            ret.append(std::to_string(std::get<T>(entry->m_max)))
               .append(entry->m_max_inclusive ? "]" : ")");
        }
        else
        {
            ret.append("+oo)");
        }

        return std::move(ret);
    }
};

template<typename T>
std::string
config_get_valid_range_string(
    ConfigEntry *entry)
{
    return config_get_valid_range_string_impl<T>::get(entry); 
}

bool
Config::parse_line(
    std::string &str,
    int lineno)
{
    std::string::size_type key_p1 = 0, key_p2, eq_p, value_p1, value_p2, scan_p;
    std::string key, value_str;

    while (key_p1 < str.length() && std::isspace(str[key_p1])) ++key_p1;
    if (key_p1 < str.length() && str[key_p1] == '#')
    {
        return true;
    }

    key_p2 = key_p1;
    while (key_p2 < str.length() && (
            std::isalnum(str[key_p2]) ||
            str[key_p2] == '_' ||
            str[key_p2] == '.')) ++key_p2;
    if (key_p2 == str.length())
    {
        m_help_str.append("[ERROR] Line ")
                  .append(std::to_string(lineno))
                  .append(": malformatted line\n");
        return false;
    }

    key = str.substr(key_p1, key_p2 - key_p1);
    auto iter = m_entry_map.find(key);
    if (iter == m_entry_map.end())
    {
        m_help_str.append("[WARN] Line ")
                  .append(std::to_string(lineno))
                  .append(": skipping unknown variable ")
                  .append(key)
                  .append("\n");
        return true;
    }
    ConfigEntry *entry = *iter;

    eq_p = key_p2;
    while (eq_p < str.length() && std::isspace(str[eq_p])) ++eq_p;
    if (eq_p == str.length() || str[eq_p] != '=')
    {
        m_help_str.append("[ERROR] Line ")
                  .append(std::to_string(lineno))
                  .append(": malformatted line\n");
        return false;
    }
    
    value_p1 = eq_p + 1;
    while (value_p1 < str.length() && std::isspace(str[value_p1])) ++value_p1;
    if (value_p1 == str.length())
    {
        if (entry->m_value_type == CVT_string)
        {
            entry->m_value.emplace<std::string>("");
            return true;
        }
        else
        {
            m_help_str.append("[ERROR] Line ")
                      .append(std::to_string(lineno))
                      .append(": missing value for ")
                      .append(key)
                      .append("\n");
            return false;
        }
    }
    
    if (str[value_p1] == '"')
    {
        ++value_p1;
        scan_p = value_p2 = value_p1;
        bool in_quote = true;
        while (scan_p < str.length())
        {
            if (str[scan_p] == '\\' &&
                value_p2 + 1 < str.length() &&
                str[value_p2 + 1] == '"')
            {
                str[value_p2++] = '"';
                scan_p += 2;
                continue;
            }

            if (str[scan_p] == '"')
            {
                ++scan_p;
                in_quote = false;
                break;        
            }

            str[value_p2++] = str[scan_p++];
        }

        if (in_quote)
        {
            m_help_str.append("[ERROR] Line ")
                      .append(std::to_string(lineno))
                      .append(": unclosed quoted value\n");
            return false;
        }
    }
    else
    {
        scan_p = value_p1;
        while (scan_p < str.length() && !std::isspace(str[scan_p]) && str[scan_p] != '#')
        {
            ++scan_p;
        }
        value_p2 = scan_p;
    }
    while (scan_p < str.length() && std::isspace(str[scan_p])) ++scan_p;
    if (scan_p < str.length() && str[scan_p] != '#')
    {
        m_help_str.append("[WARN] Line ")
                  .append(std::to_string(lineno))
                  .append(": trailing junks skipped\n");
        return true;
    }

    std::string value = str.substr(value_p1, value_p2 - value_p1);
    switch (entry->m_value_type)
    {
#   define CONFIG_VALUE_TYPE(name, typ) \
    case CVT(name): \
        switch (config_parse_value<typ>(entry, std::move(value))) { \
        case 0: /* OK */ \
            break; \
        case 1: /* mal-format */ \
            m_help_str.append("[ERROR] Line ") \
                      .append(std::to_string(lineno)) \
                      .append(": invalid value, expecting " STRINGIFY(name)) \
                      .append("\n"); \
            return false; \
        case 2: /* lower than min */ \
        case 3: /* higher than max */ \
            m_help_str.append("[ERROR] Line ") \
                      .append(std::to_string(lineno)) \
                      .append(": value out of range ") \
                      .append(config_get_valid_range_string<typ>(entry)) \
                      .append("\n"); \
            return false; \
        } \
        break;
#   include "config_list.h"
#   undef CONFIG_VALUE_TYPE
    }
    
    entry->m_is_set = true;
    return true;
}

bool
Config::parse_file(
    const std::string &file_name,
    const char **help_str)
{
    std::ifstream fin(file_name);
    if (!fin)
    {
        if (help_str)
        {
            m_help_str = "[ERROR] Unable to open config file ";
            m_help_str += file_name;
            m_help_str += "\n";
            *help_str = m_help_str.c_str();
        }
        return false;
    }
    
    m_help_str.clear();
    bool ok = true;
    int lineno = 0;
    std::string line;
    while (std::getline(fin, line))
    {
        ++lineno;
        if (line.empty()) continue;
        
        //m_help_str.append(std::to_string(lineno)).append(":")
        //    .append(line).append("\n");
        ok &= parse_line(line, lineno);
    }

    for (ConfigEntry *entry: m_entry_map)
    {
        if (!entry->m_is_set && (!entry->m_optional ||
            (entry->m_has_dependent &&
             std::get<bool>((*m_entry_map.find(entry->m_dependent_key))->m_value))))
        {
            m_help_str.append("[ERROR] missing required entry ")
                      .append(entry->m_key)
                      .append("\n");
            ok = false;
        }
    }

    
    if (!ok && help_str)
    {
        *help_str = m_help_str.c_str();
    }
    return ok;
}

