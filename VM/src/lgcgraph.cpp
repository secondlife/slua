// TODO: this all probably isn't needed. Might be better
//  to just include obj color / fixed state in luaC_dump()?
//  Ah well! It's just for initial debugging!

#include <cstring>
#include <memory>
#include <string>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <unordered_map>
#include <vector>

#include "lgc.h"
#include "lgcgraph.h"

typedef struct Node
{
    // might not be `gco` in the udata case.
    void *node_id;
    GCObject* gco;
    uint8_t tt;
    uint8_t memcat;
    size_t size;
    std::shared_ptr<const char *> name;
    bool synthesized;
} Node;

typedef struct Edge
{
    GCObject* src;
    GCObject* dst;
    std::shared_ptr<const char *> name;
} Edge;

typedef struct EnumContext
{
    std::unordered_map<void*, Node> nodes = {};
    std::vector<Edge> edges = {};
} EnumContext;


static std::string gen_node_id(const Node &node)
{
    return "obj_" + std::to_string((size_t)node.node_id);
}

static std::string escape_byte_string(const char *val)
{
    std::stringstream esc_val;
    auto len = strlen(val);
    for (size_t i=0; i<len; ++i)
    {
        auto char_val = val[i];
        if (char_val == '\\')
        {
            esc_val << "\\\\";
        }
        else if (char_val == '"')
        {
            esc_val << "\\\"";
        }
        else if (char_val >= 0x20 && char_val <= 0x7F)
        {
            esc_val << char_val;
        }
        else
        {
            esc_val << "\\\\x";
            esc_val << std::setfill('0') << std::setw(2) << std::hex << (uint16_t)char_val;
        }
    }
    return esc_val.str();
}

CLANG_NOOPT static std::string GCC_NOOPT generate_node_attrs(const Node &node)
{
    std::stringstream ss;

    // Need to output ID as string due to ints not existing in JSON, they're really floats,
    // and our pointers are likely 64-bit.
    ss << "\"id\": \"" << (size_t)node.node_id << "\"";
    ss << ", \"type\": \"" << luaT_typenames[node.tt] << "\"";
    ss << ", \"name\": \"" << (*node.name ? escape_byte_string(*node.name) : gen_node_id(node)) << "\"";
    ss << ", \"fixed\": " << (isfixed(node.gco) ? "true" : "false");
    ss << ", \"synthesized\": " << (node.synthesized ? "true" : "false");

    const char *color = "unknown";
    if (isblack(node.gco))
        color = "black";
    else if (isgray(node.gco))
        color = "gray";
    else if (iswhite(node.gco))
        color = "white";

    ss << ", \"color\": \"" << color << "\"";
    ss << ", \"memcat\": " << (int)node.memcat;

    return ss.str();
}

static std::string generate_edge_attrs(const Edge &edge)
{
    std::stringstream ss;

    ss << "\"src\": \"" << (size_t)edge.src << "\"";
    ss << ", \"dst\": \"" << (size_t)edge.dst << "\"";
    if (edge.name)
        ss << ", \"name\": \"" << escape_byte_string(*edge.name) << "\"";
    else
        ss << ", \"name\": null";

    return ss.str();
}

void luaX_graphheap(lua_State *L, const char *out)
{
    EnumContext ctx;

    luaC_enumheap(
        L, &ctx,
        [](void* ctx, void* gco, uint8_t tt, uint8_t memcat, size_t size, const char* name) {
            EnumContext& context = *(EnumContext*)ctx;

            LUAU_ASSERT(gco);

            auto *casted_gco = ((GCObject *)gco);
            if (name == nullptr)
            {
                if (casted_gco->gch.tt == LUA_TSTRING)
                {
                    name = getstr(&casted_gco->ts);
                }
            }

            name = name ? strdup(name) : nullptr;
            context.nodes[gco] = {gco, casted_gco, tt, memcat, size, std::make_shared<const char*>(name), false};
        },
        [](void* ctx, void* src, void* dst, const char* edge_name) {
            EnumContext& context = *(EnumContext*)ctx;
            edge_name = edge_name ? strdup(edge_name) : nullptr;
            context.edges.push_back({(GCObject *)src, (GCObject *)dst, std::make_shared<const char*>(edge_name)});
        });

    // We may need to synthesize some "foreign" nodes that belong to other GCs.
    for (const auto &edge_iter : ctx.edges)
    {
        for (GCObject *point : {edge_iter.src, edge_iter.dst})
        {
            LUAU_ASSERT(point != nullptr);
            if (ctx.nodes.find((void *)point) != ctx.nodes.cend())
                continue;
            ctx.nodes[(void *)point] = {point, point, point->gch.tt, point->gch.memcat, 0, std::make_shared<const char *>(nullptr), true};
        }
    }

    std::ofstream ss {out, std::ios::out | std::ios::binary};
    LUAU_ASSERT(ss.is_open());

    ss << "{\n";
    ss << "  \"nodes\": [\n    ";
    bool first = true;
    for (const auto &node_iter : ctx.nodes)
    {
        if (!first)
            ss << ",\n    ";
        ss << "{" << generate_node_attrs(node_iter.second) << "}";
        first = false;
    }
    ss << "\n  ],\n";

    ss << "  \"edges\": [\n    ";
    first = true;
    for (const auto &edge : ctx.edges)
    {
        if (!first)
            ss << ",\n    ";
        ss << "{" << generate_edge_attrs(edge) << "}";
        first = false;
    }
    ss << "\n  ]\n";

    ss << "}\n";

    ss.close();
}
