#include <cstdio>
#include <iostream>
#include <sstream>
#include <fstream>
#include <unordered_map>

#include "lslmini.hh"
#include "logger.hh"
#include "strings.hh"

#include "builtins_embedded.hh"

namespace Tailslide {

// Keep builtins alive as long as the library is loaded
static ScriptAllocator gStaticAllocator {};

// holds the symbols for the default builtins
LSLSymbolTable gBuiltinsSymbolTable{nullptr, SYMTAB_BUILTINS}; // NOLINT(cert-err58-cpp)

static const std::unordered_map<std::string, LSLIType> sTypeNameToType = {
    {"void", LST_NULL},
    {"integer", LST_INTEGER},
    {"float", LST_FLOATINGPOINT},
    {"string", LST_STRING},
    {"key", LST_KEY},
    {"vector", LST_VECTOR},
    {"rotation", LST_QUATERNION},
    {"list", LST_LIST},
};

static LSLType* str_to_type(const std::string& str)
{
    auto type_iter = sTypeNameToType.find(str);
    if (type_iter != sTypeNameToType.end())
        return LSLType::get(type_iter->second);
    // might be a type from a newer builtins file than we know about, skip the line.
    fprintf(stderr, "invalid type in builtins: %s\n", str.c_str());
    return nullptr;
}

static void set_default_value(LSLType* type, LSLConstant* constant)
{
    if (constant)
        constant->markStatic();
    type->setDefaultValue(constant);
}

static void init_default_values()
{
    set_default_value(TYPE(LST_INTEGER), gStaticAllocator.newTracked<LSLIntegerConstant>(0));
    set_default_value(TYPE(LST_FLOATINGPOINT), gStaticAllocator.newTracked<LSLFloatConstant>(0.0f));
    set_default_value(TYPE(LST_STRING), gStaticAllocator.newTracked<LSLStringConstant>(""));
    // Default value is _not_ NULL_KEY, even though you might logically think that.
    set_default_value(TYPE(LST_KEY), gStaticAllocator.newTracked<LSLKeyConstant>(""));
    set_default_value(TYPE(LST_VECTOR), gStaticAllocator.newTracked<LSLVectorConstant>(0.0f, 0.0f, 0.0f));
    set_default_value(TYPE(LST_QUATERNION), gStaticAllocator.newTracked<LSLQuaternionConstant>(0.0f, 0.0f, 0.0f, 1.0f));
    set_default_value(TYPE(LST_LIST), gStaticAllocator.newTracked<LSLListConstant>(nullptr));

    // These are used for de-sugaring the pre/post-inc/decrement operators
    auto* int_one = gStaticAllocator.newTracked<LSLIntegerConstant>(1);
    int_one->markStatic();
    TYPE(LST_INTEGER)->setOneValue(int_one);
    auto* float_one = gStaticAllocator.newTracked<LSLFloatConstant>(1.0f);
    float_one->markStatic();
    TYPE(LST_FLOATINGPOINT)->setOneValue(float_one);
}

// `name( type pname, type pname, ... )` from the rest of an event or function line.
static LSLFunctionDec* parse_signature(std::string rest, std::string& name)
{
    // Turn the punctuation into whitespace so the whole thing tokenizes with `>>`
    for (char& c : rest)
    {
        if (c == '(' || c == ')' || c == ',')
            c = ' ';
    }
    std::istringstream args(rest);
    args >> name;

    auto* dec = gStaticAllocator.newTracked<LSLFunctionDec>();
    std::string ptype, pname;
    while (args >> ptype >> pname)
    {
        LSLType* param_type = str_to_type(ptype);
        if (!param_type)
            return nullptr;
        dec->pushChild(gStaticAllocator.newTracked<LSLIdentifier>(
            param_type, gStaticAllocator.copyStr(pname.c_str())
        ));
    }
    return dec;
}

// `const type name = value`, with `iss` positioned just past the `const`.
// Returns false if the line couldn't be parsed and was skipped.
static bool parse_constant(std::istringstream& iss, const std::string& line, const char* source_name)
{
    std::string ret_type, name, eq, value;
    iss >> ret_type >> name >> eq;
    if (eq != "=")
    {
        fprintf(stderr, "error parsing %s, skipping: %s\n", source_name, line.c_str());
        return false;
    }

    // Value is everything after the current stream position
    value = line.substr(iss.tellg());
    // Strip off any whitespace
    value = value.erase(value.find_last_not_of(" \n\r\t") + 1);
    value = value.erase(0, value.find_first_not_of(" \n\r\t"));

    // key constants don't exist, and there are no key literals either.
    LSLType* const_type = str_to_type(ret_type == "key" ? "string" : ret_type);
    if (!const_type)
        return false;
    auto* sym = gStaticAllocator.newTracked<LSLSymbol>(
        gStaticAllocator.copyStr(name.c_str()), const_type, SYM_VARIABLE, SYM_BUILTIN
    );

#define CONST_PARSE_FAIL() fprintf(stderr, "couldn't parse value for '%s', skipping it\n", name.c_str()); return false

#define CONST_SSCANF(num, fmt, ...) \
if (sscanf(value.c_str(), (fmt), __VA_ARGS__) != num) { \
CONST_PARSE_FAIL(); \
}; do { } while (0)

    LSLConstant* const_built;
    switch (const_type->getIType())
    {
    case LST_INTEGER:
    {
        int const_val;
        CONST_SSCANF(1, "%d", &const_val);
        if (const_val == 0)
        {
            // Might be a hex constant
            sscanf(value.c_str(), "0x%x", &const_val);
        }
        const_built = gStaticAllocator.newTracked<LSLIntegerConstant>(const_val);
        break;
    }
    case LST_FLOATINGPOINT:
    {
        float const_val;
        CONST_SSCANF(1, "%f", &const_val);
        const_built = gStaticAllocator.newTracked<LSLFloatConstant>(const_val);
        break;
    }
    case LST_VECTOR:
    {
        float x, y, z;
        CONST_SSCANF(3, "<%f, %f, %f>", &x, &y, &z);
        const_built = gStaticAllocator.newTracked<LSLVectorConstant>(x, y, z);
        break;
    }
    case LST_QUATERNION:
    {
        float x, y, z, w;
        CONST_SSCANF(4, "<%f, %f, %f, %f>", &x, &y, &z, &w);
        const_built = gStaticAllocator.newTracked<LSLQuaternionConstant>(x, y, z, w);
        break;
    }
    case LST_STRING:
    case LST_KEY:
    {
        if (value[0] != '"' || value[value.length() - 1] != '"')
        {
            CONST_PARSE_FAIL();
        }

        // parse the escape codes out
        const_built = gStaticAllocator.newTracked<LSLStringConstant>(
            parse_string(&gStaticAllocator, value.data())
        );
        break;
    }
    default:
        return true;
    }
#undef CONST_PARSE_FAIL
#undef CONST_SSCANF

    const_built->markStatic();
    sym->setConstantValue(const_built);
    gBuiltinsSymbolTable.define(sym);
    return true;
}

// Helper to parse builtins from any input stream.
// Returns false if any line had to be skipped or a required builtin was missing.
static bool parse_builtins(std::istream& stream, const char* source_name)
{
    init_default_values();

    bool ok = true;
    // Events are numbered by their order in the file, that's the number the
    // handled-event bitfields use.
    int event_index = 0;
    std::string line;
    while (std::getline(stream, line))
    {
        // ignore comment and blank lines
        if (!line.rfind("//", 0) || !line.rfind('\r', 0) || !line.rfind('\n', 0) || line.empty())
            continue;

        std::string line_type;
        std::istringstream iss(line);
        iss >> line_type;

        if (line_type == "const")
        {
            if (!parse_constant(iss, line, source_name))
                ok = false;
            continue;
        }

        // `event name( args )` or `rettype name( args )`
        std::string name;
        auto* dec = parse_signature(line.substr(iss.tellg()), name);
        LSLType* ret_type = line_type == "event" ? TYPE(LST_NULL) : str_to_type(line_type);
        if (name.empty() || !dec || !ret_type)
        {
            fprintf(stderr, "error parsing %s, skipping: %s\n", source_name, line.c_str());
            ok = false;
            continue;
        }

        if (line_type == "event")
        {
            // the file's order is the event's number
            auto* event_sym = gStaticAllocator.newTracked<LSLSymbol>(
                gStaticAllocator.copyStr(name.c_str()), TYPE(LST_NULL), SYM_EVENT, SYM_BUILTIN, dec
            );
            event_sym->setEventIndex(++event_index);
            gBuiltinsSymbolTable.define(event_sym);
        }
        else
        {
            gBuiltinsSymbolTable.define(gStaticAllocator.newTracked<LSLSymbol>(
                gStaticAllocator.copyStr(name.c_str()), ret_type, SYM_FUNCTION, SYM_BUILTIN, dec
            ));
        }
    }

    // The language itself depends on these
    for (const char* name : {"TRUE", "FALSE"})
    {
        if (!gBuiltinsSymbolTable.lookup(name, SYM_VARIABLE))
        {
            fprintf(stderr, "%s: no definition for %s\n", source_name, name);
            ok = false;
        }
    }
    return ok;
}

// Called once at startup, not thread-safe.
// Loads builtins from a NUL-terminated buffer in builtins.txt format.
// Returns false if any definitions couldn't be loaded.
bool tailslide_init_builtins_from_data(const char* data)
{
    std::istringstream stream(data);
    return parse_builtins(stream, "<data>");
}

// Called once at startup, not thread-safe.
// If builtins_file is nullptr, uses embedded builtins data.
// Returns false if the file couldn't be opened or any definitions couldn't be loaded.
bool tailslide_init_builtins(const char* builtins_file)
{
    if (builtins_file == nullptr)
    {
        // Use embedded builtins
        return tailslide_init_builtins_from_data(EMBEDDED_BUILTINS);
    }

    // Load from file
    std::ifstream file_stream(builtins_file);
    if (!file_stream)
    {
        fprintf(stderr, "couldn't open %s\n", builtins_file);
        return false;
    }
    return parse_builtins(file_stream, builtins_file);
}

}
