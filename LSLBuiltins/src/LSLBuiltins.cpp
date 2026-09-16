#include "string.h"
#include <iostream>
#include <sstream>
#include <fstream>
#include <regex>
#include <iterator>
#include <list>
#include <unordered_map>
#include <vector>

#include "Luau/Common.h"
#include "Luau/LSLBuiltins.h"

#include "builtins_embedded.h"

using Luau::SLConstant;
using Luau::SLConstantType;

static const std::unordered_map<std::string, SLConstantType> sTypeNameToType = {
    {"void", SLConstantType::Void},
    {"integer", SLConstantType::Integer},
    {"float", SLConstantType::Float},
    {"string", SLConstantType::String},
    {"key", SLConstantType::Key},
    {"vector", SLConstantType::Vector},
    {"rotation", SLConstantType::Quaternion},
    {"list", SLConstantType::List},
};

static std::unordered_map<std::string, SLConstant> sSLConstants = {};

static const char* const kKnownEventNames[] = {
#define LUAU_LSL_EVENT_NAME(name, str) str,
    LUAU_LSL_EVENTS(LUAU_LSL_EVENT_NAME)
#undef LUAU_LSL_EVENT_NAME
};
static_assert(std::size(kKnownEventNames) == Luau::LSLEvent::KnownCount - 1);

// Starts out as the enum's events, replaced by whatever builtins.txt lists
static std::vector<std::string> sLSLEventNames(std::begin(kKnownEventNames), std::end(kKnownEventNames));

static std::unordered_map<std::string, int> index_event_names(const std::vector<std::string>& names)
{
    std::unordered_map<std::string, int> indices;
    for (size_t i = 0; i < names.size(); ++i)
        indices.emplace(names[i], (int)i + 1);
    return indices;
}

// Normally I don't sanction use of statics like this, but I don't give a flexi about plumbing
// this through correctly everywhere.
static std::unordered_map<std::string, int> sLSLEventIndices = index_event_names(sLSLEventNames);
// This holds the allocations for the strings in the SLConstant instances
// We use a list because we don't want the values moving around in memory like a vector would do.
static std::list<std::string> sSLConstantStrings = {};

static const std::regex UUID_REGEX = std::regex("^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$", std::regex::icase);


static SLConstantType str_to_type(const std::string& str)
{
    auto type_iter = sTypeNameToType.find(str);
    if (type_iter != sTypeNameToType.end())
        return type_iter->second;
    fprintf(stderr, "invalid type in builtins: %s\n", str.c_str());
    exit(EXIT_FAILURE);
    LUAU_UNREACHABLE();
}

// Helper to parse builtins from any input stream
static void parse_builtins(std::istream& stream, const char* source_name)
{
    std::vector<std::string> events;
    std::string line;
    while (std::getline(stream, line))
    {
        // ignore comment and blank lines
        if (!line.rfind("//", 0) || !line.rfind('\r', 0) || !line.rfind('\n', 0) || line.empty())
            continue;

        std::string line_type;
        std::istringstream iss(line);
        iss >> line_type;

        if (line_type == "event")
        {
            // `event name( args )`, and the file's order is the event's number
            std::string rest = line.substr(iss.tellg());
            size_t name_start = rest.find_first_not_of(" \t");
            size_t name_end = rest.find_first_of(" \t(", name_start);
            if (name_start == std::string::npos || name_end == std::string::npos)
            {
                fprintf(stderr, "error parsing %s: %s\n", source_name, line.c_str());
                exit(EXIT_FAILURE);
            }
            events.push_back(rest.substr(name_start, name_end - name_start));
            continue;
        }

        if (line_type != "const")
            continue;

        std::string ret_type, name, eq, value;
        iss >> ret_type >> name >> eq;
        if (eq != "=")
        {
            fprintf(stderr, "error parsing %s: %s\n", source_name, line.c_str());
            exit(EXIT_FAILURE);
        }

        // Nah, we don't want these defined.
        if (name == "TRUE" || name == "FALSE")
            continue;

        // Value is everything after the current stream position
        value = line.substr(iss.tellg());
        // Strip off any whitespace
        value = value.erase(value.find_last_not_of(" \n\r\t") + 1);
        value = value.erase(0, value.find_first_not_of(" \n\r\t"));

        SLConstantType const_type = str_to_type(ret_type);
        SLConstant const_item;
        const_item.type = const_type;

#define CONST_PARSE_FAIL() fprintf(stderr, "couldn't parse value for '%s'\n", name.c_str()); exit(EXIT_FAILURE)

#define CONST_SSCANF(num, fmt, ...) \
if (sscanf(value.c_str(), (fmt), __VA_ARGS__) != num) { \
CONST_PARSE_FAIL(); \
}; do { } while (0)

        switch (const_type)
        {
        case SLConstantType::Integer:
        {
            int32_t const_val;
            CONST_SSCANF(1, "%d", &const_val);
            if (const_val == 0)
            {
                // Might be a hex constant
                sscanf(value.c_str(), "0x%x", &const_val);
            }
            const_item.valueInteger = (double)const_val;
            break;
        }
        case SLConstantType::Float:
        {
            float const_val;
            CONST_SSCANF(1, "%f", &const_val);
            const_item.valueNumber = const_val;
            break;
        }
        case SLConstantType::Vector:
        {
            float x, y, z;
            CONST_SSCANF(3, "<%f, %f, %f>", &x, &y, &z);
            const_item.valueVector[0] = x;
            const_item.valueVector[1] = y;
            const_item.valueVector[2] = z;
            break;
        }
        case SLConstantType::Quaternion:
        {
            float x, y, z, w;
            CONST_SSCANF(4, "<%f, %f, %f, %f>", &x, &y, &z, &w);
            const_item.valueQuat[0] = x;
            const_item.valueQuat[1] = y;
            const_item.valueQuat[2] = z;
            const_item.valueQuat[3] = w;
            break;
        }
        case SLConstantType::String:
        case SLConstantType::Key:
        {
            if (value[0] != '"' || value[value.length() - 1] != '"')
            {
                CONST_PARSE_FAIL();
            }

            // parse the escape codes out
            std::stringstream const_ss;
            for (size_t i = 1; i < value.length() - 1; ++i)
            {
                if (value[i] == '\\')
                {
                    ++i;
                    if (value[i] == 'n')
                        const_ss << '\n';
                    else
                        const_ss << value[i];
                }
                else
                    const_ss << value[i];
            }

            auto const_val = const_ss.str();
            if (std::regex_match(const_val, UUID_REGEX))
            {
                // This is something the sim would treat as a UUID constant even though it's a string in LSL.
                const_item.type = SLConstantType::Key;
            }
            const auto &stored_str = sSLConstantStrings.emplace_back(const_val);
            const_item.stringLength = stored_str.length();
            const_item.valueString = stored_str.c_str();
            break;
        }
        default:
            continue;
        }
        sSLConstants[name] = const_item;
    }
#undef CONST_PARSE_FAIL
#undef CONST_SSCANF

    // The engine numbers events off this list, and assumes the ones it knows
    // sit where its enum says
    if (!events.empty() && !Luau::setLSLEventNames(events))
    {
        fprintf(stderr, "%s: events are not in the order LSLBuiltins.h expects\n", source_name);
        exit(EXIT_FAILURE);
    }
}

// Called once at startup, not thread-safe.
// Loads constants into the map for the constant folder to use.
// If builtins_file is nullptr, uses embedded builtins data.
void luauSL_init_global_builtins(const char* builtins_file)
{
    sSLConstants.clear();
    sSLConstantStrings.clear();

    if (builtins_file == nullptr)
    {
        // Use embedded builtins
        std::istringstream stream(EMBEDDED_BUILTINS);
        parse_builtins(stream, "<embedded>");
    }
    else
    {
        // Load from file
        std::ifstream file_stream(builtins_file);
        if (!file_stream)
        {
            fprintf(stderr, "couldn't open %s\n", builtins_file);
            exit(EXIT_FAILURE);
        }
        parse_builtins(file_stream, builtins_file);
    }
}

bool Luau::setLSLEventNames(const std::vector<std::string>& names)
{
    if (names.size() < std::size(kKnownEventNames))
        return false;
    for (size_t i = 0; i < std::size(kKnownEventNames); ++i)
    {
        if (names[i] != kKnownEventNames[i])
            return false;
    }
    sLSLEventNames = names;
    sLSLEventIndices = index_event_names(names);
    return true;
}

const std::vector<std::string>& Luau::getLSLEventNames()
{
    return sLSLEventNames;
}

int Luau::lslEventIndex(const char* name)
{
    if (name == nullptr)
        return 0;
    const auto& found = sLSLEventIndices.find(name);
    return found != sLSLEventIndices.end() ? found->second : 0;
}

const char* Luau::lslEventName(int index)
{
    if (index < 1 || (size_t)index > sLSLEventNames.size())
        return nullptr;
    return sLSLEventNames[index - 1].c_str();
}

const std::unordered_map<std::string, SLConstant>& Luau::luauSL_constants()
{
    return sSLConstants;
}

const SLConstant* Luau::luauSL_find_constant(const char* name)
{
    if (name == nullptr)
        return nullptr;
    const auto& const_iter = sSLConstants.find(name);
    return const_iter != sSLConstants.end() ? &const_iter->second : nullptr;
}
