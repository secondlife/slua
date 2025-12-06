#include "string.h"
#include <iostream>
#include <sstream>
#include <fstream>
#include <regex>
#include <list>
#include <unordered_map>

#include "Luau/Common.h"
#include "lua.h"
#include "llsl.h"
#include "luacode.h"

static const std::unordered_map<std::string, LSLIType> sTypeNameToType = {
    {"void", LSLIType::LST_NULL},
    {"integer", LSLIType::LST_INTEGER},
    {"float", LSLIType::LST_FLOATINGPOINT},
    {"string", LSLIType::LST_STRING},
    {"key", LSLIType::LST_KEY},
    {"vector", LSLIType::LST_VECTOR},
    {"rotation", LSLIType::LST_QUATERNION},
    {"list", LSLIType::LST_LIST},
};

struct SLConstant
{
    LSLIType type = LSLIType::LST_ERROR;
    size_t stringLength = 0;

    union
    {
        int32_t valueInteger;
        double valueNumber;
        float valueVector[3];
        float valueQuat[4];
        const char* valueString = nullptr; // length stored in stringLength
    };
};

static std::unordered_map<std::string, SLConstant> sSLConstants = {};
// This holds the allocations for the strings in the SLConstant instances
// We use a list because we don't want the values moving around in memory like a vector would do.
static std::list<std::string> sSLConstantStrings = {};

static const std::regex UUID_REGEX = std::regex("^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$", std::regex::icase);


LSLIType str_to_type(const std::string& str)
{
    auto type_iter = sTypeNameToType.find(str);
    if (type_iter != sTypeNameToType.end())
        return type_iter->second;
    fprintf(stderr, "invalid type in builtins: %s\n", str.c_str());
    exit(EXIT_FAILURE);
    LUAU_UNREACHABLE();
}

// Called once at startup, not thread-safe.
// Loads constants into the map for the constant folder to use
void luauSL_init_global_builtins(const char* builtins_file)
{
    sSLConstants.clear();
    sSLConstantStrings.clear();

    LUAU_ASSERT(builtins_file != nullptr);

    std::ifstream file_stream(builtins_file);
    if (!file_stream)
    {
        fprintf(stderr, "couldn't open %s\n", builtins_file);
        exit(EXIT_FAILURE);
    }

    std::string line;
    while (std::getline(file_stream, line))
    {
        // ignore comment and blank lines
        if (!line.rfind("//", 0) || !line.rfind('\r', 0) || !line.rfind('\n', 0) || line.empty())
            continue;

        std::string line_type;
        std::istringstream iss(line);
        iss >> line_type;

        if (line_type != "const")
            continue;

        std::string ret_type, name, eq, value;
        iss >> ret_type >> name >> eq;
        if (eq != "=")
        {
            fprintf(stderr, "error parsing %s: %s\n", builtins_file, line.c_str());
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

        LSLIType const_type = str_to_type(ret_type);
        SLConstant const_item;
        const_item.type = const_type;

#define CONST_PARSE_FAIL() fprintf(stderr, "couldn't parse value for '%s'\n", name.c_str()); exit(EXIT_FAILURE)

#define CONST_SSCANF(num, fmt, ...) \
if (sscanf(value.c_str(), (fmt), __VA_ARGS__) != num) { \
CONST_PARSE_FAIL(); \
}; do { } while (0)

        switch (const_type)
        {
        case LSLIType::LST_INTEGER:
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
        case LSLIType::LST_FLOATINGPOINT:
        {
            float const_val;
            CONST_SSCANF(1, "%f", &const_val);
            const_item.valueNumber = const_val;
            break;
        }
        case LSLIType::LST_VECTOR:
        {
            float x, y, z;
            CONST_SSCANF(3, "<%f, %f, %f>", &x, &y, &z);
            const_item.valueVector[0] = x;
            const_item.valueVector[1] = y;
            const_item.valueVector[2] = z;
            break;
        }
        case LSLIType::LST_QUATERNION:
        {
            float x, y, z, w;
            CONST_SSCANF(4, "<%f, %f, %f, %f>", &x, &y, &z, &w);
            const_item.valueQuat[0] = x;
            const_item.valueQuat[1] = y;
            const_item.valueQuat[2] = z;
            const_item.valueQuat[3] = w;
            break;
        }
        case LSLIType::LST_STRING:
        case LSLIType::LST_KEY:
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
                const_item.type = LSLIType::LST_KEY;
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
}

void luauSL_lookup_constant_cb(const char* library, const char* member, lua_CompileConstant* constant)
{
    // We only touch _globals_
    if (library != nullptr)
        return;

    const auto &const_iter = sSLConstants.find(member);
    if (const_iter != sSLConstants.end())
    {
        const auto &sl_constant = const_iter->second;
        switch (sl_constant.type)
        {
        case LSLIType::LST_STRING:
            luau_set_compile_constant_string(constant, sl_constant.valueString, sl_constant.stringLength);
            break;
        case LSLIType::LST_INTEGER:
            luau_set_compile_constant_number(constant, (double)sl_constant.valueInteger);
            break;
        case LSLIType::LST_FLOATINGPOINT:
            luau_set_compile_constant_number(constant, sl_constant.valueNumber);
            break;
        case LSLIType::LST_VECTOR:
        {
            const auto &vec = sl_constant.valueVector;
            luau_set_compile_constant_vector(constant, vec[0], vec[1], vec[2], 0.0f);
            break;
        }
        default:
            // Can't set these as compile-time constants.
            break;
        }
    }
}

void luaSL_set_constant_globals(lua_State *L)
{
    // Push constants onto _G.
    for (const auto &item : sSLConstants)
    {
        switch(item.second.type)
        {
        case LSLIType::LST_QUATERNION:
        {
            const auto &quat = item.second.valueQuat;
            luaSL_pushquaternion(L, quat[0], quat[1], quat[2], quat[3]);
            break;
        }
        case LSLIType::LST_KEY:
            luaSL_pushuuidlstring(L, item.second.valueString, item.second.stringLength);
            break;
        case LSLIType::LST_STRING:
            lua_pushlstring(L, item.second.valueString, item.second.stringLength);
            break;
        case LSLIType::LST_INTEGER:
            lua_pushnumber(L, (double)item.second.valueInteger);
            break;
        case LSLIType::LST_FLOATINGPOINT:
            // We specifically truncate to 32-bit precision
            lua_pushnumber(L, (float)item.second.valueNumber);
            break;
        case LSLIType::LST_VECTOR:
        {
            const auto &vec = item.second.valueVector;
            lua_pushvector(L, vec[0], vec[1], vec[2]);
            break;
        }
        default:
            continue;
        }
        lua_setglobal(L, item.first.c_str());
    }
}
