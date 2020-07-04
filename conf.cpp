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

enum ConfigValueType: uint16_t
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

#define CVT_LIST(_1) (CVT(_1) + NUM_CVT)
#define CVT2CVT_LIST(_1) (_1 + NUM_CVT)
#define CVT_LIST2CVT(_1) (_1 - NUM_CVT)

struct ConfigEntry
{
    std::string         m_key;
    std::string         m_dependent_key;

    ConfigValueType     m_value_type;
    bool                m_optional: 1;
    bool                m_can_be_list: 1;
    bool                m_has_dependent: 1;
    bool                m_has_min: 1;
    bool                m_min_inclusive: 1;
    bool                m_has_max: 1;
    bool                m_max_inclusive: 1;

    bool                m_is_assigned: 1;
    bool                m_is_list: 1;

    size_t              m_list_length,
                        m_list_capacity;
    
    std::variant<
#       define CONFIG_VALUE_TYPE(name, typ) typ,
#       include "config_list.h"
#       undef CONFIG_VALUE_TYPE
        nullptr_t>        m_min,
                        m_max;

    std::variant<
#       define CONFIG_VALUE_TYPE(name, typ) typ,
#       include "config_list.h"
#       undef CONFIG_VALUE_TYPE
#       define CONFIG_VALUE_TYPE(name, typ) typ*,
#       include "config_list.h"
#       undef CONFIG_VALUE_TYPE
        nullptr_t>      m_value;
};

template<typename T>
std::string config_to_string(T t) { return std::to_string(t); }

template<>
std::string config_to_string(bool t) { return t ? "true" : "false"; }

template<>
std::string config_to_string(std::string t) { return t; }

#define CENT_KEY(...) CAR(__VA_ARGS__)
#define CENT_TYP(...) CADR(__VA_ARGS__)
#define CENT_IS_OPTIONAL(...) CADDR(__VA_ARGS__)
#define CENT_CAN_BE_LIST(...) IF_NONEMPTY(CADDDR(__VA_ARGS__), CADDDR(__VA_ARGS__), false)
#define CENT_DEFAULT_VALUE(...) CADDDDR(__VA_ARGS__)
#define CENT_MIN_INCLUSIVE(...) CAR(CDDDDDR(__VA_ARGS__))
#define CENT_MIN_VALUE(...) CADR(CDDDDDR(__VA_ARGS__))
#define CENT_MAX_INCLUSIVE(...) CADDR(CDDDDDR(__VA_ARGS__))
#define CENT_MAX_VALUE(...) CADDDR(CDDDDDR(__VA_ARGS__))

/* Some sanity checks on the static config entries */
#define DEFINE_CONFIG_ENTRY(...) \
    IF_EMPTY(CENT_KEY(__VA_ARGS__), \
        static_assert(false, "missing key on line " STRINGIFY(__LINE__) " in config_list.h");, \
    IF_EMPTY(CENT_TYP(__VA_ARGS__), \
        static_assert(false, "missing type_name in entry " STRINGIFY(CENT_KEY(__VA_ARGS__)));, \
    IF_EMPTY(CENT_IS_OPTIONAL(__VA_ARGS__), \
        static_assert(false, "missing is_optional in entry " STRINGIFY(CENT_KEY(__VA_ARGS__)));))) \
    IF_NONEMPTY(CENT_MIN_INCLUSIVE(__VA_ARGS__), IF_EMPTY(CENT_MIN_VALUE(__VA_ARGS__), \
        static_assert(false, "missing min value with min_inclusive specified in entry " STRINGIFY(CENT_TYP(__VA_ARGS__)));)) \
    IF_NONEMPTY(CENT_MAX_INCLUSIVE(__VA_ARGS__), IF_EMPTY(CENT_MAX_VALUE(__VA_ARGS__), \
        static_assert(false, "missing max value with max_inclusive specified in entry " STRINGIFY(CENT_KEY(__VA_ARGS__)));))
#include "config_list.h"
#undef DEFINE_CONFIG_ENTRY

/* Definition of config entries */
static ConfigEntry EntryList[] = {
#   define DEFINE_CONFIG_ENTRY(...) \
    { \
        .m_key = STRINGIFY(CENT_KEY(__VA_ARGS__)), \
        .m_dependent_key = \
            IF_BOOLEAN_LITERAL(CENT_IS_OPTIONAL(__VA_ARGS__), "", STRINGIFY(CENT_IS_OPTIONAL(__VA_ARGS__))), \
        .m_value_type = CVT(CENT_TYP(__VA_ARGS__)), \
        .m_optional = IF_BOOLEAN_LITERAL(CENT_IS_OPTIONAL(__VA_ARGS__), CENT_IS_OPTIONAL(__VA_ARGS__), false), \
        .m_can_be_list = CENT_CAN_BE_LIST(__VA_ARGS__), \
        .m_has_dependent = IF_BOOLEAN_LITERAL(CENT_IS_OPTIONAL(__VA_ARGS__), false, true), \
        .m_has_min = IS_NONEMPTY(CENT_MIN_INCLUSIVE(__VA_ARGS__)), \
        IF_NONEMPTY_COMMA(CENT_MIN_INCLUSIVE(__VA_ARGS__), .m_min_inclusive = CENT_MIN_VALUE(__VA_ARGS__))  \
        .m_has_max = IS_NONEMPTY(CENT_MAX_INCLUSIVE(__VA_ARGS__)), \
        IF_NONEMPTY_COMMA(CENT_MAX_INCLUSIVE(__VA_ARGS__), .m_max_inclusive = CENT_MAX_VALUE(__VA_ARGS__)) \
        .m_is_assigned = NOT(IS_EMPTY(CENT_DEFAULT_VALUE(__VA_ARGS__))), \
        IF_NONEMPTY_COMMA(CENT_MIN_INCLUSIVE(__VA_ARGS__), .m_min = (CVT2Type_t<CVT(CENT_TYP(__VA_ARGS__))>) CENT_MIN_VALUE(__VA_ARGS__)) \
        IF_NONEMPTY_COMMA(CENT_MAX_INCLUSIVE(__VA_ARGS__), .m_max = (CVT2Type_t<CVT(CENT_TYP(__VA_ARGS__))>) CENT_MAX_VALUE(__VA_ARGS__)) \
        IF_NONEMPTY_COMMA(CENT_DEFAULT_VALUE(__VA_ARGS__), .m_value = (CVT2Type_t<CVT(CENT_TYP(__VA_ARGS__))>) CENT_DEFAULT_VALUE(__VA_ARGS__)) \
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

bool
Config::is_list(
    const std::string &key) const
{
    return list_length(key) >= 0;
}

bool
Config::is_assigned(
    const std::string &key) const
{
    return list_length(key) > -2;
}

int
Config::list_length(
    const std::string &key) const
{
    auto iter = m_entry_map.find(key);
    if (iter == m_entry_map.end()) return -2;
    
    ConfigEntry *entry = *iter;
    if (!entry->m_is_assigned) return -3;
    if (!entry->m_is_list) return -1;
    return (int) entry->m_list_length;
}

std::optional<std::string>
Config::get(
    const std::string &key,
    int idx) const
{
    auto iter = m_entry_map.find(key);
    if (iter == m_entry_map.end())
    {
        return std::optional<std::string>();
    }
    
    ConfigEntry *entry = *iter;
    if (!entry->m_is_assigned)
    {
        return std::optional<std::string>();
    }
    
    if (entry->m_is_list)
    {
        if (idx < 0 || (size_t) idx >= entry->m_list_length)
        {
            return std::optional<std::string>();
        }
    }
    else
    {
        if (idx != -1)
        {
            return std::optional<std::string>();
        }
    }

    switch (entry->m_value_type)
    {
#   define CONFIG_VALUE_TYPE(name, typ) \
    case CVT(name): \
        return entry->m_is_list ? \
            config_to_string(std::get<CVT_LIST(name)>(entry->m_value)[idx]) : \
            config_to_string(std::get<CVT(name)>(entry->m_value));
#   include "config_list.h"
#   undef CONFIG_VALUE_TYPE
    default:
        break;
    }

    return std::optional<std::string>();
}

#define CONFIG_VALUE_TYPE(name, typ) \
std::optional<typ> \
Config::CONCAT(get_, name)( \
    const std::string &key, \
    int idx) const \
{ \
    auto iter = m_entry_map.find(key); \
    if (iter == m_entry_map.end()) \
    { \
        return std::optional<typ>(); \
    } \
    \
    ConfigEntry *entry = *iter; \
    if (!entry->m_is_assigned) \
    { \
        return std::optional<typ>(); \
    } \
    \
    if (entry->m_is_list) \
    { \
        if (idx < 0 || (size_t) idx >= entry->m_list_length) \
        { \
            return std::optional<typ>(); \
        } \
    } \
    else \
    { \
        if (idx != -1) \
        { \
            return std::optional<typ>(); \
        } \
    } \
    \
    return entry->m_is_list ? \
        std::optional<typ>(std::get<CVT_LIST(name)>(entry->m_value)[idx]) : \
        std::optional<typ>(std::get<CVT(name)>(entry->m_value)); \
}
#include "config_list.h"
#undef CONFIG_VALUE_TYPE

#define CONFIG_VALUE_TYPE(name, typ) \
void \
Config::CONCAT(set_, name)( \
    const std::string &key, \
    typ new_value, \
    int idx) \
{ \
    auto iter = m_entry_map.find(key); \
    if (iter == m_entry_map.end()) \
    { \
        return ; \
    } \
    ConfigEntry *entry = *iter; \
    if (entry->m_is_list || idx != -1) \
    { \
        assert(false); /* list not supported */ \
    } \
    else \
    { \
        if (entry->m_has_min && ( \
            entry->m_min_inclusive ? \
            std::get<CVT(name)>(entry->m_min) > new_value : \
            std::get<CVT(name)>(entry->m_min) >= new_value)) \
        { \
            return ; \
        } \
        if (entry->m_has_max && ( \
            entry->m_max_inclusive ? \
            std::get<CVT(name)>(entry->m_max) < new_value : \
            std::get<CVT(name)>(entry->m_max) <= new_value)) \
        { \
            return ; \
        } \
        entry->m_is_assigned = true; \
        entry->m_value.emplace<CVT(name)>(new_value); \
    } \
}
#include "config_list.h"
#undef CONFIG_VALUE_TYPE

template<ConfigValueType CVT, typename = void>
struct config_parse_value_impl{};

template<ConfigValueType CVT>
struct config_parse_value_impl<CVT, std::enable_if_t<std::is_arithmetic_v<CVT2Type_t<CVT>>>>
{
    static int
    parse(
        ConfigEntry *entry,
        std::string &&value,
        int idx)
    {
        if (value.empty())
        {
            return 4; // missing value
        }

        CVT2Type_t<CVT> ret;
        std::istringstream in(value);
        in >> std::boolalpha;

        if (!(in >> ret))
        {
            return 1; // mal-format
        }

        if (entry->m_has_min && (
            entry->m_min_inclusive ? std::get<CVT>(entry->m_min) > ret :
            std::get<CVT>(entry->m_min) >= ret))
        {
            return 2; // lower than min value
        }

        if (entry->m_has_max && (
            entry->m_max_inclusive ? std::get<CVT>(entry->m_max) < ret :
            std::get<CVT>(entry->m_max) <= ret))
        {
            return 3; // higher than max value
        }

        if (idx == -1)
        {
            entry->m_value.emplace<CVT>(ret);
        }
        else
        {
            std::get<CVT2CVT_LIST(CVT)>(entry->m_value)[idx] = ret;
        }

        return 0;
    }
};

template<>
struct config_parse_value_impl<CVT_string, void> {
    static int
    parse(
        ConfigEntry *entry,
        std::string &&value,
        int idx)
    {
        if (idx == -1)
        {
            entry->m_value.emplace<std::string>(std::move(value));
        }
        else
        {
            std::get<CVT2CVT_LIST(CVT_string)>(entry->m_value)[idx].swap(value);
        }
        return 0;
    }
};

template<ConfigValueType CVT>
int config_parse_value(
    ConfigEntry *entry,
    std::string &&value,
    int idx)
{
    
    return config_parse_value_impl<CVT>::parse(
        entry, std::forward<std::string>(value), idx);
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

enum ParsingStatus {
    PS_INIT, // initial status
    PS_AFTER_KEY,
    PS_AFTER_EQ,
    PS_AFTER_VALUE,
    PS_IN_LIST_BEG,
    PS_IN_LIST_VALUE,
    PS_IN_LIST_AFTER_VALUE,
    PS_DONE,
    PS_ERROR
};

struct ParsingContext {
    ParsingStatus       m_status;
    
    ConfigEntry         *m_saved_entry;
};

static ParsingContext parsing_ctx;

static void
parsing_init_ctx()
{
    parsing_ctx.m_status = PS_DONE;
}

static void
parsing_skip_ws(
    const std::string &str,
    std::string::size_type &p0)
{
    while (p0 < str.length() && std::isspace(str[p0])) ++p0;
    if (p0 < str.length() && str[p0] == '#') p0 = str.length();
}

static std::string::size_type
parsing_parse_key(
    const std::string &str,
    std::string::size_type p0)
{
    while (p0 < str.length() && (
            std::isalnum(str[p0]) ||
            str[p0] == '_' ||
            str[p0] == '.')) ++p0;
    return p0;
}

static void
parsing_parse_value_string(
    int lineno,
    const std::string &str,
    std::string::size_type &p0,
    std::string &value_str,
    std::string &help_str)
{
    value_str.clear();
    if (p0 == str.length())
    {
        return ;
    }
    else
    {
        if (str[p0] == '"')
        {
            ++p0;
            bool in_quote = true;
            while (p0 < str.length())
            {
                if (str[p0] == '\\' &&
                    p0 + 1 < str.length() &&
                    str[p0 + 1] == '"')
                {
                    value_str.push_back('"');
                    continue;
                }

                if (str[p0] == '"')
                {
                    ++p0;
                    in_quote = false;
                    break;        
                }

                value_str.push_back(str[p0++]);
            }

            if (in_quote)
            {
                help_str.append("[ERROR] Line ")
                          .append(std::to_string(lineno))
                          .append(": unclosed quoted value\n");
                parsing_ctx.m_status = PS_ERROR;
            }
        } // if (str[p0] == '"')
        else if (str[p0] == '[')
        {
            if (parsing_ctx.m_status != PS_AFTER_EQ)
            {
                help_str.append("[ERROR] Line ")
                          .append(std::to_string(lineno))
                          .append(": already in list\n");
                parsing_ctx.m_status = PS_ERROR;
            }
            else
            {
                ++p0;
                parsing_ctx.m_status = PS_IN_LIST_BEG;
            }
        }
        else
        {
            auto p1 = p0;
            while (p1 < str.length() && !std::isspace(str[p1]) && str[p1] != '#' &&
                (parsing_ctx.m_status != PS_IN_LIST_VALUE || (
                    str[p1] != ']' && str[p1] != ','))) ++p1;
            value_str = str.substr(p0, p1 - p0);
            p0 = p1;
        }
    }
}

static void
clear_entry(ConfigEntry *entry)
{
    if (entry->m_is_list)
    {
        switch (entry->m_value_type)
        {
#   define CONFIG_VALUE_TYPE(name, typ) \
        case CVT(name): \
            delete[] std::get<(CVT_LIST(name))>(entry->m_value); \
            break; 
#   include "config_list.h"
#   undef CONFIG_VALUE_TYPE
        default: break;
        }
    }
    entry->m_is_assigned = false;
    entry->m_is_list = false;
}

static void
init_entry_list(ConfigEntry *entry)
{
    entry->m_list_length = 0;
    entry->m_list_capacity = 16;
    entry->m_is_list = true;
    switch (entry->m_value_type)
    {
#define CONFIG_VALUE_TYPE(name, typ) \
    case CVT(name): \
        entry->m_value.emplace<CVT_LIST(name)>(new typ[entry->m_list_capacity]); \
        break;
#include "config_list.h"
#undef CONFIG_VALUE_TYPE
    default: break;
    }
}

static void
double_entry_list_capacity(ConfigEntry *entry)
{
    entry->m_list_capacity = entry->m_list_capacity * 2;
    switch (entry->m_value_type)
    {
#define CONFIG_VALUE_TYPE(name, typ) \
    case CVT(name): \
    { \
        typ *new_arr = new typ[entry->m_list_capacity]; \
        typ *old_arr = std::get<CVT_LIST(name)>(entry->m_value); \
        std::copy(old_arr, old_arr + entry->m_list_length, new_arr); \
        delete [] old_arr; \
        entry->m_value.emplace<CVT_LIST(name)>(new_arr); \
    } \
        break;
#include "config_list.h"
#undef CONFIG_VALUE_TYPE
    default: break;
    }
}

bool
Config::parse_line(
    const std::string &str,
    int lineno)
{
    std::string::size_type p0 = 0;
    ConfigEntry *entry; 

    if (parsing_ctx.m_status == PS_ERROR || parsing_ctx.m_status == PS_DONE)
    {
        parsing_ctx.m_status = PS_INIT;
    }
    
    if (parsing_ctx.m_status >= PS_AFTER_KEY)
    {
        entry = parsing_ctx.m_saved_entry;
    }
    else
    {
        entry = nullptr;
    }

    do {
        parsing_skip_ws(str, p0);
        switch (parsing_ctx.m_status)
        {
        case PS_INIT:
        {
            if (p0 == str.length())
            {
                parsing_ctx.m_status = PS_DONE;
                break; // empty line
            }

            auto p1 = parsing_parse_key(str, p0); 
            std::string key = str.substr(p0, p1 - p0);
            auto iter = m_entry_map.find(key);
            if (iter == m_entry_map.end())
            {
                m_help_str.append("[WARN] Line ")
                          .append(std::to_string(lineno))
                          .append(": skipping unknown variable ")
                          .append(key)
                          .append("\n");
                parsing_ctx.m_status = PS_DONE;
                break;
            }

            entry = parsing_ctx.m_saved_entry = *iter;
            p0 = p1;
            parsing_ctx.m_status = PS_AFTER_KEY;
        }
            break;

        case PS_AFTER_KEY:
        {
            if (p0 == str.length() || str[p0] != '=')
            {
                m_help_str.append("[ERROR] Line ")
                          .append(std::to_string(lineno))
                          .append(": malformatted line\n");
                parsing_ctx.m_status = PS_ERROR;
                break;
            }
            ++p0;
            parsing_ctx.m_status = PS_AFTER_EQ;
        }
            break;

        case PS_AFTER_EQ:
        {
            std::string value_str;
            parsing_parse_value_string(lineno, str, p0, value_str, m_help_str);
            if (parsing_ctx.m_status == PS_AFTER_EQ)
            {
                if (entry->m_is_assigned)
                {
                    clear_entry(entry);
                }

                switch (entry->m_value_type)
                {
#           define CONFIG_VALUE_TYPE(name, typ) \
                case CVT(name): \
                    switch (config_parse_value<CVT(name)>(entry, std::move(value_str), -1)) { \
                    case 0: /* OK */ \
                        entry->m_is_assigned = true; \
                        parsing_ctx.m_status = PS_AFTER_VALUE; \
                        break; \
                    case 1: /* mal-format */ \
                        m_help_str.append("[ERROR] Line ") \
                                  .append(std::to_string(lineno)) \
                                  .append(": invalid value, expecting " STRINGIFY(name)) \
                                  .append("\n"); \
                        parsing_ctx.m_status = PS_ERROR; \
                        break; \
                    case 2: /* lower than min */ \
                    case 3: /* higher than max */ \
                        m_help_str.append("[ERROR] Line ") \
                                  .append(std::to_string(lineno)) \
                                  .append(": value out of range ") \
                                  .append(config_get_valid_range_string<typ>(entry)) \
                                  .append("\n"); \
                        parsing_ctx.m_status = PS_ERROR; \
                        break; \
                    case 4: /* missing value */ \
                        m_help_str.append("[ERROR] Line ") \
                                  .append(std::to_string(lineno)) \
                                  .append(": missing value for ") \
                                  .append(entry->m_key) \
                                  .append("\n"); \
                        parsing_ctx.m_status = PS_ERROR; \
                        break; \
                    } \
                    break;
#           include "config_list.h"
#           undef CONFIG_VALUE_TYPE
                default: break;
                }
            }
        }
            break;

        case PS_AFTER_VALUE:
        {
            if (p0 < str.length() && str[p0] != '#')
            {
                m_help_str.append("[WARN] Line ")
                          .append(std::to_string(lineno))
                          .append(": trailing junks skipped\n");
            }
            parsing_ctx.m_status = PS_DONE;
        }
            break;

        case PS_IN_LIST_BEG:
            if (entry->m_is_assigned && entry->m_is_list)
            {
                // we'll just reuse the list
                // assuming none of the config types require to be explicitly de'alloced
                entry->m_list_length = 0;
            }
            else
            {
                assert(!entry->m_is_list);
                init_entry_list(entry);
            }
            parsing_ctx.m_status = PS_IN_LIST_VALUE;
            break;

        case PS_IN_LIST_VALUE:
        {
            std::string value_str;
            if (p0 == str.length())
            {
                p0 = str.length();
                break;
            }
            if (str[p0] == ']')
            {
                entry->m_is_assigned = true;
                parsing_ctx.m_status = PS_AFTER_VALUE;
                ++p0;
                break;
            }

            parsing_parse_value_string(lineno, str, p0, value_str, m_help_str);
            if (parsing_ctx.m_status == PS_IN_LIST_VALUE)
            {
                if (entry->m_list_length == entry->m_list_capacity)
                {
                    double_entry_list_capacity(entry);
                }
                switch (entry->m_value_type)
                {
#           define CONFIG_VALUE_TYPE(name, typ) \
                case CVT(name): \
                    switch (config_parse_value<CVT(name)>(entry, std::move(value_str), \
                                entry->m_list_length)) { \
                    case 0: /* OK */ \
                        ++entry->m_list_length; \
                        parsing_ctx.m_status = PS_IN_LIST_AFTER_VALUE; \
                        break; \
                    case 1: /* mal-format */ \
                        m_help_str.append("[ERROR] Line ") \
                                  .append(std::to_string(lineno)) \
                                  .append(": invalid value, expecting " STRINGIFY(name)) \
                                  .append("\n"); \
                        parsing_ctx.m_status = PS_ERROR; \
                        clear_entry(entry); \
                        break; \
                    case 2: /* lower than min */ \
                    case 3: /* higher than max */ \
                        m_help_str.append("[ERROR] Line ") \
                                  .append(std::to_string(lineno)) \
                                  .append(": value out of range ") \
                                  .append(config_get_valid_range_string<typ>(entry)) \
                                  .append("\n"); \
                        parsing_ctx.m_status = PS_ERROR; \
                        clear_entry(entry); \
                        break; \
                    case 4: /* missing value */ \
                        m_help_str.append("[ERROR] Line ") \
                                  .append(std::to_string(lineno)) \
                                  .append(": empty value in the list for ") \
                                  .append(entry->m_key) \
                                  .append("\n"); \
                        parsing_ctx.m_status = PS_ERROR; \
                        break; \
                    } \
                    break;
#           include "config_list.h"
#           undef CONFIG_VALUE_TYPE
                default: break;
                }
            }
        }
            break;

        case PS_IN_LIST_AFTER_VALUE:
            if (p0 == str.length())
            {
                p0 = str.length();
                break;
            }
            if (str[p0] == ']')
            {
                entry->m_is_assigned = true;
                parsing_ctx.m_status = PS_AFTER_VALUE;
                ++p0;
                break;
            }
            if (str[p0] != ',')
            {
                m_help_str.append("[ERROR] Line ")
                          .append(std::to_string(lineno))
                          .append(": missing delimiter in list\n");
                parsing_ctx.m_status = PS_ERROR;
                break;
            }

            parsing_ctx.m_status = PS_IN_LIST_VALUE;
            ++p0;
            break;
        default: break;
        }
    } while (
        parsing_ctx.m_status != PS_DONE &&
        parsing_ctx.m_status != PS_ERROR &&
        ((parsing_ctx.m_status != PS_IN_LIST_VALUE &&
         parsing_ctx.m_status != PS_IN_LIST_AFTER_VALUE) ||
         p0 < str.length()));

    if (parsing_ctx.m_status == PS_ERROR)
    {
        return false;
    }
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
    
    parsing_init_ctx();
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
        if (!entry->m_is_assigned && (!entry->m_optional &&
            (!entry->m_has_dependent ||
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

